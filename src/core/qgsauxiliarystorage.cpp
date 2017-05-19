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

#include <QTemporaryDir>

#include <sqlite3.h>
#include <spatialite.h>

QgsAuxiliaryStorage::QgsAuxiliaryStorage( const QString &filename, const QgsVectorLayer &layer, const QString &table ):
  QgsVectorLayer( QString( "dbname='%1' table='%2' sql=" ).arg( filename, table ), QString( "%1-auxiliary-storage" ).arg( layer.name() ), "spatialite" )
{
  // add features
  const QgsFeatureIds ids = layer.allFeatureIds();
  QgsFeatureIds::const_iterator it = ids.constBegin();

  startEditing();
  for ( ; it != ids.constEnd(); ++it )
  {
    QgsAttributes attrs( 1 );
    attrs[0] = QVariant( *it );
    QgsFeature f( *it );
    f.setAttributes( attrs );
  }
  commitChanges();
}

QgsAuxiliaryStorage::~QgsAuxiliaryStorage()
{
}

QgsAuxiliaryStorage *QgsAuxiliaryStorage::create( const QgsVectorLayer &layer )
{
  QgsAuxiliaryStorage *storage = nullptr;
  QTemporaryDir tmpDir;
  tmpDir.setAutoRemove( false );

  QString dbFileName = QString( "%1.as" ).arg( layer.name() );
  QString dbPath = QDir( tmpDir.path() ).absoluteFilePath( dbFileName );
  QString table( "data_defined_property" );
  QFile dbFile( dbPath );

  if ( dbFile.open( QIODevice::ReadWrite ) )
  {
    dbFile.close();

    if ( createDB( dbPath, table ) )
    {
      storage = new QgsAuxiliaryStorage( dbPath, layer, table );
    }
  }

  return storage;
}

bool QgsAuxiliaryStorage::createDB( const QString &filename, const QString &table )
{
  QString errMsg;
  char *msg = nullptr;
  sqlite3 *sqlite_handle = nullptr;
  int rc;

  // create database
  rc = QgsSLConnect::sqlite3_open_v2( filename.toUtf8().constData(), &sqlite_handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr );
  if ( rc )
  {
    errMsg = tr( "Could not create a new database\n" );
    errMsg += QString::fromUtf8( sqlite3_errmsg( sqlite_handle ) );
    goto err;
  }

  // activating Foreign Key constraints
  rc = sqlite3_exec( sqlite_handle, "PRAGMA foreign_keys = 1", nullptr, nullptr, &msg );
  if ( rc != SQLITE_OK )
  {
    errMsg = tr( "Unable to activate FOREIGN_KEY constraints [%1]" ).arg( msg );
    sqlite3_free( msg );
    goto err;
  }

  // init spatial metadata
  rc = initializeSpatialMetadata( sqlite_handle, errMsg );
  if ( !rc )
  {
    goto err;
  }

  rc = createDataDefinedPropertyTable( table, sqlite_handle, errMsg );
  if ( !rc )
  {
    goto err;
  }

  QgsSLConnect::sqlite3_close( sqlite_handle );
  return true;

err:
  QgsSLConnect::sqlite3_close( sqlite_handle );
  QgsDebugMsg( errMsg );
  return false;
}

bool QgsAuxiliaryStorage::initializeSpatialMetadata( sqlite3 *sqlite_handle, QString &err )
{
  char **results = nullptr;
  int rows, columns;
  char *errMsg = nullptr;
  bool above41 = false;
  int rc;
  int count = 0;

  QString sql = "select count(*) from sqlite_master";
  rc = sqlite3_get_table( sqlite_handle, sql.toStdString().c_str(), &results, &rows, &columns, nullptr );
  if ( rc != SQLITE_OK )
  {
    err = tr( "Unable to execute '%1'" ).arg( sql );
    return false;
  }

  if ( rows >= 1 )
  {
    for ( int i = 1; i <= rows; i++ )
      count = atoi( results[( i * columns ) + 0] );
  }

  sqlite3_free_table( results );

  if ( count > 0 )
  {
    err = tr( "Invalid count" );
    return false;
  }

  rc = sqlite3_get_table( sqlite_handle, "select spatialite_version()", &results, &rows, &columns, nullptr );
  if ( rc == SQLITE_OK && rows == 1 && columns == 1 )
  {
    QString version = QString::fromUtf8( results[1] );
    QStringList parts = version.split( ' ', QString::SkipEmptyParts );
    if ( parts.size() >= 1 )
    {
      QStringList verparts = parts[0].split( '.', QString::SkipEmptyParts );
      above41 = verparts.size() >= 2 && ( verparts[0].toInt() > 4 || ( verparts[0].toInt() == 4 && verparts[1].toInt() >= 1 ) );
    }
  }

  sqlite3_free_table( results );

  rc = sqlite3_exec( sqlite_handle, above41 ? "SELECT InitSpatialMetadata(1)" : "SELECT InitSpatialMetadata()", nullptr, nullptr, &errMsg );
  if ( rc != SQLITE_OK )
  {
    err = tr( "Unable to initialize SpatialMetadata:\n" );
    err += QString::fromUtf8( errMsg );
    sqlite3_free( errMsg );
    return false;
  }

  spatial_ref_sys_init( sqlite_handle, 0 );

  return true;
}

bool QgsAuxiliaryStorage::createDataDefinedPropertyTable( const QString &table, sqlite3 *sqlite_handle, QString &err )
{
  QString sql = QString( "CREATE TABLE '%1' ( 'ID' int64 )" ).arg( table );
  char *errMsg = nullptr;
  int rc = sqlite3_exec( sqlite_handle, sql.toUtf8(), nullptr, nullptr, &errMsg );
  if ( rc != SQLITE_OK )
  {
    err = tr( "Unable to create data defined property table:\n" );
    err += QString::fromUtf8( errMsg );
    sqlite3_free( errMsg );
    return false;
  }

  return true;
}
