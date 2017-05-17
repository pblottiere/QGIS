/***************************************************************************
                          qgsproject.cpp -  description
                             -------------------
  begin                : July 23, 2004
  copyright            : (C) 2004 by Mark Coletti
  email                : mcoletti at gmail.com
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsproject.h"

#include "qgsdatasourceuri.h"
#include "qgsexception.h"
#include "qgslayertree.h"
#include "qgslayertreeutils.h"
#include "qgslayertreeregistrybridge.h"
#include "qgslogger.h"
#include "qgsmessagelog.h"
#include "qgspluginlayer.h"
#include "qgspluginlayerregistry.h"
#include "qgsprojectfiletransform.h"
#include "qgssnappingconfig.h"
#include "qgspathresolver.h"
#include "qgsprojectversion.h"
#include "qgsrasterlayer.h"
#include "qgsrectangle.h"
#include "qgsrelationmanager.h"
#include "qgsannotationmanager.h"
#include "qgsvectorlayer.h"
#include "qgsvectorlayerjoininfo.h"
#include "qgsmapthemecollection.h"
#include "qgslayerdefinition.h"
#include "qgsunittypes.h"
#include "qgstransaction.h"
#include "qgstransactiongroup.h"
#include "qgsvectordataprovider.h"
#include "qgsprojectbadlayerhandler.h"
#include "qgssettings.h"
#include "qgsmaplayerlistutils.h"
#include "qgslayoutmanager.h"
#include "qgsziputils.h"

#include <QApplication>
#include <QFileInfo>
#include <QDomNode>
#include <QObject>
#include <QTextStream>
#include <QTemporaryFile>
#include <QDir>
#include <QUrl>

#ifdef Q_OS_UNIX
#include <utime.h>
#elif _MSC_VER
#include <sys/utime.h>
#endif

// canonical project instance
QgsProject *QgsProject::sProject = nullptr;

/**
    Take the given scope and key and convert them to a string list of key
    tokens that will be used to navigate through a Property hierarchy

    E.g., scope "someplugin" and key "/foo/bar/baz" will become a string list
    of { "properties", "someplugin", "foo", "bar", "baz" }.  "properties" is
    always first because that's the permanent ``root'' Property node.
 */
QStringList makeKeyTokens_( const QString &scope, const QString &key )
{
  QStringList keyTokens = QStringList( scope );
  keyTokens += key.split( '/', QString::SkipEmptyParts );

  // be sure to include the canonical root node
  keyTokens.push_front( QStringLiteral( "properties" ) );

  //check validy of keys since an unvalid xml name will will be dropped upon saving the xml file. If not valid, we print a message to the console.
  for ( int i = 0; i < keyTokens.size(); ++i )
  {
    QString keyToken = keyTokens.at( i );

    //invalid chars in XML are found at http://www.w3.org/TR/REC-xml/#NT-NameChar
    //note : it seems \x10000-\xEFFFF is valid, but it when added to the regexp, a lot of unwanted characters remain
    QString nameCharRegexp = QStringLiteral( "[^:A-Z_a-z\\xC0-\\xD6\\xD8-\\xF6\\xF8-\\x2FF\\x370-\\x37D\\x37F-\\x1FFF\\x200C-\\x200D\\x2070-\\x218F\\x2C00-\\x2FEF\\x3001-\\xD7FF\\xF900-\\xFDCF\\xFDF0-\\xFFFD\\-\\.0-9\\xB7\\x0300-\\x036F\\x203F-\\x2040]" );
    QString nameStartCharRegexp = QStringLiteral( "^[^:A-Z_a-z\\xC0-\\xD6\\xD8-\\xF6\\xF8-\\x2FF\\x370-\\x37D\\x37F-\\x1FFF\\x200C-\\x200D\\x2070-\\x218F\\x2C00-\\x2FEF\\x3001-\\xD7FF\\xF900-\\xFDCF\\xFDF0-\\xFFFD]" );

    if ( keyToken.contains( QRegExp( nameCharRegexp ) ) || keyToken.contains( QRegExp( nameStartCharRegexp ) ) )
    {

      QString errorString = QObject::tr( "Entry token invalid : '%1'. The token will not be saved to file." ).arg( keyToken );
      QgsMessageLog::logMessage( errorString, QString::null, QgsMessageLog::CRITICAL );

    }

  }

  return keyTokens;
}



/**
   return the property that matches the given key sequence, if any

   @param scope scope of key
   @param key keyname
   @param rootProperty is likely to be the top level QgsProjectPropertyKey in QgsProject:e:Imp.

   @return null if not found, otherwise located Property
*/
QgsProjectProperty *findKey_( const QString &scope,
                              const QString &key,
                              QgsProjectPropertyKey &rootProperty )
{
  QgsProjectPropertyKey *currentProperty = &rootProperty;
  QgsProjectProperty *nextProperty;           // link to next property down hierarchy

  QStringList keySequence = makeKeyTokens_( scope, key );

  while ( !keySequence.isEmpty() )
  {
    // if the current head of the sequence list matches the property name,
    // then traverse down the property hierarchy
    if ( keySequence.first() == currentProperty->name() )
    {
      // remove front key since we're traversing down a level
      keySequence.pop_front();

      if ( 1 == keySequence.count() )
      {
        // if we have only one key name left, then return the key found
        return currentProperty->find( keySequence.front() );
      }
      else if ( keySequence.isEmpty() )
      {
        // if we're out of keys then the current property is the one we
        // want; i.e., we're in the rate case of being at the top-most
        // property node
        return currentProperty;
      }
      else if ( ( nextProperty = currentProperty->find( keySequence.first() ) ) )
      {
        if ( nextProperty->isKey() )
        {
          currentProperty = static_cast<QgsProjectPropertyKey *>( nextProperty );
        }
        else if ( nextProperty->isValue() && 1 == keySequence.count() )
        {
          // it may be that this may be one of several property value
          // nodes keyed by QDict string; if this is the last remaining
          // key token and the next property is a value node, then
          // that's the situation, so return the currentProperty
          return currentProperty;
        }
        else
        {
          // QgsProjectPropertyValue not Key, so return null
          return nullptr;
        }
      }
      else
      {
        // if the next key down isn't found
        // then the overall key sequence doesn't exist
        return nullptr;
      }
    }
    else
    {
      return nullptr;
    }
  }

  return nullptr;
}



/** Add the given key and value

@param scope scope of key
@param key key name
@param rootProperty is the property from which to start adding
@param value the value associated with the key
*/
QgsProjectProperty *addKey_( const QString &scope,
                             const QString &key,
                             QgsProjectPropertyKey *rootProperty,
                             const QVariant &value )
{
  QStringList keySequence = makeKeyTokens_( scope, key );

  // cursor through property key/value hierarchy
  QgsProjectPropertyKey *currentProperty = rootProperty;
  QgsProjectProperty *nextProperty; // link to next property down hierarchy
  QgsProjectPropertyKey *newPropertyKey = nullptr;

  while ( ! keySequence.isEmpty() )
  {
    // if the current head of the sequence list matches the property name,
    // then traverse down the property hierarchy
    if ( keySequence.first() == currentProperty->name() )
    {
      // remove front key since we're traversing down a level
      keySequence.pop_front();

      // if key sequence has one last element, then we use that as the
      // name to store the value
      if ( 1 == keySequence.count() )
      {
        currentProperty->setValue( keySequence.front(), value );
        return currentProperty;
      }
      // we're at the top element if popping the keySequence element
      // will leave it empty; in that case, just add the key
      else if ( keySequence.isEmpty() )
      {
        currentProperty->setValue( value );

        return currentProperty;
      }
      else if ( ( nextProperty = currentProperty->find( keySequence.first() ) ) )
      {
        currentProperty = dynamic_cast<QgsProjectPropertyKey *>( nextProperty );

        if ( currentProperty )
        {
          continue;
        }
        else            // QgsProjectPropertyValue not Key, so return null
        {
          return nullptr;
        }
      }
      else                // the next subkey doesn't exist, so add it
      {
        if ( ( newPropertyKey = currentProperty->addKey( keySequence.first() ) ) )
        {
          currentProperty = newPropertyKey;
        }
        continue;
      }
    }
    else
    {
      return nullptr;
    }
  }

  return nullptr;

}


