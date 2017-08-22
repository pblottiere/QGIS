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
#include "qgslabelingenginesettings.h"
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
#include "qgsreadwritecontext.h"
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
#include "qgsmaplayerstore.h"
#include "qgsziputils.h"
#include "qgsauxiliarystorage.h"

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
      QgsMessageLog::logMessage( errorString, QString(), QgsMessageLog::CRITICAL );

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
  , mLayerStore( new QgsMapLayerStore( this ) )
  , mBadLayerHandler( new QgsProjectBadLayerHandler() )
  , mSnappingConfig( this )
  , mRelationManager( new QgsRelationManager( this ) )
  , mAnnotationManager( new QgsAnnotationManager( this ) )
  , mLayoutManager( new QgsLayoutManager( this ) )
  , mRootGroup( new QgsLayerTree )
  , mLabelingEngineSettings( new QgsLabelingEngineSettings )
  , mArchive( new QgsProjectArchive() )
  , mAuxiliaryStorage( new QgsAuxiliaryStorage() )
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

  // proxy map layer store signals to this
  connect( mLayerStore.get(), static_cast<void ( QgsMapLayerStore::* )( const QStringList & )>( &QgsMapLayerStore::layersWillBeRemoved ),
           this, static_cast<void ( QgsProject::* )( const QStringList & )>( &QgsProject::layersWillBeRemoved ) );
  connect( mLayerStore.get(), static_cast<void ( QgsMapLayerStore::* )( const QList<QgsMapLayer *> & )>( &QgsMapLayerStore::layersWillBeRemoved ),
           this, static_cast<void ( QgsProject::* )( const QList<QgsMapLayer *> & )>( &QgsProject::layersWillBeRemoved ) );
  connect( mLayerStore.get(), static_cast<void ( QgsMapLayerStore::* )( const QString & )>( &QgsMapLayerStore::layerWillBeRemoved ),
           this, static_cast<void ( QgsProject::* )( const QString & )>( &QgsProject::layerWillBeRemoved ) );
  connect( mLayerStore.get(), static_cast<void ( QgsMapLayerStore::* )( QgsMapLayer * )>( &QgsMapLayerStore::layerWillBeRemoved ),
           this, static_cast<void ( QgsProject::* )( QgsMapLayer * )>( &QgsProject::layerWillBeRemoved ) );
  connect( mLayerStore.get(), static_cast<void ( QgsMapLayerStore::* )( const QStringList & )>( &QgsMapLayerStore::layersRemoved ), this, &QgsProject::layersRemoved );
  connect( mLayerStore.get(), &QgsMapLayerStore::layerRemoved, this, &QgsProject::layerRemoved );
  connect( mLayerStore.get(), &QgsMapLayerStore::allLayersRemoved, this, &QgsProject::removeAll );
  connect( mLayerStore.get(), &QgsMapLayerStore::layersAdded, this, &QgsProject::layersAdded );
  connect( mLayerStore.get(), &QgsMapLayerStore::layerWasAdded, this, &QgsProject::layerWasAdded );
}


QgsProject::~QgsProject()
{
  clear();
  delete mBadLayerHandler;
  delete mRelationManager;
  delete mLayerTreeRegistryBridge;
  delete mRootGroup;
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

  mArchive->clear();

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
  emit ellipsoidChanged( ellipsoid );
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

  mLabelingEngineSettings->clear();

  mArchive->clear();

  emit labelingEngineSettingsChanged();

  // reset some default project properties
  // XXX THESE SHOULD BE MOVED TO STATUSBAR RELATED SOURCE
  writeEntry( QStringLiteral( "PositionPrecision" ), QStringLiteral( "/Automatic" ), true );
  writeEntry( QStringLiteral( "PositionPrecision" ), QStringLiteral( "/DecimalPlaces" ), 2 );
  writeEntry( QStringLiteral( "Paths" ), QStringLiteral( "/Absolute" ), false );

  //copy default units to project
  QgsSettings s;
  writeEntry( QStringLiteral( "Measurement" ), QStringLiteral( "/DistanceUnits" ), s.value( QStringLiteral( "/qgis/measure/displayunits" ) ).toString() );
  writeEntry( QStringLiteral( "Measurement" ), QStringLiteral( "/AreaUnits" ), s.value( QStringLiteral( "/qgis/measure/areaunits" ) ).toString() );

  removeAllMapLayers();
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

  QDomNode titleNode = nl.item( 0 );  // there should only be one, so zeroth element OK

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

  QDomNode qgisNode = nl.item( 0 );  // there should only be one, so zeroth element OK

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
      QgsReadWriteContext context;
      context.setPathResolver( pathResolver() );
      if ( !addLayer( element, brokenNodes, context ) )
      {
        returnStatus = false;
      }
    }
    emit layerLoaded( i + 1, nl.count() );
    i++;
  }

  return returnStatus;
}

