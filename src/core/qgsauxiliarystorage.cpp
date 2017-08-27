/***************************************************************************
                          qgsauxiliarystorage.cpp  -  description
                            -------------------
    begin                : May 18, 2017
    copyright            : (C) 2017 by Paul Blottiere
    email                : paul.blottiere@oslandia.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsauxiliarystorage.h"
#include "qgslogger.h"
#include "qgsslconnect.h"
#include "qgsvectordataprovider.h"
#include "qgsproject.h"
#include "qgspallabeling.h"
#include "qgsdiagramrenderer.h"
#include "qgsmemoryproviderutils.h"

#include <QVariant>
#include <QFile>

#include <sqlite3.h>
#include <spatialite.h>

const QString AS_PKFIELD = "ROWID";
const QString AS_JOINFIELD = "ASPK";
const QString AS_EXTENSION = "qgd";
const QString AS_JOINPREFIX = "auxiliary_storage_";

QgsAuxiliaryField::QgsAuxiliaryField( const QgsPropertyDefinition &def )
  : QgsField()
  , mPropertyDefinition( def )
{
  init( def );
}

QgsAuxiliaryField::QgsAuxiliaryField( const QgsField &f )
{
  QStringList parts = f.name().split( '_' );

  if ( parts.size() <= 1 )
    return;

  QString target = parts[0];
  QString propertyName = parts[1];
  QgsPropertyDefinition def;

  if ( target.compare( "pal", Qt::CaseInsensitive ) == 0 )
  {
    QgsPropertiesDefinition props = QgsPalLayerSettings::propertyDefinitions();
    Q_FOREACH ( const QgsPropertyDefinition p, props.values() )
    {
      if ( p.name().compare( propertyName, Qt::CaseInsensitive ) == 0 )
      {
        def = p;
        break;
      }
    }
  }
  else if ( target.compare( "diagram", Qt::CaseInsensitive ) == 0 )
  {
    QgsPropertiesDefinition props = QgsDiagramLayerSettings::propertyDefinitions();
    Q_FOREACH ( const QgsPropertyDefinition p, props.values() )
    {
      if ( p.name().compare( propertyName, Qt::CaseInsensitive ) == 0 )
      {
        def = p;
        break;
      }
    }
  }

  if ( !def.name().isEmpty() )
  {
    init( def );
    setTypeName( f.typeName() );
    mPropertyDefinition = def;
  }
}

void QgsAuxiliaryField::init( const QgsPropertyDefinition &def )
{
  if ( !def.name().isEmpty() )
  {
    QVariant::Type type;
    int len( 0 ), precision( 0 );
    switch ( def.dataType() )
    {
      case QgsPropertyDefinition::DataTypeString:
        type = QVariant::String;
        len = 50;
        break;
      case QgsPropertyDefinition::DataTypeNumeric:
        type = QVariant::Double;
        len = 0;
        precision = 0;
        break;
      case QgsPropertyDefinition::DataTypeBoolean:
        type = QVariant::Int; // sqlite does not have a bool type
        break;
      default:
        break;
    }

    setType( type );
    setName( name( def ) );
    setLength( len );
    setPrecision( precision );
  }
}

QString QgsAuxiliaryField::name( const QgsPropertyDefinition &def, bool joined )
{
  QString target;
  switch ( def.target() )
  {
    case QgsPropertyDefinition::Pal:
      target = "pal";
      break;
    case QgsPropertyDefinition::Diagram:
      target = "diagram";
      break;
    default:
      break;
  }

  QString fieldName = QString( "%2_%3" ).arg( target, def.name().toLower() );

  if ( joined )
    fieldName = QString( "%1%2" ).arg( AS_JOINPREFIX, fieldName );

  return fieldName;
}

//
// QgsAuxiliaryLayer
//

QgsAuxiliaryLayer::QgsAuxiliaryLayer( const QString &pkField, const QString &filename, const QString &table, const QgsVectorLayer *vlayer ):
  QgsVectorLayer( QString( "%1|layername=%2" ).arg( filename, table, AS_PKFIELD ), QString( "%1_auxiliarystorage" ).arg( table ), "ogr" )
  , mLayer( vlayer )
{
  // init join info
  mJoinInfo.setPrefix( AS_JOINPREFIX );
  mJoinInfo.setJoinLayer( this );
  mJoinInfo.setJoinFieldName( AS_JOINFIELD );
  mJoinInfo.setTargetFieldName( pkField );
  mJoinInfo.setEditable( true );
  mJoinInfo.setUpsertOnEdit( true );
  mJoinInfo.setCascadedDelete( true );
  mJoinInfo.setAuxiliaryStorage( true );
  mJoinInfo.setJoinFieldNamesBlackList( QStringList() << "rowid" ); // introduced by ogr provider
}

QgsVectorLayer *QgsAuxiliaryLayer::toSpatialLayer() const
{
  QgsVectorLayer *layer = QgsMemoryProviderUtils::createMemoryLayer( QStringLiteral( "auxiliary_layer" ), fields(), mLayer->wkbType(), mLayer->crs() );

  QString pkField = mJoinInfo.targetFieldName();
  QgsFeature joinFeature;
  QgsFeature targetFeature;
  QgsFeatureIterator it = getFeatures();

  layer->startEditing();
  while ( it.nextFeature( joinFeature ) )
  {
    QString filter = QgsExpression::createFieldEqualityExpression( pkField, joinFeature.attribute( AS_JOINFIELD ) );

    QgsFeatureRequest request;
    request.setFilterExpression( filter );

    mLayer->getFeatures( request ).nextFeature( targetFeature );

    if ( targetFeature.isValid() )
    {
      QgsFeature newFeature( joinFeature );
      newFeature.setGeometry( targetFeature.geometry() );
      layer->addFeature( newFeature );
    }
  }
  layer->commitChanges();

  return layer;
}

QgsPropertyDefinition QgsAuxiliaryField::propertyDefinition() const
{
  return mPropertyDefinition;
}

QgsVectorLayerJoinInfo QgsAuxiliaryLayer::joinInfo() const
{
  return mJoinInfo;
}

bool QgsAuxiliaryLayer::addAuxiliaryField( const QgsPropertyDefinition &definition )
{
  if ( definition.name().isEmpty() || exists( definition ) )
    return false;

  QgsAuxiliaryField af( definition );
  bool rc = addAttribute( af );
  updateFields();

  return rc;
}

bool QgsAuxiliaryLayer::exists( const QgsPropertyDefinition &definition ) const
{
  return ( fields().indexOf( QgsAuxiliaryField::name( definition ) ) >= 0 );
}

QgsAuxiliaryFields QgsAuxiliaryLayer::auxiliaryFields() const
{
  QgsAuxiliaryFields afields;

  for ( int i = 2; i < fields().count(); i++ ) // ignore rowid and PK field
    afields.append( QgsAuxiliaryField( fields().field( i ) ) );

  return afields;
}

bool QgsAuxiliaryLayer::clear()
{
  bool rc = deleteFeatures( allFeatureIds() );
  commitChanges();
  startEditing();
  return rc;
}

bool QgsAuxiliaryLayer::deleteAttribute( int attr )
{
  QgsVectorLayer::deleteAttribute( attr );
  bool rc = commitChanges();
  startEditing();
  return rc;
}

//
// QgsAuxiliaryStorage
//

QgsAuxiliaryStorage::QgsAuxiliaryStorage( const QgsProject &project, bool copy )
  : mValid( false )
  , mFileName( QString() )
  , mTmpFileName( QString() )
  , mCopy( copy )
{
  initTmpFileName();

  QFileInfo info = project.fileInfo();
  QString path = info.path() + QDir::separator() + info.baseName();
  QString asFileName = path + "." + QgsAuxiliaryStorage::extension();
  mFileName = asFileName;

  sqlite3 *handler = open( asFileName );
  close( handler );
}

QgsAuxiliaryStorage::QgsAuxiliaryStorage( const QString &filename, bool copy )
  : mValid( false )
  , mFileName( filename )
  , mTmpFileName( QString() )
  , mCopy( copy )
{
  initTmpFileName();

  sqlite3 *handler = open( filename );
  close( handler );
}

QgsAuxiliaryStorage::~QgsAuxiliaryStorage()
{
  QFile::remove( mTmpFileName );
}

bool QgsAuxiliaryStorage::isValid() const
{
  return mValid;
}

QString QgsAuxiliaryStorage::fileName() const
{
  return mFileName;
}

bool QgsAuxiliaryStorage::save() const
{
  if ( mFileName.isEmpty() )
  {
    // only a saveAs is available on a new database
    return false;
  }
  else if ( mCopy )
  {
    if ( QFile::exists( mFileName ) )
      QFile::remove( mFileName );

    return QFile::copy( mTmpFileName, mFileName );
  }
  else
  {
    // if the file is not empty the copy mode is not activated, then we're
    // directly working on the database since the beginning (no savepoints
    // /rollback for now)
    return true;
  }
}

bool QgsAuxiliaryStorage::saveAs( const QString &filename ) const
{
  if ( QFile::exists( filename ) )
    QFile::remove( filename );

  return  QFile::copy( currentFileName(), filename );
}

bool QgsAuxiliaryStorage::saveAs( const QgsProject &project ) const
{
  return saveAs( filenameForProject( project ) );
}

QString QgsAuxiliaryStorage::extension()
{
  return AS_EXTENSION;
}

QgsAuxiliaryLayer *QgsAuxiliaryStorage::createAuxiliaryLayer( const QgsVectorLayer *layer )
{
  QgsAuxiliaryLayer *alayer = nullptr;

  QgsAttributeList pks = layer->dataProvider()->pkAttributeIndexes();
  if ( !pks.isEmpty() )
  {
    // the first primary key field is used for joining
    alayer = createAuxiliaryLayer( layer->fields().field( pks[0] ), layer );
  }

  return alayer;
}

QgsAuxiliaryLayer *QgsAuxiliaryStorage::createAuxiliaryLayer( const QgsField &field, const QgsVectorLayer *layer )
{
  QgsAuxiliaryLayer *alayer = nullptr;

  if ( mValid && layer && layer->isSpatial() )
  {
    QString table( layer->id() );
    sqlite3 *handler = open( currentFileName() );

    if ( !tableExists( table, handler ) )
    {
      if ( !createTable( field.typeName(), table, handler ) )
      {
        close( handler );
        return alayer;
      }
    }

    alayer = new QgsAuxiliaryLayer( field.name(), currentFileName(), table, layer );
    close( handler );
  }

  return alayer;
}

bool QgsAuxiliaryStorage::deleteTable( const QgsDataSourceUri &uri )
{
  bool rc = false;

  // parsing for ogr style uri :
  // " filePath|layername='tableName' table="" sql="
  QStringList uriParts = uri.uri().split( '|' );
  if ( uriParts.count() < 2 )
    return false;

  QString databasePath = uriParts[0].replace( ' ', "" );

  QString table = uriParts[1];
  QStringList tableParts = table.split( ' ' );

  if ( tableParts.count() < 1 )
    return false;

  QString tableName = tableParts[0].replace( "layername=", "" );

  if ( !databasePath.isEmpty() && !tableName.isEmpty() )
  {
    sqlite3 *handler = openDB( databasePath );

    if ( handler )
    {
      QString sql = QString( "DROP TABLE %1" ).arg( tableName );
      rc = exec( sql, handler );

      sql = QString( "VACUUM" );
      rc = exec( sql, handler );

      close( handler );
    }
  }

  return rc;
}

bool QgsAuxiliaryStorage::exec( const QString &sql, sqlite3 *handler )
{
  bool rc = false;

  if ( handler )
  {
    int err = sqlite3_exec( handler, sql.toStdString().c_str(), nullptr, nullptr, nullptr );

    if ( err == SQLITE_OK )
      rc = true;
    else
      debugMsg( sql, handler );
  }

  return rc;
}

void QgsAuxiliaryStorage::debugMsg( const QString &sql, sqlite3 *handler )
{
  QString err = QString::fromUtf8( sqlite3_errmsg( handler ) );
  QString msg = QObject::tr( "Unable to execute" );
  QString errMsg = QObject::tr( "%1 '%2': %3" ).arg( msg ).arg( sql ).arg( err );
  QgsDebugMsg( errMsg );
}

sqlite3 *QgsAuxiliaryStorage::openDB( const QString &filename )
{
  sqlite3 *handler = nullptr;

  bool rc = QgsSLConnect::sqlite3_open_v2( filename.toUtf8().constData(), &handler, SQLITE_OPEN_READWRITE, nullptr );
  if ( rc )
  {
    debugMsg( "sqlite3_open_v2", handler );
    return nullptr;
  }

  return handler;
}

sqlite3 *QgsAuxiliaryStorage::createDB( const QString &filename )
{
  sqlite3 *handler = nullptr;
  int rc;

  // open/create database
  rc = QgsSLConnect::sqlite3_open_v2( filename.toUtf8().constData(), &handler, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr );
  if ( rc )
  {
    debugMsg( "sqlite3_open_v2", handler );
    return handler;
  }

  // activating Foreign Key constraints
  if ( !exec( "PRAGMA foreign_keys = 1", handler ) )
    return handler;

  return handler;
}

bool QgsAuxiliaryStorage::createTable( const QString &type, const QString &table, sqlite3 *handler )
{
  QString sql = QString( "CREATE TABLE IF NOT EXISTS '%1' ( '%2' %3  )" ).arg( table ).arg( AS_JOINFIELD ).arg( type );

  if ( !exec( sql, handler ) )
    return false;

  return true;
}

bool QgsAuxiliaryStorage::tableExists( const QString &table, sqlite3 *handler )
{
  QString sql = QString( "SELECT 1 FROM sqlite_master WHERE type='table' AND name='%1'" ).arg( table );
  int rows = 0;
  int columns = 0;
  char **results = nullptr;
  int rc = sqlite3_get_table( handler, sql.toStdString().c_str(), &results, &rows, &columns, nullptr );
  if ( rc != SQLITE_OK )
  {
    debugMsg( sql, handler );
    return false;
  }

  sqlite3_free_table( results );
  if ( rows >= 1 )
    return true;

  return false;
}

sqlite3 *QgsAuxiliaryStorage::open( const QString &filename )
{
  sqlite3 *handler = nullptr;

  if ( filename.isEmpty() )
  {
    if ( ( handler = createDB( currentFileName() ) ) )
      mValid = true;
  }
  else if ( QFile::exists( filename ) )
  {
    if ( mCopy )
      QFile::copy( filename, mTmpFileName );

    if ( ( handler = openDB( currentFileName() ) ) )
      mValid = true;
  }
  else
  {
    if ( ( handler = createDB( currentFileName() ) ) )
      mValid = true;
  }

  return handler;
}

sqlite3 *QgsAuxiliaryStorage::open( const QgsProject &project )
{
  return open( filenameForProject( project ) );
}

void QgsAuxiliaryStorage::close( sqlite3 *handler )
{
  if ( handler )
  {
    QgsSLConnect::sqlite3_close_v2( handler );
    handler = nullptr;
  }
}

QString QgsAuxiliaryStorage::filenameForProject( const QgsProject &project )
{
  QFileInfo info = project.fileInfo();
  QString path = info.path() + QDir::separator() + info.baseName();
  return path + "." + QgsAuxiliaryStorage::extension();
}

void QgsAuxiliaryStorage::initTmpFileName()
{
  QTemporaryFile tmpFile;
  tmpFile.open();
  tmpFile.close();
  mTmpFileName = tmpFile.fileName();
}

QString QgsAuxiliaryStorage::currentFileName() const
{
  if ( mCopy || mFileName.isEmpty() )
    return mTmpFileName;
  else
    return mFileName;
}