void removeKey_( const QString &scope,
                 const QString &key,
                 QgsProjectPropertyKey &rootProperty )
{
  QgsProjectPropertyKey *currentProperty = &rootProperty;

  QgsProjectProperty *nextProperty = nullptr;   // link to next property down hierarchy
  QgsProjectPropertyKey *previousQgsPropertyKey = nullptr; // link to previous property up hierarchy

  QStringList keySequence = makeKeyTokens_( scope, key );

  while ( ! keySequence.isEmpty() )
  {
    // if the current head of the sequence list matches the property name,
    // then traverse down the property hierarchy
    if ( keySequence.first() == currentProperty->name() )
    {
      // remove front key since we're traversing down a level
      keySequence.pop_front();

      // if we have only one key name left, then try to remove the key
      // with that name
      if ( 1 == keySequence.count() )
      {
        currentProperty->removeKey( keySequence.front() );
      }
      // if we're out of keys then the current property is the one we
      // want to remove, but we can't delete it directly; we need to
      // delete it from the parent property key container
      else if ( keySequence.isEmpty() )
      {
        previousQgsPropertyKey->removeKey( currentProperty->name() );
      }
      else if ( ( nextProperty = currentProperty->find( keySequence.first() ) ) )
      {
        previousQgsPropertyKey = currentProperty;
        currentProperty = dynamic_cast<QgsProjectPropertyKey *>( nextProperty );

        if ( currentProperty )
        {
          continue;
        }
        else            // QgsProjectPropertyValue not Key, so return null
        {
          return;
        }
      }
      else                // if the next key down isn't found
      {
        // then the overall key sequence doesn't exist
        return;
      }
    }
    else
    {
      return;
    }
  }

}

QgsProject::QgsProject( QObject *parent )
  : QObject( parent )
  , mBadLayerHandler( new QgsProjectBadLayerHandler() )
  , mSnappingConfig( this )
  , mRelationManager( new QgsRelationManager( this ) )
  , mAnnotationManager( new QgsAnnotationManager( this ) )
  , mLayoutManager( new QgsLayoutManager( this ) )
  , mRootGroup( new QgsLayerTree )
  , mAutoTransaction( false )
  , mEvaluateDefaultValues( false )
  , mDirty( false )
{
  mProperties.setName( QStringLiteral( "properties" ) );
  clear();

  // bind the layer tree to the map layer registry.
  // whenever layers are added to or removed from the registry,
  // layer tree will be updated
  mLayerTreeRegistryBridge = new QgsLayerTreeRegistryBridge( mRootGroup, this, this );
  connect( this, &QgsProject::layersAdded, this, &QgsProject::onMapLayersAdded );
  connect( this, &QgsProject::layersRemoved, this, [ = ] { cleanTransactionGroups(); } );
  connect( this, static_cast < void ( QgsProject::* )( const QList<QgsMapLayer *> & ) >( &QgsProject::layersWillBeRemoved ), this, &QgsProject::onMapLayersRemoved );
}


QgsProject::~QgsProject()
{
  delete mBadLayerHandler;
  delete mRelationManager;
  delete mLayerTreeRegistryBridge;
  delete mRootGroup;

  removeAllMapLayers();
}


QgsProject *QgsProject::instance()
{
  if ( !sProject )
  {
    sProject = new QgsProject;
  }
  return sProject;
}

void QgsProject::setTitle( const QString &title )
{
  if ( title == mTitle )
    return;

  mTitle = title;

  setDirty( true );
}


QString QgsProject::title() const
{
  return mTitle;
}


bool QgsProject::isDirty() const
{
  return mDirty;
}

void QgsProject::setDirty( bool b )
{
  mDirty = b;
}

bool QgsProject::unzip( const QString &filename )
{
  clearError();
  clearZip();

  // extract the archive in a temporary directory and returns all filenames
  QTemporaryDir tmpDir;
  tmpDir.setAutoRemove( false );
  QStringList files = QgsZipUtils::unzip( filename, tmpDir.path() );

  // check extracted files to retrieve the qgs project filename
  Q_FOREACH ( QString file, files )
  {
    QFileInfo fileInfo( file );
    if ( "qgs" == fileInfo.suffix().toLower() )
    {
      mZip.mQgsFile = file;
    }
    else
    {
      mZip.mFiles.append( file );
    }
  }

  bool readOk( false );
  if ( ! mZip.mQgsFile.isEmpty() )
  {
    readOk = read( mZip.mQgsFile );
  }
  else
  {
    setError( "Unable to find a .qgs file into the zip archive." );
  }

  if ( readOk )
  {
    mZip.mValid = true;
    mZip.mZipFile = filename;
    mZip.mDir = tmpDir.path();
  }
  else
  {
    clearZip();
  }

  return readOk;
}

void QgsProject::clearZip()
{
  // remove the previous directory and files
  QDir unzipDir( mZip.mDir );
  if ( !mZip.mDir.isEmpty() && unzipDir.exists() )
  {
    Q_FOREACH ( QFileInfo info, unzipDir.entryInfoList( QDir::Files ) )
    {
      QFile::remove( info.absoluteFilePath() );
    }

    QDir().rmdir( mZip.mDir );
  }

  mZip.mValid = false;
  mZip. mZipFile = "";
  mZip.mDir = "";
  mZip. mQgsFile = "";
  mZip.mFiles.clear();
}

void QgsProject::setFileName( const QString &name )
{
  if ( name == mFile.fileName() )
    return;

  QString oldHomePath = homePath();

  mFile.setFileName( name );
  emit fileNameChanged();

  QString newHomePath = homePath();
  if ( newHomePath != oldHomePath )
    emit homePathChanged();

  setDirty( true );
}

QString QgsProject::fileName() const
{
  return mFile.fileName();
}

QFileInfo QgsProject::fileInfo() const
{
  return QFileInfo( mFile );
}

QgsCoordinateReferenceSystem QgsProject::crs() const
{
  return mCrs;
}

void QgsProject::setCrs( const QgsCoordinateReferenceSystem &crs )
{
  mCrs = crs;
  writeEntry( QStringLiteral( "SpatialRefSys" ), QStringLiteral( "/ProjectCRSProj4String" ), crs.toProj4() );
  writeEntry( QStringLiteral( "SpatialRefSys" ), QStringLiteral( "/ProjectCRSID" ), static_cast< int >( crs.srsid() ) );
  writeEntry( QStringLiteral( "SpatialRefSys" ), QStringLiteral( "/ProjectCrs" ), crs.authid() );
  writeEntry( QStringLiteral( "SpatialRefSys" ), QStringLiteral( "/ProjectionsEnabled" ), crs.isValid() ? 1 : 0 );
  setDirty( true );
  emit crsChanged();
}

QString QgsProject::ellipsoid() const
{
  if ( !crs().isValid() )
    return GEO_NONE;

  return readEntry( QStringLiteral( "Measure" ), QStringLiteral( "/Ellipsoid" ), GEO_NONE );
}

void QgsProject::setEllipsoid( const QString &ellipsoid )
{
  writeEntry( QStringLiteral( "Measure" ), QStringLiteral( "/Ellipsoid" ), ellipsoid );
  setDirty( true );
}

void QgsProject::clear()
{
  mFile.setFileName( QString() );
  mProperties.clearKeys();
  mTitle.clear();
  mAutoTransaction = false;
  mEvaluateDefaultValues = false;
  mDirty = false;
  mCustomVariables.clear();

  mEmbeddedLayers.clear();
  mRelationManager->clear();
  mAnnotationManager->clear();
  mLayoutManager->clear();
  mSnappingConfig.reset();
  emit snappingConfigChanged( mSnappingConfig );

  mMapThemeCollection.reset( new QgsMapThemeCollection( this ) );
  emit mapThemeCollectionChanged();

  mRootGroup->clear();

  // reset some default project properties
  // XXX THESE SHOULD BE MOVED TO STATUSBAR RELATED SOURCE
  writeEntry( QStringLiteral( "PositionPrecision" ), QStringLiteral( "/Automatic" ), true );
  writeEntry( QStringLiteral( "PositionPrecision" ), QStringLiteral( "/DecimalPlaces" ), 2 );
  writeEntry( QStringLiteral( "Paths" ), QStringLiteral( "/Absolute" ), false );

  //copy default units to project
  QgsSettings s;
  writeEntry( QStringLiteral( "Measurement" ), QStringLiteral( "/DistanceUnits" ), s.value( QStringLiteral( "/qgis/measure/displayunits" ) ).toString() );
  writeEntry( QStringLiteral( "Measurement" ), QStringLiteral( "/AreaUnits" ), s.value( QStringLiteral( "/qgis/measure/areaunits" ) ).toString() );

  setDirty( false );
}

// basically a debugging tool to dump property list values
void dump_( const QgsProjectPropertyKey &topQgsPropertyKey )
{
  QgsDebugMsg( "current properties:" );
  topQgsPropertyKey.dump();
}


/**

Restore any optional properties found in "doc" to "properties".

properties tags for all optional properties.  Within that there will be scope
tags.  In the following example there exist one property in the "fsplugin"
scope.  "layers" is a list containing three string values.

\code{.xml}
<properties>
  <fsplugin>
    <foo type="int" >42</foo>
    <baz type="int" >1</baz>
    <layers type="QStringList" >
      <value>railroad</value>
      <value>airport</value>
    </layers>
    <xyqzzy type="int" >1</xyqzzy>
    <bar type="double" >123.456</bar>
    <feature_types type="QStringList" >
       <value>type</value>
    </feature_types>
  </fsplugin>
</properties>
\endcode

@param doc xml document
@param project_properties should be the top QgsProjectPropertyKey node.

*/
void _getProperties( const QDomDocument &doc, QgsProjectPropertyKey &project_properties )
{
  QDomElement propertiesElem = doc.documentElement().firstChildElement( QStringLiteral( "properties" ) );

  if ( propertiesElem.isNull() )  // no properties found, so we're done
  {
    return;
  }

  QDomNodeList scopes = propertiesElem.childNodes();

  if ( scopes.count() < 1 )
  {
    QgsDebugMsg( "empty ``properties'' XML tag ... bailing" );
    return;
  }

  if ( ! project_properties.readXml( propertiesElem ) )
  {
    QgsDebugMsg( "Project_properties.readXml() failed" );
  }
}