bool QgsProject::addLayer( const QDomElement &layerElem, QList<QDomNode> &brokenNodes, const QgsReadWriteContext &context )
{
  QString type = layerElem.attribute( QStringLiteral( "type" ) );
  QgsDebugMsgLevel( "Layer type is " + type, 4 );
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
  if ( mapLayer->readLayerXml( layerElem, context ) && mapLayer->isValid() )
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
  QString filename = mFile.fileName();
  bool rc;

  if ( QgsZipUtils::isZipFile( mFile.fileName() ) )
  {
    rc = unzip( mFile.fileName() );
  }
  else
  {
    mAuxiliaryStorage.reset( new QgsAuxiliaryStorage( *this ) );
    rc = readProjectFile( mFile.fileName() );
  }

  mFile.setFileName( filename );
  return rc;
}

bool QgsProject::readProjectFile( const QString &filename )
{
  QFile projectFile( filename );
  clearError();

  std::unique_ptr<QDomDocument> doc( new QDomDocument( QStringLiteral( "qgis" ) ) );

  if ( !projectFile.open( QIODevice::ReadOnly | QIODevice::Text ) )
  {
    projectFile.close();

    setError( tr( "Unable to open %1" ).arg( projectFile.fileName() ) );

    return false;
  }

  // location of problem associated with errorMsg
  int line, column;
  QString errorMsg;

  if ( !doc->setContent( &projectFile, &errorMsg, &line, &column ) )
  {
    // want to make this class as GUI independent as possible; so commented out
#if 0
    QMessageBox::critical( 0, tr( "Project File Read Error" ),
                           tr( "%1 at line %2 column %3" ).arg( errorMsg ).arg( line ).arg( column ) );
#endif

    QString errorString = tr( "Project file read error in file %1: %2 at line %3 column %4" )
                          .arg( projectFile.fileName() ).arg( errorMsg ).arg( line ).arg( column );

    QgsDebugMsg( errorString );

    projectFile.close();

    setError( tr( "%1 for file %2" ).arg( errorString, projectFile.fileName() ) );

    return false;
  }

  projectFile.close();


  QgsDebugMsg( "Opened document " + projectFile.fileName() );
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

  QgsReadWriteContext context;
  context.setPathResolver( pathResolver() );

  //crs
  QgsCoordinateReferenceSystem projectCrs;
  if ( readNumEntry( QStringLiteral( "SpatialRefSys" ), QStringLiteral( "/ProjectionsEnabled" ), 0 ) )
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
  QMap<QString, QgsMapLayer *> layers = mLayerStore->mapLayers();
  for ( QMap<QString, QgsMapLayer *>::iterator it = layers.begin(); it != layers.end(); it++ )
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

  mLabelingEngineSettings->readSettingsFromProject( this );
  emit labelingEngineSettingsChanged();

  mAnnotationManager->readXml( doc->documentElement(), context );
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
  emit ellipsoidChanged( ellipsoid() );

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

void QgsProject::setLabelingEngineSettings( const QgsLabelingEngineSettings &settings )
{
  *mLabelingEngineSettings = settings;
  emit labelingEngineSettingsChanged();
}

const QgsLabelingEngineSettings &QgsProject::labelingEngineSettings() const
{
  return *mLabelingEngineSettings;
}

QgsMapLayerStore *QgsProject::layerStore()
{
  return mLayerStore.get();
}

const QgsMapLayerStore *QgsProject::layerStore() const
{
  return mLayerStore.get();
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
  QgsReadWriteContext context;
  context.setPathResolver( pathResolver() );
  QList<QDomNode> brokenNodes;
  if ( addLayer( layerNode.toElement(), brokenNodes, context ) )
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

bool QgsProject::write()
{
  if ( QgsZipUtils::isZipFile( mFile.fileName() ) )
  {
    return zip( mFile.fileName() );
  }
  else
  {
    if ( mAuxiliaryStorage->fileName().isEmpty() ) // new project
      mAuxiliaryStorage->saveAs( *this );
    else
      mAuxiliaryStorage->save();

    return writeProjectFile( mFile.fileName() );
  }
}

bool QgsProject::writeProjectFile( const QString &filename )
{
  QFile projectFile( filename );
  clearError();

  // if we have problems creating or otherwise writing to the project file,
  // let's find out up front before we go through all the hand-waving
  // necessary to create all the Dom objects
  QFileInfo myFileInfo( projectFile );
  if ( myFileInfo.exists() && !myFileInfo.isWritable() )
  {
    setError( tr( "%1 is not writable. Please adjust permissions (if possible) and try again." )
              .arg( projectFile.fileName() ) );
    return false;
  }

  QgsReadWriteContext context;
  context.setPathResolver( pathResolver() );

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

        ml->writeLayerXml( maplayerElem, *doc, context );

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

  mLabelingEngineSettings->writeSettingsToProject( this );

  QDomElement annotationsElem = mAnnotationManager->writeXml( *doc, context );
  qgisNode.appendChild( annotationsElem );

  QDomElement layoutElem = mLayoutManager->writeXml( *doc );
  qgisNode.appendChild( layoutElem );

  // now wrap it up and ship it to the project file
  doc->normalize();             // XXX I'm not entirely sure what this does

  // Create backup file
  if ( QFile::exists( fileName() ) )
  {
    QFile backupFile( QString( "%1~" ).arg( filename ) );
    bool ok = true;
    ok &= backupFile.open( QIODevice::WriteOnly | QIODevice::Truncate );
    ok &= projectFile.open( QIODevice::ReadOnly );

    QByteArray ba;
    while ( ok && !projectFile.atEnd() )
    {
      ba = projectFile.read( 10240 );
      ok &= backupFile.write( ba ) == ba.size();
    }

    projectFile.close();
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

  if ( !projectFile.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
  {
    projectFile.close();         // even though we got an error, let's make
    // sure it's closed anyway

    setError( tr( "Unable to save to file %1" ).arg( projectFile.fileName() ) );
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
      ok &= projectFile.write( ba ) == ba.size();
    }

    ok &= projectFile.error() == QFile::NoError;

    projectFile.close();
  }

  tempFile.close();

  if ( !ok )
  {
    setError( tr( "Unable to save to file %1. Your project "
                  "may be corrupted on disk. Try clearing some space on the volume and "
                  "check file permissions before pressing save again." )
              .arg( projectFile.fileName() ) );
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

  QgsReadWriteContext embeddedContext;
  if ( !useAbsolutePaths )
    embeddedContext.setPathResolver( QgsPathResolver( projectFilePath ) );

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

      if ( addLayer( mapLayerElem, brokenNodes, embeddedContext ) )
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
    return QString();

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

const QgsAnnotationManager *QgsProject::annotationManager() const
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
// QgsMapLayerStore methods
//


int QgsProject::count() const
{
  return mLayerStore->count();
}

QgsMapLayer *QgsProject::mapLayer( const QString &layerId ) const
{
  return mLayerStore->mapLayer( layerId );
}

QList<QgsMapLayer *> QgsProject::mapLayersByName( const QString &layerName ) const
{
  return mLayerStore->mapLayersByName( layerName );
}

bool QgsProject::unzip( const QString &filename )
{
  clearError();
  std::unique_ptr<QgsProjectArchive> archive( new QgsProjectArchive() );

  // unzip the archive
  if ( !archive->unzip( filename ) )
  {
    setError( tr( "Unable to unzip file '%1'" ).arg( filename ) );
    return false;
  }

  // test if zip provides a .qgs file
  if ( archive->projectFile().isEmpty() )
  {
    setError( tr( "Zip archive does not provide a project file" ) );
    return false;
  }

  // load auxiliary storage
  if ( !archive->auxiliaryStorageFile().isEmpty() )
  {
    mAuxiliaryStorage.reset( new QgsAuxiliaryStorage( archive->auxiliaryStorageFile(), false ) );
  }
  else
  {
    mAuxiliaryStorage.reset( new QgsAuxiliaryStorage( *this ) );
  }

  // read the project file
  if ( ! readProjectFile( archive->projectFile() ) )
  {
    setError( tr( "Cannot read unzipped qgs project file" ) );
    return false;
  }

  // keep the archive and remove the temporary .qgs file
  mArchive.reset( archive.release() );
  mArchive->clearProjectFile();

  return true;
}

bool QgsProject::zip( const QString &filename )
{
  clearError();

  // save the current project in a temporary .qgs file
  std::unique_ptr<QgsProjectArchive> archive( new QgsProjectArchive() );
  const QString baseName = QFileInfo( filename ).baseName();
  const QString qgsFileName = QString( "%1.qgs" ).arg( baseName );
  QFile qgsFile( QDir( archive->dir() ).filePath( qgsFileName ) );

  bool writeOk = false;
  if ( qgsFile.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
  {
    writeOk = writeProjectFile( qgsFile.fileName() );
    qgsFile.close();
  }

  // stop here with an error message
  if ( ! writeOk )
  {
    setError( tr( "Unable to write temporary qgs file" ) );
    return false;
  }

  // save auxiliary storage
  QFileInfo info( qgsFile );
  QString asFileName = info.path() + QDir::separator() + info.completeBaseName() + "." + QgsAuxiliaryStorage::extension();

  if ( ! mAuxiliaryStorage->saveAs( asFileName ) )
  {
    setError( tr( "Unable to copy auxiliary storage" ) );
    return false;
  }

  // add files to the archive
  archive->addFile( qgsFile.fileName() );
  archive->addFile( asFileName );

  // zip
  QString errMsg;
  if ( !archive->zip( filename ) )
  {
    setError( tr( "Unable to perform zip" ) );
    return false;
  }

  return true;
}

bool QgsProject::isZipped() const
{
  return QgsZipUtils::isZipFile( mFile.fileName() );
}

QList<QgsMapLayer *> QgsProject::addMapLayers(
  const QList<QgsMapLayer *> &layers,
  bool addToLegend,
  bool takeOwnership )
{
  QList<QgsMapLayer *> myResultList = mLayerStore->addMapLayers( layers, takeOwnership );
  if ( !myResultList.isEmpty() )
  {
    if ( addToLegend )
      emit legendLayersAdded( myResultList );
  }

  Q_FOREACH ( QgsMapLayer *mlayer, myResultList )
  {
    if ( QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( mlayer ) )
    {
      QgsAuxiliaryLayer *alayer = mAuxiliaryStorage->createAuxiliaryLayer( vl );
      vl->setAuxiliaryLayer( alayer );
    }
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
  mLayerStore->removeMapLayers( layerIds );
}

void QgsProject::removeMapLayers( const QList<QgsMapLayer *> &layers )
{
  mLayerStore->removeMapLayers( layers );
}

void QgsProject::removeMapLayer( const QString &layerId )
{
  mLayerStore->removeMapLayer( layerId );
}

void QgsProject::removeMapLayer( QgsMapLayer *layer )
{
  mLayerStore->removeMapLayer( layer );
}

QgsMapLayer *QgsProject::takeMapLayer( QgsMapLayer *layer )
{
  return mLayerStore->takeMapLayer( layer );
}

void QgsProject::removeAllMapLayers()
{
  mLayerStore->removeAllMapLayers();
}

void QgsProject::reloadAllLayers()
{
  QMap<QString, QgsMapLayer *> layers = mLayerStore->mapLayers();
  QMap<QString, QgsMapLayer *>::const_iterator it = layers.constBegin();
  for ( ; it != layers.constEnd(); ++it )
  {
    it.value()->reload();
  }
}

QMap<QString, QgsMapLayer *> QgsProject::mapLayers() const
{
  return mLayerStore->mapLayers();
}

void QgsProject::removeAuxiliaryStorageLayers()
{
  Q_FOREACH ( QgsMapLayer *mlayer, mLayerStore->mapLayers() )
  {
    if ( QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( mlayer ) )
      vl->setAuxiliaryStorageJoin();
  }
}

QgsCoordinateReferenceSystem QgsProject::defaultCrsForNewLayers() const
{
  QgsSettings settings;
  QgsCoordinateReferenceSystem defaultCrs;
  if ( settings.value( QStringLiteral( "/Projections/defaultBehavior" ), QStringLiteral( "prompt" ) ).toString() == QStringLiteral( "useProject" )
       || settings.value( QStringLiteral( "/Projections/defaultBehavior" ), QStringLiteral( "prompt" ) ).toString() == QStringLiteral( "prompt" ) )
  {
    // for new layers if the new layer crs method is set to either prompt or use project, then we use the project crs
    // (since "prompt" has no meaning here - the prompt will always be shown, it's just deciding on the default choice in the prompt!)
    defaultCrs = crs();
  }
  else
  {
    // global crs
    QString layerDefaultCrs = settings.value( QStringLiteral( "/Projections/layerDefaultCrs" ), GEO_EPSG_CRS_AUTHID ).toString();
    defaultCrs = QgsCoordinateReferenceSystem::fromOgcWmsCrs( layerDefaultCrs );
  }

  return defaultCrs;

void QgsProject::reloadAuxiliaryStorageLayers()
{
  Q_FOREACH ( QgsMapLayer *mlayer, mLayerStore->mapLayers() )
  {
    if ( QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( mlayer ) )
    {
      QgsAuxiliaryStorageJoin *asl = mAuxiliaryStorage->createStorageLayer( vl );
      vl->setAuxiliaryStorageJoin( asl );
    }
  }
}
