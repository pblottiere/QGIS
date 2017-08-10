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

#include <QTemporaryDir>
#include <QVariant>
#include <QFile>

#include <sqlite3.h>
#include <spatialite.h>

const QString AS_PKFIELD = "ASPK";
const QString AS_EXTENSION = "qgd";

QgsAuxiliaryStorageJoin::QgsAuxiliaryStorageJoin( const QString &pkField, const QString &filename, const QString &table, QgsVectorLayer *layer, bool exist ):
  QgsVectorLayer( QString( "dbname='%1' table='%2' key='%3'" ).arg( filename, table, AS_PKFIELD ), QString( "%1_auxiliarystorage" ).arg( table ), "spatialite" )
  , mLayer( layer )
{

  // add features if necessary
  if ( ! exist )
  {
    QgsFeature f;
    QgsFeatureIterator it = layer->getFeatures();

    startEditing();
    while ( it.nextFeature( f ) )
    {
      QgsAttributes attrs( 1 );
      attrs[0] = QVariant( f.attribute( pkField ) );
      QgsFeature newf;
      newf.setAttributes( attrs );
      addFeature( newf );
    }
    commitChanges();
  }

  // init join info
  mJoinInfo.setJoinLayer( this );
  mJoinInfo.setJoinFieldName( AS_PKFIELD );
  mJoinInfo.setTargetFieldName( pkField );
  mJoinInfo.setEditable( true );
  mJoinInfo.setUpsertOnEdit( true );
  mJoinInfo.setCascadedDelete( true );
}

QgsAuxiliaryStorageJoin::~QgsAuxiliaryStorageJoin()
{
}

QgsVectorLayerJoinInfo QgsAuxiliaryStorageJoin::joinInfo() const
{
  return mJoinInfo;
}

QgsAuxiliaryStorage::QgsAuxiliaryStorage( const QgsProject &project )
  : mValid( false )
  , mTmp( false )
  , mFileName( QString() )
{
  QFileInfo info = project.fileInfo();
  QString path = info.path() + QDir::separator() + info.baseName();
  QString asFileName = path + "." + QgsAuxiliaryStorage::extension();

  sqlite3 *handler = open( asFileName );
  close( handler );
}

QgsAuxiliaryStorage::QgsAuxiliaryStorage( const QString &filename )
  : mValid( false )
  , mTmp( false )
  , mFileName( QString() )
{
  sqlite3 *handler = open( filename );
  close( handler );
}

QgsAuxiliaryStorage::~QgsAuxiliaryStorage()
{
  if ( mTmp )
    QFile::remove( mFileName );
}

bool QgsAuxiliaryStorage::isValid() const
{
  return mValid;
}

QString QgsAuxiliaryStorage::fileName() const
{
  return mFileName;
}

bool QgsAuxiliaryStorage::isNew() const
{
  return mTmp;
}

void QgsAuxiliaryStorage::saveAs( const QString &filename ) const
{
  QFile::copy( mFileName, filename );
}

void QgsAuxiliaryStorage::saveAs( const QgsProject &project ) const
{
  saveAs( filenameForProject( project ) );
}

QString QgsAuxiliaryStorage::extension()
{
  return AS_EXTENSION;
}

QgsAuxiliaryStorageJoin *QgsAuxiliaryStorage::createStorageLayer( QgsVectorLayer *layer )
{
  QgsAuxiliaryStorageJoin *asl = nullptr;

  QgsAttributeList pks = layer->dataProvider()->pkAttributeIndexes();
  if ( !pks.isEmpty() )
  {
    // the first primary key field is used for joining
    asl = createStorageLayer( layer->fields().field( pks[0] ), layer );
  }

  return asl;
}

QgsAuxiliaryStorageJoin *QgsAuxiliaryStorage::createStorageLayer( const QgsField &field, QgsVectorLayer *layer )
{
  QgsAuxiliaryStorageJoin *asl = nullptr;

  if ( mValid )
  {
    bool tmp = mTmp;
    QString table( layer->id() );
    sqlite3 *handler = open( mFileName );

    bool exist = tableExists( table, handler );
    if ( !exist )
    {
      if ( !createTable( field.typeName(), table, handler ) )
      {
        close( handler );
        return asl;
      }
    }

    asl = new QgsAuxiliaryStorageJoin( field.name(), mFileName, table, layer, exist );
    close( handler );
    mTmp = tmp;
  }

  return asl;
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

  // init spatial metadata
  rc = initializeSpatialMetadata( handler );
  if ( !rc )
    return handler;

  return handler;
}

bool QgsAuxiliaryStorage::initializeSpatialMetadata( sqlite3 *handler )
{
  char **res = nullptr;
  int rows, cols;
  bool above41 = false;
  int rc;
  int count = 0;

  // count
  QString sql = "select count(*) from sqlite_master";
  rc = sqlite3_get_table( handler, sql.toStdString().c_str(), &res, &rows, &cols, nullptr );
  if ( rc != SQLITE_OK )
  {
    debugMsg( sql, handler );
    return false;
  }

  if ( rows >= 1 )
  {
    for ( int i = 1; i <= rows; i++ )
      count = atoi( res[( i * cols ) + 0] );
  }

  sqlite3_free_table( res );

  if ( count > 0 )
  {
    QgsDebugMsg( QObject::tr( "Invalid count" ) );
    return false;
  }

  // select spatialite version
  sql = "select spatialite_version()";
  rc = sqlite3_get_table( handler, sql.toStdString().c_str(), &res, &rows, &cols, nullptr );
  if ( rc == SQLITE_OK && rows == 1 && cols == 1 )
  {
    QString version = QString::fromUtf8( res[1] );
    QStringList parts = version.split( ' ', QString::SkipEmptyParts );
    if ( parts.size() >= 1 )
    {
      QStringList verparts = parts[0].split( '.', QString::SkipEmptyParts );
      above41 = verparts.size() >= 2 && ( verparts[0].toInt() > 4 || ( verparts[0].toInt() == 4 && verparts[1].toInt() >= 1 ) );
    }
  }
  else
  {
    debugMsg( sql, handler );
    return false;
  }

  sqlite3_free_table( res );

  // init spatial metadata according to the current spatialite version
  sql = "SELECT InitSpatialMetadata()";
  if ( above41 )
    sql = "SELECT InitSpatialMetadata(1)" ;

  if ( !exec( sql, handler ) )
    return false;

  spatial_ref_sys_init( handler, 0 );

  return true;
}

bool QgsAuxiliaryStorage::createTable( const QString &type, const QString &table, sqlite3 *handler )
{
  QString sql = QString( "CREATE TABLE IF NOT EXISTS '%1' ( '%2' %3 PRIMARY KEY )" ).arg( table ).arg( AS_PKFIELD ).arg( type );

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
    QTemporaryFile tmpFile;
    tmpFile.setAutoRemove( false );
    tmpFile.open();
    tmpFile.close();

    if ( ( handler = createDB( tmpFile.fileName() ) ) )
    {
      mTmp = true;
      mValid = true;
      mFileName = tmpFile.fileName();
    }
  }
  else if ( QFile::exists( filename ) )
  {
    if ( ( handler = openDB( filename ) ) )
    {
      mTmp = false;
      mValid = true;
      mFileName = filename;
    }
  }
  else
  {
    if ( ( handler = createDB( filename ) ) )
    {
      mTmp = false;
      mValid = true;
      mFileName = filename;
    }
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