/**
   Get the project title
   @todo XXX we should go with the attribute xor title, not both.
*/
static void _getTitle( const QDomDocument &doc, QString &title )
{
  QDomNodeList nl = doc.elementsByTagName( QStringLiteral( "title" ) );

  title = QLatin1String( "" );               // by default the title will be empty

  if ( !nl.count() )
  {
    QgsDebugMsg( "unable to find title element" );
    return;
  }

  QDomNode titleNode = nl.item( 0 );  // there should only be one, so zeroth element ok

  if ( !titleNode.hasChildNodes() ) // if not, then there's no actual text
  {
    QgsDebugMsg( "unable to find title element" );
    return;
  }

  QDomNode titleTextNode = titleNode.firstChild();  // should only have one child

  if ( !titleTextNode.isText() )
  {
    QgsDebugMsg( "unable to find title element" );
    return;
  }

  QDomText titleText = titleTextNode.toText();

  title = titleText.data();

}

QgsProjectVersion getVersion( const QDomDocument &doc )
{
  QDomNodeList nl = doc.elementsByTagName( QStringLiteral( "qgis" ) );

  if ( !nl.count() )
  {
    QgsDebugMsg( " unable to find qgis element in project file" );
    return QgsProjectVersion( 0, 0, 0, QString() );
  }

  QDomNode qgisNode = nl.item( 0 );  // there should only be one, so zeroth element ok

  QDomElement qgisElement = qgisNode.toElement(); // qgis node should be element
  QgsProjectVersion projectVersion( qgisElement.attribute( QStringLiteral( "version" ) ) );
  return projectVersion;
}


QgsSnappingConfig QgsProject::snappingConfig() const
{
  return mSnappingConfig;
}

void QgsProject::setSnappingConfig( const QgsSnappingConfig &snappingConfig )
{
  if ( mSnappingConfig == snappingConfig )
    return;

  mSnappingConfig = snappingConfig;
  setDirty();
  emit snappingConfigChanged( mSnappingConfig );
}

bool QgsProject::_getMapLayers( const QDomDocument &doc, QList<QDomNode> &brokenNodes )
{
  // Layer order is set by the restoring the legend settings from project file.
  // This is done on the 'readProject( ... )' signal

  QDomNodeList nl = doc.elementsByTagName( QStringLiteral( "maplayer" ) );

  // process the map layer nodes

  if ( 0 == nl.count() )      // if we have no layers to process, bail
  {
    return true; // Decided to return "true" since it's
    // possible for there to be a project with no
    // layers; but also, more imporantly, this
    // would cause the tests/qgsproject to fail
    // since the test suite doesn't currently
    // support test layers
  }

  bool returnStatus = true;

  emit layerLoaded( 0, nl.count() );

  // order layers based on their dependencies
  QgsLayerDefinition::DependencySorter depSorter( doc );
  if ( depSorter.hasCycle() || depSorter.hasMissingDependency() )
    return false;

  QVector<QDomNode> sortedLayerNodes = depSorter.sortedLayerNodes();

  int i = 0;
  Q_FOREACH ( const QDomNode &node, sortedLayerNodes )
  {
    QDomElement element = node.toElement();

    QString name = node.namedItem( QStringLiteral( "layername" ) ).toElement().text();
    if ( !name.isNull() )
      emit loadingLayer( tr( "Loading layer %1" ).arg( name ) );

    if ( element.attribute( QStringLiteral( "embedded" ) ) == QLatin1String( "1" ) )
    {
      createEmbeddedLayer( element.attribute( QStringLiteral( "id" ) ), readPath( element.attribute( QStringLiteral( "project" ) ) ), brokenNodes );
      continue;
    }
    else
    {
      if ( !addLayer( element, brokenNodes ) )
      {
        returnStatus = false;
      }
    }
    emit layerLoaded( i + 1, nl.count() );
    i++;
  }

  return returnStatus;
}

bool QgsProject::addLayer( const QDomElement &layerElem, QList<QDomNode> &brokenNodes )
{
  QString type = layerElem.attribute( QStringLiteral( "type" ) );
  QgsDebugMsg( "Layer type is " + type );
  QgsMapLayer *mapLayer = nullptr;

  if ( type == QLatin1String( "vector" ) )
  {
    mapLayer = new QgsVectorLayer;
  }
  else if ( type == QLatin1String( "raster" ) )
  {
    mapLayer = new QgsRasterLayer;
  }
  else if ( type == QLatin1String( "plugin" ) )
  {
    QString typeName = layerElem.attribute( QStringLiteral( "name" ) );
    mapLayer = QgsApplication::pluginLayerRegistry()->createLayer( typeName );
  }

  if ( !mapLayer )
  {
    QgsDebugMsg( "Unable to create layer" );

    return false;
  }

  Q_CHECK_PTR( mapLayer ); // NOLINT

  // have the layer restore state that is stored in Dom node
  if ( mapLayer->readLayerXml( layerElem, pathResolver() ) && mapLayer->isValid() )
  {
    emit readMapLayer( mapLayer, layerElem );

    QList<QgsMapLayer *> myLayers;
    myLayers << mapLayer;
    addMapLayers( myLayers );

    return true;
  }
  else
  {
    delete mapLayer;

    QgsDebugMsg( "Unable to load " + type + " layer" );
    brokenNodes.push_back( layerElem );
    return false;
  }
}

bool QgsProject::read( const QString &filename )
{
  mFile.setFileName( filename );

  return read();
}

bool QgsProject::read()
{
  clearError();

  std::unique_ptr<QDomDocument> doc( new QDomDocument( QStringLiteral( "qgis" ) ) );

  if ( !mFile.open( QIODevice::ReadOnly | QIODevice::Text ) )
  {
    mFile.close();

    setError( tr( "Unable to open %1" ).arg( mFile.fileName() ) );

    return false;
  }

  // location of problem associated with errorMsg
  int line, column;
  QString errorMsg;

  if ( !doc->setContent( &mFile, &errorMsg, &line, &column ) )
  {
    // want to make this class as GUI independent as possible; so commented out
#if 0
    QMessageBox::critical( 0, tr( "Project File Read Error" ),
                           tr( "%1 at line %2 column %3" ).arg( errorMsg ).arg( line ).arg( column ) );
#endif

    QString errorString = tr( "Project file read error: %1 at line %2 column %3" )
                          .arg( errorMsg ).arg( line ).arg( column );

    QgsDebugMsg( errorString );

    mFile.close();

    setError( tr( "%1 for file %2" ).arg( errorString, mFile.fileName() ) );

    return false;
  }

  mFile.close();


  QgsDebugMsg( "Opened document " + mFile.fileName() );
  QgsDebugMsg( "Project title: " + mTitle );

  // get project version string, if any
  QgsProjectVersion fileVersion =  getVersion( *doc );
  QgsProjectVersion thisVersion( Qgis::QGIS_VERSION );

  if ( thisVersion > fileVersion )
  {
    QgsLogger::warning( "Loading a file that was saved with an older "
                        "version of qgis (saved in " + fileVersion.text() +
                        ", loaded in " + Qgis::QGIS_VERSION +
                        "). Problems may occur." );

    QgsProjectFileTransform projectFile( *doc, fileVersion );

    // Shows a warning when an old project file is read.
    emit oldProjectVersionWarning( fileVersion.text() );
    QgsDebugMsg( "Emitting oldProjectVersionWarning(oldVersion)." );

    projectFile.updateRevision( thisVersion );
  }

  // start new project, just keep the file name
  QString fileName = mFile.fileName();
  clear();
  mFile.setFileName( fileName );

  // now get any properties
  _getProperties( *doc, mProperties );

  QgsDebugMsg( QString::number( mProperties.count() ) + " properties read" );

  dump_( mProperties );

  // now get project title
  _getTitle( *doc, mTitle );

  //crs
  QgsCoordinateReferenceSystem projectCrs;
  if ( QgsProject::instance()->readNumEntry( QStringLiteral( "SpatialRefSys" ), QStringLiteral( "/ProjectionsEnabled" ), 0 ) )
  {
    long currentCRS = readNumEntry( QStringLiteral( "SpatialRefSys" ), QStringLiteral( "/ProjectCRSID" ), -1 );
    if ( currentCRS != -1 )
    {
      projectCrs = QgsCoordinateReferenceSystem::fromSrsId( currentCRS );
    }
  }
  mCrs = projectCrs;
  emit crsChanged();

  QDomNodeList nl = doc->elementsByTagName( QStringLiteral( "autotransaction" ) );
  if ( nl.count() )
  {
    QDomElement transactionElement = nl.at( 0 ).toElement();
    if ( transactionElement.attribute( QStringLiteral( "active" ), QStringLiteral( "0" ) ).toInt() == 1 )
      mAutoTransaction = true;
  }

  nl = doc->elementsByTagName( QStringLiteral( "evaluateDefaultValues" ) );
  if ( nl.count() )
  {
    QDomElement evaluateDefaultValuesElement = nl.at( 0 ).toElement();
    if ( evaluateDefaultValuesElement.attribute( QStringLiteral( "active" ), QStringLiteral( "0" ) ).toInt() == 1 )
      mEvaluateDefaultValues = true;
  }

  // read the layer tree from project file

  mRootGroup->setCustomProperty( QStringLiteral( "loading" ), 1 );

  QDomElement layerTreeElem = doc->documentElement().firstChildElement( QStringLiteral( "layer-tree-group" ) );
  if ( !layerTreeElem.isNull() )
  {
    mRootGroup->readChildrenFromXml( layerTreeElem );
  }
  else
  {
    QgsLayerTreeUtils::readOldLegend( mRootGroup, doc->documentElement().firstChildElement( QStringLiteral( "legend" ) ) );
  }

  mLayerTreeRegistryBridge->setEnabled( false );

  // get the map layers
  QList<QDomNode> brokenNodes;
  bool clean = _getMapLayers( *doc, brokenNodes );

  // review the integrity of the retrieved map layers
  if ( !clean )
  {
    QgsDebugMsg( "Unable to get map layers from project file." );

    if ( !brokenNodes.isEmpty() )
    {
      QgsDebugMsg( "there are " + QString::number( brokenNodes.size() ) + " broken layers" );
    }

    // we let a custom handler decide what to do with missing layers
    // (default implementation ignores them, there's also a GUI handler that lets user choose correct path)
    mBadLayerHandler->handleBadLayers( brokenNodes );
  }

  // Resolve references to other vector layers
  // Needs to be done here once all dependent layers are loaded
  for ( QMap<QString, QgsMapLayer *>::iterator it = mMapLayers.begin(); it != mMapLayers.end(); it++ )
  {
    if ( QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( it.value() ) )
      vl->resolveReferences( this );
  }

  mLayerTreeRegistryBridge->setEnabled( true );

  // load embedded groups and layers
  loadEmbeddedNodes( mRootGroup );

  // now that layers are loaded, we can resolve layer tree's references to the layers
  mRootGroup->resolveReferences( this );


  if ( !layerTreeElem.isNull() )
  {
    mRootGroup->readLayerOrderFromXml( layerTreeElem );
  }
  else
  {
    // Load pre 3.0 configuration
    QDomElement elem = doc->documentElement().firstChildElement( QStringLiteral( "layer-tree-canvas" ) );
    mRootGroup->readLayerOrderFromXml( elem );
  }

  // make sure the are just valid layers
  QgsLayerTreeUtils::removeInvalidLayers( mRootGroup );

  mRootGroup->removeCustomProperty( QStringLiteral( "loading" ) );

  mMapThemeCollection.reset( new QgsMapThemeCollection( this ) );
  emit mapThemeCollectionChanged();
  mMapThemeCollection->readXml( *doc );

  mAnnotationManager->readXml( doc->documentElement(), *doc );
  mLayoutManager->readXml( doc->documentElement(), *doc );

  // reassign change dependencies now that all layers are loaded
  QMap<QString, QgsMapLayer *> existingMaps = mapLayers();
  for ( QMap<QString, QgsMapLayer *>::iterator it = existingMaps.begin(); it != existingMaps.end(); it++ )
  {
    it.value()->setDependencies( it.value()->dependencies() );
  }

  mSnappingConfig.readProject( *doc );
  emit snappingConfigChanged( mSnappingConfig );

  //add variables defined in project file
  QStringList variableNames = readListEntry( QStringLiteral( "Variables" ), QStringLiteral( "/variableNames" ) );
  QStringList variableValues = readListEntry( QStringLiteral( "Variables" ), QStringLiteral( "/variableValues" ) );

  mCustomVariables.clear();
  if ( variableNames.length() == variableValues.length() )
  {
    for ( int i = 0; i < variableNames.length(); ++i )
    {
      mCustomVariables.insert( variableNames.at( i ), variableValues.at( i ) );
    }
  }
  else
  {
    QgsMessageLog::logMessage( tr( "Project Variables Invalid" ), tr( "The project contains invalid variable settings." ) );
  }
  emit customVariablesChanged();

  // read the project: used by map canvas and legend
  emit readProject( *doc );

  // if all went well, we're allegedly in pristine state
  if ( clean )
    setDirty( false );

  emit nonIdentifiableLayersChanged( nonIdentifiableLayers() );
  emit crsChanged();

  return true;
}


void QgsProject::loadEmbeddedNodes( QgsLayerTreeGroup *group )
{
  Q_FOREACH ( QgsLayerTreeNode *child, group->children() )
  {
    if ( QgsLayerTree::isGroup( child ) )
    {
      QgsLayerTreeGroup *childGroup = QgsLayerTree::toGroup( child );
      if ( childGroup->customProperty( QStringLiteral( "embedded" ) ).toInt() )
      {
        // make sure to convert the path from relative to absolute
        QString projectPath = readPath( childGroup->customProperty( QStringLiteral( "embedded_project" ) ).toString() );
        childGroup->setCustomProperty( QStringLiteral( "embedded_project" ), projectPath );

        QgsLayerTreeGroup *newGroup = createEmbeddedGroup( childGroup->name(), projectPath, childGroup->customProperty( QStringLiteral( "embedded-invisible-layers" ) ).toStringList() );
        if ( newGroup )
        {
          QList<QgsLayerTreeNode *> clonedChildren;
          Q_FOREACH ( QgsLayerTreeNode *newGroupChild, newGroup->children() )
            clonedChildren << newGroupChild->clone();
          delete newGroup;

          childGroup->insertChildNodes( 0, clonedChildren );
        }
      }
      else
      {
        loadEmbeddedNodes( childGroup );
      }
    }
    else if ( QgsLayerTree::isLayer( child ) )
    {
      if ( child->customProperty( QStringLiteral( "embedded" ) ).toInt() )
      {
        QList<QDomNode> brokenNodes;
        createEmbeddedLayer( QgsLayerTree::toLayer( child )->layerId(), child->customProperty( QStringLiteral( "embedded_project" ) ).toString(), brokenNodes );
      }
    }

  }
}

QVariantMap QgsProject::customVariables() const
{
  return mCustomVariables;
}

void QgsProject::setCustomVariables( const QVariantMap &variables )
{
  if ( variables == mCustomVariables )
    return;

  //write variable to project
  QStringList variableNames;
  QStringList variableValues;

  QVariantMap::const_iterator it = variables.constBegin();
  for ( ; it != variables.constEnd(); ++it )
  {
    variableNames << it.key();
    variableValues << it.value().toString();
  }

  writeEntry( QStringLiteral( "Variables" ), QStringLiteral( "/variableNames" ), variableNames );
  writeEntry( QStringLiteral( "Variables" ), QStringLiteral( "/variableValues" ), variableValues );

  mCustomVariables = variables;

  emit customVariablesChanged();
}

QList<QgsVectorLayer *> QgsProject::avoidIntersectionsLayers() const
{
  QList<QgsVectorLayer *> layers;
  QStringList layerIds = readListEntry( QStringLiteral( "Digitizing" ), QStringLiteral( "/AvoidIntersectionsList" ), QStringList() );
  Q_FOREACH ( const QString &layerId, layerIds )
  {
    if ( QgsVectorLayer *vlayer = qobject_cast<QgsVectorLayer *>( mapLayer( layerId ) ) )
      layers << vlayer;
  }
  return layers;
}

void QgsProject::setAvoidIntersectionsLayers( const QList<QgsVectorLayer *> &layers )
{
  QStringList list;
  Q_FOREACH ( QgsVectorLayer *layer, layers )
    list << layer->id();
  writeEntry( QStringLiteral( "Digitizing" ), QStringLiteral( "/AvoidIntersectionsList" ), list );
  emit avoidIntersectionsLayersChanged();
}

QgsExpressionContext QgsProject::createExpressionContext() const
{
  QgsExpressionContext context;

  context << QgsExpressionContextUtils::globalScope()
          << QgsExpressionContextUtils::projectScope( this );

  return context;
}

void QgsProject::onMapLayersAdded( const QList<QgsMapLayer *> &layers )
{
  QMap<QString, QgsMapLayer *> existingMaps = mapLayers();

  bool tgChanged = false;

  Q_FOREACH ( QgsMapLayer *layer, layers )
  {
    QgsVectorLayer *vlayer = qobject_cast<QgsVectorLayer *>( layer );
    if ( vlayer )
    {
      if ( autoTransaction() )
      {
        if ( QgsTransaction::supportsTransaction( vlayer ) )
        {
          QString connString = QgsDataSourceUri( vlayer->source() ).connectionInfo();
          QString key = vlayer->providerType();

          QgsTransactionGroup *tg = mTransactionGroups.value( qMakePair( key, connString ) );

          if ( !tg )
          {
            tg = new QgsTransactionGroup();
            mTransactionGroups.insert( qMakePair( key, connString ), tg );
            tgChanged = true;
          }
          tg->addLayer( vlayer );
        }
      }
      vlayer->dataProvider()->setProviderProperty( QgsVectorDataProvider::EvaluateDefaultValues, evaluateDefaultValues() );
    }

    if ( tgChanged )
      emit transactionGroupsChanged();

    connect( layer, &QgsMapLayer::configChanged, this, [ = ] { setDirty(); } );

    // check if we have to update connections for layers with dependencies
    for ( QMap<QString, QgsMapLayer *>::iterator it = existingMaps.begin(); it != existingMaps.end(); it++ )
    {
      QSet<QgsMapLayerDependency> deps = it.value()->dependencies();
      if ( deps.contains( layer->id() ) )
      {
        // reconnect to change signals
        it.value()->setDependencies( deps );
      }
    }
  }

  if ( mSnappingConfig.addLayers( layers ) )
    emit snappingConfigChanged( mSnappingConfig );
}

void QgsProject::onMapLayersRemoved( const QList<QgsMapLayer *> &layers )
{
  if ( mSnappingConfig.removeLayers( layers ) )
    emit snappingConfigChanged( mSnappingConfig );
}

void QgsProject::cleanTransactionGroups( bool force )
{
  bool changed = false;
  for ( QMap< QPair< QString, QString>, QgsTransactionGroup *>::Iterator tg = mTransactionGroups.begin(); tg != mTransactionGroups.end(); )
  {
    if ( tg.value()->isEmpty() || force )
    {
      delete tg.value();
      tg = mTransactionGroups.erase( tg );
      changed = true;
    }
    else
    {
      ++tg;
    }
  }
  if ( changed )
    emit transactionGroupsChanged();
}

bool QgsProject::readLayer( const QDomNode &layerNode )
{
  QList<QDomNode> brokenNodes;
  if ( addLayer( layerNode.toElement(), brokenNodes ) )
  {
    // have to try to update joins for all layers now - a previously added layer may be dependent on this newly
    // added layer for joins
    QVector<QgsVectorLayer *> vectorLayers = layers<QgsVectorLayer *>();
    Q_FOREACH ( QgsVectorLayer *layer, vectorLayers )
    {
      // TODO: should be only done later - and with all layers (other layers may have referenced this layer)
      layer->resolveReferences( this );
    }

    return true;
  }
  return false;
}

bool QgsProject::write( const QString &filename )
{
  mFile.setFileName( filename );

  return write();
}

bool QgsProject::zip( const QString &name )
{
  clearError();

  bool zipOk = false;
  QString originalName = mFile.fileName();
  QTemporaryDir tmpDir;

  if ( QFile::exists( name ) )
  {
    if ( ! QFile::remove( name ) )
    {
      setError( tr( "Unable to remove zip file '%1'" ).arg( name ) );
      return false;
    }
  }

  if ( tmpDir.isValid() )
  {
    // create qgs file into tmp directory
    QFile qgsFile( QDir( tmpDir.path() ).filePath( "project.qgs" ) );

    if ( qgsFile.open( QIODevice::WriteOnly ) )
    {
      mFile.setFileName( qgsFile.fileName() );
      if ( write() )
      {
        QStringList files;
        files.append( mFile.fileName() );
        zipOk = QgsZipUtils::zip( name, files );
      }

      mFile.setFileName( originalName );
      qgsFile.close();
    }
  }
  else
  {
    setError( "Cannot open temporary file" );
  }

  return zipOk;
}

bool QgsProject::write()
{
  clearError();

  // if we have problems creating or otherwise writing to the project file,
  // let's find out up front before we go through all the hand-waving
  // necessary to create all the Dom objects
  QFileInfo myFileInfo( mFile );
  if ( myFileInfo.exists() && !myFileInfo.isWritable() )
  {
    setError( tr( "%1 is not writable. Please adjust permissions (if possible) and try again." )
              .arg( mFile.fileName() ) );
    return false;
  }

  QDomImplementation DomImplementation;
  DomImplementation.setInvalidDataPolicy( QDomImplementation::DropInvalidChars );

  QDomDocumentType documentType =
    DomImplementation.createDocumentType( QStringLiteral( "qgis" ), QStringLiteral( "http://mrcc.com/qgis.dtd" ),
                                          QStringLiteral( "SYSTEM" ) );
  std::unique_ptr<QDomDocument> doc( new QDomDocument( documentType ) );

  QDomElement qgisNode = doc->createElement( QStringLiteral( "qgis" ) );
  qgisNode.setAttribute( QStringLiteral( "projectname" ), title() );
  qgisNode.setAttribute( QStringLiteral( "version" ), QStringLiteral( "%1" ).arg( Qgis::QGIS_VERSION ) );

  doc->appendChild( qgisNode );

  // title
  QDomElement titleNode = doc->createElement( QStringLiteral( "title" ) );
  qgisNode.appendChild( titleNode );

  QDomElement transactionNode = doc->createElement( QStringLiteral( "autotransaction" ) );
  transactionNode.setAttribute( QStringLiteral( "active" ), mAutoTransaction ? "1" : "0" );
  qgisNode.appendChild( transactionNode );

  QDomElement evaluateDefaultValuesNode = doc->createElement( QStringLiteral( "evaluateDefaultValues" ) );
  evaluateDefaultValuesNode.setAttribute( QStringLiteral( "active" ), mEvaluateDefaultValues ? "1" : "0" );
  qgisNode.appendChild( evaluateDefaultValuesNode );

  QDomText titleText = doc->createTextNode( title() );  // XXX why have title TWICE?
  titleNode.appendChild( titleText );

  // write layer tree - make sure it is without embedded subgroups
  QgsLayerTreeNode *clonedRoot = mRootGroup->clone();
  QgsLayerTreeUtils::replaceChildrenOfEmbeddedGroups( QgsLayerTree::toGroup( clonedRoot ) );
  QgsLayerTreeUtils::updateEmbeddedGroupsProjectPath( QgsLayerTree::toGroup( clonedRoot ), this ); // convert absolute paths to relative paths if required
  clonedRoot->writeXml( qgisNode );
  delete clonedRoot;

  mSnappingConfig.writeProject( *doc );

  // let map canvas and legend write their information
  emit writeProject( *doc );

  // within top level node save list of layers
  const QMap<QString, QgsMapLayer *> &layers = mapLayers();

  // Iterate over layers in zOrder
  // Call writeXml() on each
  QDomElement projectLayersNode = doc->createElement( QStringLiteral( "projectlayers" ) );

  QMap<QString, QgsMapLayer *>::ConstIterator li = layers.constBegin();
  while ( li != layers.end() )
  {
    QgsMapLayer *ml = li.value();

    if ( ml )
    {
      QHash< QString, QPair< QString, bool> >::const_iterator emIt = mEmbeddedLayers.constFind( ml->id() );
      if ( emIt == mEmbeddedLayers.constEnd() )
      {
        // general layer metadata
        QDomElement maplayerElem = doc->createElement( QStringLiteral( "maplayer" ) );

        ml->writeLayerXml( maplayerElem, *doc, pathResolver() );

        emit writeMapLayer( ml, maplayerElem, *doc );

        projectLayersNode.appendChild( maplayerElem );
      }
      else
      {
        // layer defined in an external project file
        // only save embedded layer if not managed by a legend group
        if ( emIt.value().second )
        {
          QDomElement mapLayerElem = doc->createElement( QStringLiteral( "maplayer" ) );
          mapLayerElem.setAttribute( QStringLiteral( "embedded" ), 1 );
          mapLayerElem.setAttribute( QStringLiteral( "project" ), writePath( emIt.value().first ) );
          mapLayerElem.setAttribute( QStringLiteral( "id" ), ml->id() );
          projectLayersNode.appendChild( mapLayerElem );
        }
      }
    }
    li++;
  }

  qgisNode.appendChild( projectLayersNode );

  QDomElement layerOrderNode = doc->createElement( QStringLiteral( "layerorder" ) );
  Q_FOREACH ( QgsMapLayer *layer, mRootGroup->customLayerOrder() )
  {
    QDomElement mapLayerElem = doc->createElement( QStringLiteral( "layer" ) );
    mapLayerElem.setAttribute( QStringLiteral( "id" ), layer->id() );
    layerOrderNode.appendChild( mapLayerElem );
  }
  qgisNode.appendChild( layerOrderNode );


  // now add the optional extra properties

  dump_( mProperties );

  QgsDebugMsg( QString( "there are %1 property scopes" ).arg( static_cast<int>( mProperties.count() ) ) );

  if ( !mProperties.isEmpty() ) // only worry about properties if we
    // actually have any properties
  {
    mProperties.writeXml( QStringLiteral( "properties" ), qgisNode, *doc );
  }

  mMapThemeCollection->writeXml( *doc );

  QDomElement annotationsElem = mAnnotationManager->writeXml( *doc );
  qgisNode.appendChild( annotationsElem );

  QDomElement layoutElem = mLayoutManager->writeXml( *doc );
  qgisNode.appendChild( layoutElem );

  // now wrap it up and ship it to the project file
  doc->normalize();             // XXX I'm not entirely sure what this does

  // Create backup file
  if ( QFile::exists( fileName() ) )
  {
    QFile backupFile( fileName() + '~' );
    bool ok = true;
    ok &= backupFile.open( QIODevice::WriteOnly | QIODevice::Truncate );
    ok &= mFile.open( QIODevice::ReadOnly );

    QByteArray ba;
    while ( ok && !mFile.atEnd() )
    {
      ba = mFile.read( 10240 );
      ok &= backupFile.write( ba ) == ba.size();
    }

    mFile.close();
    backupFile.close();

    if ( !ok )
    {
      setError( tr( "Unable to create backup file %1" ).arg( backupFile.fileName() ) );
      return false;
    }

    QFileInfo fi( fileName() );
    struct utimbuf tb = { fi.lastRead().toTime_t(), fi.lastModified().toTime_t() };
    utime( backupFile.fileName().toUtf8().constData(), &tb );
  }

  if ( !mFile.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
  {
    mFile.close();         // even though we got an error, let's make
    // sure it's closed anyway

    setError( tr( "Unable to save to file %1" ).arg( mFile.fileName() ) );
    return false;
  }

  QTemporaryFile tempFile;
  bool ok = tempFile.open();
  if ( ok )
  {
    QTextStream projectFileStream( &tempFile );
    doc->save( projectFileStream, 2 );  // save as utf-8
    ok &= projectFileStream.pos() > -1;

    ok &= tempFile.seek( 0 );

    QByteArray ba;
    while ( ok && !tempFile.atEnd() )
    {
      ba = tempFile.read( 10240 );
      ok &= mFile.write( ba ) == ba.size();
    }

    ok &= mFile.error() == QFile::NoError;

    mFile.close();
  }

  tempFile.close();

  if ( !ok )
  {
    setError( tr( "Unable to save to file %1. Your project "
                  "may be corrupted on disk. Try clearing some space on the volume and "
                  "check file permissions before pressing save again." )
              .arg( mFile.fileName() ) );
    return false;
  }

  setDirty( false );               // reset to pristine state

  emit projectSaved();

  return true;
}

bool QgsProject::writeEntry( const QString &scope, QString const &key, bool value )
{
  setDirty( true );

  return addKey_( scope, key, &mProperties, value );
}

bool QgsProject::writeEntry( const QString &scope, const QString &key, double value )
{
  setDirty( true );

  return addKey_( scope, key, &mProperties, value );
}

bool QgsProject::writeEntry( const QString &scope, QString const &key, int value )
{
  setDirty( true );

  return addKey_( scope, key, &mProperties, value );
}

bool QgsProject::writeEntry( const QString &scope, const QString &key, const QString &value )
{
  setDirty( true );

  return addKey_( scope, key, &mProperties, value );
}

bool QgsProject::writeEntry( const QString &scope, const QString &key, const QStringList &value )
{
  setDirty( true );

  return addKey_( scope, key, &mProperties, value );
}

QStringList QgsProject::readListEntry( const QString &scope,
                                       const QString &key,
                                       const QStringList &def,
                                       bool *ok ) const
{
  QgsProjectProperty *property = findKey_( scope, key, mProperties );

  QVariant value;

  if ( property )
  {
    value = property->value();

    bool valid = QVariant::StringList == value.type();
    if ( ok )
      *ok = valid;

    if ( valid )
    {
      return value.toStringList();
    }
  }

  return def;
}


QString QgsProject::readEntry( const QString &scope,
                               const QString &key,
                               const QString &def,
                               bool *ok ) const
{
  QgsProjectProperty *property = findKey_( scope, key, mProperties );

  QVariant value;

  if ( property )
  {
    value = property->value();

    bool valid = value.canConvert( QVariant::String );
    if ( ok )
      *ok = valid;

    if ( valid )
      return value.toString();
  }

  return def;
}

int QgsProject::readNumEntry( const QString &scope, const QString &key, int def,
                              bool *ok ) const
{
  QgsProjectProperty *property = findKey_( scope, key, mProperties );

  QVariant value;

  if ( property )
  {
    value = property->value();
  }

  bool valid = value.canConvert( QVariant::Int );

  if ( ok )
  {
    *ok = valid;
  }

  if ( valid )
  {
    return value.toInt();
  }

  return def;
}

double QgsProject::readDoubleEntry( const QString &scope, const QString &key,
                                    double def,
                                    bool *ok ) const
{
  QgsProjectProperty *property = findKey_( scope, key, mProperties );
  if ( property )
  {
    QVariant value = property->value();

    bool valid = value.canConvert( QVariant::Double );
    if ( ok )
      *ok = valid;

    if ( valid )
      return value.toDouble();
  }

  return def;
}

bool QgsProject::readBoolEntry( const QString &scope, const QString &key, bool def,
                                bool *ok ) const
{
  QgsProjectProperty *property = findKey_( scope, key, mProperties );

  if ( property )
  {
    QVariant value = property->value();

    bool valid = value.canConvert( QVariant::Bool );
    if ( ok )
      *ok = valid;

    if ( valid )
      return value.toBool();
  }

  return def;
}


bool QgsProject::removeEntry( const QString &scope, const QString &key )
{
  removeKey_( scope, key, mProperties );

  setDirty( true );

  return !findKey_( scope, key, mProperties );
}


QStringList QgsProject::entryList( const QString &scope, const QString &key ) const
{
  QgsProjectProperty *foundProperty = findKey_( scope, key, mProperties );

  QStringList entries;

  if ( foundProperty )
  {
    QgsProjectPropertyKey *propertyKey = dynamic_cast<QgsProjectPropertyKey *>( foundProperty );

    if ( propertyKey )
    { propertyKey->entryList( entries ); }
  }

  return entries;
}

QStringList QgsProject::subkeyList( const QString &scope, const QString &key ) const
{
  QgsProjectProperty *foundProperty = findKey_( scope, key, mProperties );

  QStringList entries;

  if ( foundProperty )
  {
    QgsProjectPropertyKey *propertyKey = dynamic_cast<QgsProjectPropertyKey *>( foundProperty );

    if ( propertyKey )
    { propertyKey->subkeyList( entries ); }
  }

  return entries;
}

void QgsProject::dumpProperties() const
{
  dump_( mProperties );
}

QgsPathResolver QgsProject::pathResolver() const
{
  bool absolutePaths = readBoolEntry( QStringLiteral( "Paths" ), QStringLiteral( "/Absolute" ), false );
  return QgsPathResolver( absolutePaths ? QString() : fileName() );
}

QString QgsProject::readPath( const QString &src ) const
{
  return pathResolver().readPath( src );
}

QString QgsProject::writePath( const QString &src ) const
{
  return pathResolver().writePath( src );
}

void QgsProject::setError( const QString &errorMessage )
{
  mErrorMessage = errorMessage;
}

QString QgsProject::error() const
{
  return mErrorMessage;
}

void QgsProject::clearError()
{
  setError( QString() );
}

void QgsProject::setBadLayerHandler( QgsProjectBadLayerHandler *handler )
{
  delete mBadLayerHandler;
  mBadLayerHandler = handler;
}

QString QgsProject::layerIsEmbedded( const QString &id ) const
{
  QHash< QString, QPair< QString, bool > >::const_iterator it = mEmbeddedLayers.find( id );
  if ( it == mEmbeddedLayers.constEnd() )
  {
    return QString();
  }
  return it.value().first;
}

bool QgsProject::createEmbeddedLayer( const QString &layerId, const QString &projectFilePath, QList<QDomNode> &brokenNodes,
                                      bool saveFlag )
{
  QgsDebugCall;

  static QString sPrevProjectFilePath;
  static QDateTime sPrevProjectFileTimestamp;
  static QDomDocument sProjectDocument;

  QDateTime projectFileTimestamp = QFileInfo( projectFilePath ).lastModified();

  if ( projectFilePath != sPrevProjectFilePath || projectFileTimestamp != sPrevProjectFileTimestamp )
  {
    sPrevProjectFilePath.clear();

    QFile projectFile( projectFilePath );
    if ( !projectFile.open( QIODevice::ReadOnly ) )
    {
      return false;
    }

    if ( !sProjectDocument.setContent( &projectFile ) )
    {
      return false;
    }

    sPrevProjectFilePath = projectFilePath;
    sPrevProjectFileTimestamp = projectFileTimestamp;
  }

  // does project store paths absolute or relative?
  bool useAbsolutePaths = true;

  QDomElement propertiesElem = sProjectDocument.documentElement().firstChildElement( QStringLiteral( "properties" ) );
  if ( !propertiesElem.isNull() )
  {
    QDomElement absElem = propertiesElem.firstChildElement( QStringLiteral( "Paths" ) ).firstChildElement( QStringLiteral( "Absolute" ) );
    if ( !absElem.isNull() )
    {
      useAbsolutePaths = absElem.text().compare( QLatin1String( "true" ), Qt::CaseInsensitive ) == 0;
    }
  }

  QDomElement projectLayersElem = sProjectDocument.documentElement().firstChildElement( QStringLiteral( "projectlayers" ) );
  if ( projectLayersElem.isNull() )
  {
    return false;
  }

  QDomNodeList mapLayerNodes = projectLayersElem.elementsByTagName( QStringLiteral( "maplayer" ) );
  for ( int i = 0; i < mapLayerNodes.size(); ++i )
  {
    // get layer id
    QDomElement mapLayerElem = mapLayerNodes.at( i ).toElement();
    QString id = mapLayerElem.firstChildElement( QStringLiteral( "id" ) ).text();
    if ( id == layerId )
    {
      // layer can be embedded only once
      if ( mapLayerElem.attribute( QStringLiteral( "embedded" ) ) == QLatin1String( "1" ) )
      {
        return false;
      }

      mEmbeddedLayers.insert( layerId, qMakePair( projectFilePath, saveFlag ) );

      // change datasource path from relative to absolute if necessary
      // see also QgsMapLayer::readLayerXML
      if ( !useAbsolutePaths )
      {
        QString provider( mapLayerElem.firstChildElement( QStringLiteral( "provider" ) ).text() );
        QDomElement dsElem( mapLayerElem.firstChildElement( QStringLiteral( "datasource" ) ) );
        QString datasource( dsElem.text() );
        if ( provider == QLatin1String( "spatialite" ) )
        {
          QgsDataSourceUri uri( datasource );
          QFileInfo absoluteDs( QFileInfo( projectFilePath ).absolutePath() + '/' + uri.database() );
          if ( absoluteDs.exists() )
          {
            uri.setDatabase( absoluteDs.absoluteFilePath() );
            datasource = uri.uri();
          }
        }
        else if ( provider == QLatin1String( "ogr" ) )
        {
          QStringList theURIParts( datasource.split( '|' ) );
          QFileInfo absoluteDs( QFileInfo( projectFilePath ).absolutePath() + '/' + theURIParts[0] );
          if ( absoluteDs.exists() )
          {
            theURIParts[0] = absoluteDs.absoluteFilePath();
            datasource = theURIParts.join( QStringLiteral( "|" ) );
          }
        }
        else if ( provider == QLatin1String( "gpx" ) )
        {
          QStringList theURIParts( datasource.split( '?' ) );
          QFileInfo absoluteDs( QFileInfo( projectFilePath ).absolutePath() + '/' + theURIParts[0] );
          if ( absoluteDs.exists() )
          {
            theURIParts[0] = absoluteDs.absoluteFilePath();
            datasource = theURIParts.join( QStringLiteral( "?" ) );
          }
        }
        else if ( provider == QLatin1String( "delimitedtext" ) )
        {
          QUrl urlSource( QUrl::fromEncoded( datasource.toLatin1() ) );

          if ( !datasource.startsWith( QLatin1String( "file:" ) ) )
          {
            QUrl file( QUrl::fromLocalFile( datasource.left( datasource.indexOf( '?' ) ) ) );
            urlSource.setScheme( QStringLiteral( "file" ) );
            urlSource.setPath( file.path() );
          }

          QFileInfo absoluteDs( QFileInfo( projectFilePath ).absolutePath() + '/' + urlSource.toLocalFile() );
          if ( absoluteDs.exists() )
          {
            QUrl urlDest = QUrl::fromLocalFile( absoluteDs.absoluteFilePath() );
            urlDest.setQueryItems( urlSource.queryItems() );
            datasource = QString::fromAscii( urlDest.toEncoded() );
          }
        }
        else
        {
          QFileInfo absoluteDs( QFileInfo( projectFilePath ).absolutePath() + '/' + datasource );
          if ( absoluteDs.exists() )
          {
            datasource = absoluteDs.absoluteFilePath();
          }
        }

        dsElem.removeChild( dsElem.childNodes().at( 0 ) );
        dsElem.appendChild( sProjectDocument.createTextNode( datasource ) );
      }

      if ( addLayer( mapLayerElem, brokenNodes ) )
      {
        return true;
      }
      else
      {
        mEmbeddedLayers.remove( layerId );
        return false;
      }
    }
  }

  return false;
}


QgsLayerTreeGroup *QgsProject::createEmbeddedGroup( const QString &groupName, const QString &projectFilePath, const QStringList &invisibleLayers )
{
  // open project file, get layer ids in group, add the layers
  QFile projectFile( projectFilePath );
  if ( !projectFile.open( QIODevice::ReadOnly ) )
  {
    return nullptr;
  }

  QDomDocument projectDocument;
  if ( !projectDocument.setContent( &projectFile ) )
  {
    return nullptr;
  }

  // store identify disabled layers of the embedded project
  QSet<QString> embeddedIdentifyDisabledLayers;
  QDomElement disabledLayersElem = projectDocument.documentElement().firstChildElement( QStringLiteral( "properties" ) ).firstChildElement( QStringLiteral( "Identify" ) ).firstChildElement( QStringLiteral( "disabledLayers" ) );
  if ( !disabledLayersElem.isNull() )
  {
    QDomNodeList valueList = disabledLayersElem.elementsByTagName( QStringLiteral( "value" ) );
    for ( int i = 0; i < valueList.size(); ++i )
    {
      embeddedIdentifyDisabledLayers.insert( valueList.at( i ).toElement().text() );
    }
  }

  QgsLayerTreeGroup *root = new QgsLayerTreeGroup;

  QDomElement layerTreeElem = projectDocument.documentElement().firstChildElement( QStringLiteral( "layer-tree-group" ) );
  if ( !layerTreeElem.isNull() )
  {
    root->readChildrenFromXml( layerTreeElem );
  }
  else
  {
    QgsLayerTreeUtils::readOldLegend( root, projectDocument.documentElement().firstChildElement( QStringLiteral( "legend" ) ) );
  }

  QgsLayerTreeGroup *group = root->findGroup( groupName );
  if ( !group || group->customProperty( QStringLiteral( "embedded" ) ).toBool() )
  {
    // embedded groups cannot be embedded again
    delete root;
    return nullptr;
  }

  // clone the group sub-tree (it is used already in a tree, we cannot just tear it off)
  QgsLayerTreeGroup *newGroup = QgsLayerTree::toGroup( group->clone() );
  delete root;
  root = nullptr;

  newGroup->setCustomProperty( QStringLiteral( "embedded" ), 1 );
  newGroup->setCustomProperty( QStringLiteral( "embedded_project" ), projectFilePath );

  // set "embedded" to all children + load embedded layers
  mLayerTreeRegistryBridge->setEnabled( false );
  initializeEmbeddedSubtree( projectFilePath, newGroup );
  mLayerTreeRegistryBridge->setEnabled( true );

  QStringList thisProjectIdentifyDisabledLayers = nonIdentifiableLayers();

  // consider the layers might be identify disabled in its project
  Q_FOREACH ( const QString &layerId, newGroup->findLayerIds() )
  {
    if ( embeddedIdentifyDisabledLayers.contains( layerId ) )
    {
      thisProjectIdentifyDisabledLayers.append( layerId );
    }

    QgsLayerTreeLayer *layer = newGroup->findLayer( layerId );
    if ( layer )
    {
      layer->setItemVisibilityChecked( invisibleLayers.contains( layerId ) );
    }
  }

  setNonIdentifiableLayers( thisProjectIdentifyDisabledLayers );

  return newGroup;
}

void QgsProject::initializeEmbeddedSubtree( const QString &projectFilePath, QgsLayerTreeGroup *group )
{
  Q_FOREACH ( QgsLayerTreeNode *child, group->children() )
  {
    // all nodes in the subtree will have "embedded" custom property set
    child->setCustomProperty( QStringLiteral( "embedded" ), 1 );

    if ( QgsLayerTree::isGroup( child ) )
    {
      initializeEmbeddedSubtree( projectFilePath, QgsLayerTree::toGroup( child ) );
    }
    else if ( QgsLayerTree::isLayer( child ) )
    {
      // load the layer into our project
      QList<QDomNode> brokenNodes;
      createEmbeddedLayer( QgsLayerTree::toLayer( child )->layerId(), projectFilePath, brokenNodes, false );
    }
  }
}

bool QgsProject::evaluateDefaultValues() const
{
  return mEvaluateDefaultValues;
}

void QgsProject::setEvaluateDefaultValues( bool evaluateDefaultValues )
{
  Q_FOREACH ( QgsMapLayer *layer, mapLayers().values() )
  {
    QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( layer );
    if ( vl )
    {
      vl->dataProvider()->setProviderProperty( QgsVectorDataProvider::EvaluateDefaultValues, evaluateDefaultValues );
    }
  }

  mEvaluateDefaultValues = evaluateDefaultValues;
}

void QgsProject::setTopologicalEditing( bool enabled )
{
  writeEntry( QStringLiteral( "Digitizing" ), QStringLiteral( "/TopologicalEditing" ), ( enabled ? 1 : 0 ) );
  emit topologicalEditingChanged();
}

bool QgsProject::topologicalEditing() const
{
  return readNumEntry( QStringLiteral( "Digitizing" ), QStringLiteral( "/TopologicalEditing" ), 0 );
}

QgsUnitTypes::DistanceUnit QgsProject::distanceUnits() const
{
  QString distanceUnitString = readEntry( QStringLiteral( "Measurement" ), QStringLiteral( "/DistanceUnits" ), QString() );
  if ( !distanceUnitString.isEmpty() )
    return QgsUnitTypes::decodeDistanceUnit( distanceUnitString );

  //fallback to QGIS default measurement unit
  QgsSettings s;
  bool ok = false;
  QgsUnitTypes::DistanceUnit type = QgsUnitTypes::decodeDistanceUnit( s.value( QStringLiteral( "/qgis/measure/displayunits" ) ).toString(), &ok );
  return ok ? type : QgsUnitTypes::DistanceMeters;
}

void QgsProject::setDistanceUnits( QgsUnitTypes::DistanceUnit unit )
{
  writeEntry( QStringLiteral( "Measurement" ), QStringLiteral( "/DistanceUnits" ), QgsUnitTypes::encodeUnit( unit ) );
}

QgsUnitTypes::AreaUnit QgsProject::areaUnits() const
{
  QString areaUnitString = readEntry( QStringLiteral( "Measurement" ), QStringLiteral( "/AreaUnits" ), QString() );
  if ( !areaUnitString.isEmpty() )
    return QgsUnitTypes::decodeAreaUnit( areaUnitString );

  //fallback to QGIS default area unit
  QgsSettings s;
  bool ok = false;
  QgsUnitTypes::AreaUnit type = QgsUnitTypes::decodeAreaUnit( s.value( QStringLiteral( "/qgis/measure/areaunits" ) ).toString(), &ok );
  return ok ? type : QgsUnitTypes::AreaSquareMeters;
}

void QgsProject::setAreaUnits( QgsUnitTypes::AreaUnit unit )
{
  writeEntry( QStringLiteral( "Measurement" ), QStringLiteral( "/AreaUnits" ), QgsUnitTypes::encodeUnit( unit ) );
}

QString QgsProject::homePath() const
{
  QFileInfo pfi( fileName() );
  if ( !pfi.exists() )
    return QString::null;

  return pfi.canonicalPath();
}

QgsRelationManager *QgsProject::relationManager() const
{
  return mRelationManager;
}

const QgsLayoutManager *QgsProject::layoutManager() const
{
  return mLayoutManager.get();
}

QgsLayoutManager *QgsProject::layoutManager()
{
  return mLayoutManager.get();
}

QgsLayerTree *QgsProject::layerTreeRoot() const
{
  return mRootGroup;
}

QgsMapThemeCollection *QgsProject::mapThemeCollection()
{
  return mMapThemeCollection.get();
}

QgsAnnotationManager *QgsProject::annotationManager()
{
  return mAnnotationManager.get();
}

void QgsProject::setNonIdentifiableLayers( const QList<QgsMapLayer *> &layers )
{
  QStringList currentLayers = nonIdentifiableLayers();

  QStringList newLayers;
  Q_FOREACH ( QgsMapLayer *l, layers )
  {
    newLayers << l->id();
  }

  if ( newLayers == currentLayers )
    return;

  QStringList disabledLayerIds;

  Q_FOREACH ( QgsMapLayer *l, layers )
  {
    disabledLayerIds << l->id();
  }

  setNonIdentifiableLayers( disabledLayerIds );
}

void QgsProject::setNonIdentifiableLayers( const QStringList &layerIds )
{
  writeEntry( QStringLiteral( "Identify" ), QStringLiteral( "/disabledLayers" ), layerIds );

  emit nonIdentifiableLayersChanged( layerIds );
}

QStringList QgsProject::nonIdentifiableLayers() const
{
  return readListEntry( QStringLiteral( "Identify" ), QStringLiteral( "/disabledLayers" ) );
}

bool QgsProject::autoTransaction() const
{
  return mAutoTransaction;
}

void QgsProject::setAutoTransaction( bool autoTransaction )
{
  if ( autoTransaction != mAutoTransaction )
  {
    mAutoTransaction = autoTransaction;

    if ( autoTransaction )
      onMapLayersAdded( mapLayers().values() );
    else
      cleanTransactionGroups( true );
  }
}

QMap<QPair<QString, QString>, QgsTransactionGroup *> QgsProject::transactionGroups()
{
  return mTransactionGroups;
}


//
// QgsMapLayerRegistry methods
//


int QgsProject::count() const
{
  return mMapLayers.size();
}

QgsMapLayer *QgsProject::mapLayer( const QString &layerId ) const
{
  return mMapLayers.value( layerId );
}

QList<QgsMapLayer *> QgsProject::mapLayersByName( const QString &layerName ) const
{
  QList<QgsMapLayer *> myResultList;
  Q_FOREACH ( QgsMapLayer *layer, mMapLayers )
  {
    if ( layer->name() == layerName )
    {
      myResultList << layer;
    }
  }
  return myResultList;
}

QList<QgsMapLayer *> QgsProject::addMapLayers(
  const QList<QgsMapLayer *> &layers,
  bool addToLegend,
  bool takeOwnership )
{
  QList<QgsMapLayer *> myResultList;
  Q_FOREACH ( QgsMapLayer *myLayer, layers )
  {
    if ( !myLayer || !myLayer->isValid() )
    {
      QgsDebugMsg( "Cannot add invalid layers" );
      continue;
    }
    //check the layer is not already registered!
    if ( !mMapLayers.contains( myLayer->id() ) )
    {
      mMapLayers[myLayer->id()] = myLayer;
      myResultList << mMapLayers[myLayer->id()];
      if ( takeOwnership )
      {
        myLayer->setParent( this );
      }
      connect( myLayer, &QObject::destroyed, this, &QgsProject::onMapLayerDeleted );
      emit layerWasAdded( myLayer );
    }
  }
  if ( !myResultList.isEmpty() )
  {
    emit layersAdded( myResultList );

    if ( addToLegend )
      emit legendLayersAdded( myResultList );
  }
  return myResultList;
}

QgsMapLayer *
QgsProject::addMapLayer( QgsMapLayer *layer,
                         bool addToLegend,
                         bool takeOwnership )
{
  QList<QgsMapLayer *> addedLayers;
  addedLayers = addMapLayers( QList<QgsMapLayer *>() << layer, addToLegend, takeOwnership );
  return addedLayers.isEmpty() ? nullptr : addedLayers[0];
}

void QgsProject::removeMapLayers( const QStringList &layerIds )
{
  QList<QgsMapLayer *> layers;
  Q_FOREACH ( const QString &myId, layerIds )
  {
    layers << mMapLayers.value( myId );
  }

  removeMapLayers( layers );
}

void QgsProject::removeMapLayers( const QList<QgsMapLayer *> &layers )
{
  if ( layers.isEmpty() )
    return;

  QStringList layerIds;
  QList<QgsMapLayer *> layerList;

  Q_FOREACH ( QgsMapLayer *layer, layers )
  {
    // check layer and the registry contains it
    if ( layer && mMapLayers.contains( layer->id() ) )
    {
      layerIds << layer->id();
      layerList << layer;
    }
  }

  if ( layerIds.isEmpty() )
    return;

  emit layersWillBeRemoved( layerIds );
  emit layersWillBeRemoved( layerList );

  Q_FOREACH ( QgsMapLayer *lyr, layerList )
  {
    QString myId( lyr->id() );
    emit layerWillBeRemoved( myId );
    emit layerWillBeRemoved( lyr );
    mMapLayers.remove( myId );
    if ( lyr->parent() == this )
    {
      delete lyr;
    }
    emit layerRemoved( myId );
  }

  emit layersRemoved( layerIds );
}

void QgsProject::removeMapLayer( const QString &layerId )
{
  removeMapLayers( QList<QgsMapLayer *>() << mMapLayers.value( layerId ) );
}

void QgsProject::removeMapLayer( QgsMapLayer *layer )
{
  if ( layer )
    removeMapLayers( QList<QgsMapLayer *>() << layer );
}

void QgsProject::removeAllMapLayers()
{
  emit removeAll();
  // now let all observers know to clear themselves,
  // and then consequently any of their map legends
  removeMapLayers( mMapLayers.keys() );
  mMapLayers.clear();
}

void QgsProject::reloadAllLayers()
{
  Q_FOREACH ( QgsMapLayer *layer, mMapLayers )
  {
    layer->reload();
  }
}

void QgsProject::onMapLayerDeleted( QObject *obj )
{
  QString id = mMapLayers.key( static_cast<QgsMapLayer *>( obj ) );

  if ( !id.isNull() )
  {
    QgsDebugMsg( QString( "Map layer deleted without unregistering! %1" ).arg( id ) );
    mMapLayers.remove( id );
  }
}

QMap<QString, QgsMapLayer *> QgsProject::mapLayers() const
{
  return mMapLayers;
}
