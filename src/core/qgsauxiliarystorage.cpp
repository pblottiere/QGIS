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

#include <QTemporaryDir>
#include <QVariant>
#include <QFile>

#include <sqlite3.h>
#include <spatialite.h>

QgsAuxiliaryStorageJoin::QgsAuxiliaryStorageJoin( const QString &filename, const QString &table, const QgsVectorLayer &layer, bool yetFilled ):
  QgsVectorLayer( QString( "dbname='%1' table='%2' key='ID'" ).arg( filename, table ), QString( "%1-auxiliary-storage" ).arg( table ), "spatialite" )
{
  if ( ! yetFilled )
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
      addFeature( f );
    }
    commitChanges();
  }
}

QgsAuxiliaryStorageJoin::~QgsAuxiliaryStorageJoin()
{
}

bool QgsAuxiliaryStorageJoin::createProperty( const QgsPropertyDefinition &definition )
{
  if ( propertyExists( definition ) )
    return false;

  QVariant::Type type;
  int len( 0 ), precision( 0 );
  switch ( definition.dataType() )
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

  QgsField field;
  field.setType( type );
  field.setName( definition.name() );
  field.setLength( len );
  field.setPrecision( precision );
  dataProvider()->addAttributes( QList<QgsField>() << field );
  updateFields();

  return true;
}

bool QgsAuxiliaryStorageJoin::propertyExists( const QgsPropertyDefinition &definition ) const
{
  return ( fields().indexOf( definition.name() ) >= 0 );
}

QString QgsAuxiliaryStorageJoin::propertyFieldName( const QgsPropertyDefinition &definition ) const
{
  // joined field name
  return QString( "%1_%2" ).arg( name(), definition.name() );
}

QgsAuxiliaryStorage::QgsAuxiliaryStorage()
  : mValid( false )
  , mFileName( "" )
  , mSqliteHandler( nullptr )
{
}

QgsAuxiliaryStorage::~QgsAuxiliaryStorage()
{
  close();
  remove();
}

QString QgsAuxiliaryStorage::tmpFileName() const
{
  QTemporaryFile tmpFile;
  tmpFile.setAutoRemove( false );
  tmpFile.open();
  tmpFile.close();

  return tmpFile.fileName();
}

bool QgsAuxiliaryStorage::create()
{
  QString tmpFile = tmpFileName();
  mValid = createDB( tmpFile );

  if ( mValid )
    mFileName = tmpFile;

  return mValid;
}

bool QgsAuxiliaryStorage::open( const QString &filename )
{
  if ( QFile::exists( filename ) )
  {
    mValid = openDB( filename );

    if ( mValid )
      mFileName = filename;
  }

  return mValid;
}

bool QgsAuxiliaryStorage::openCopy( const QString &filename )
{
  if ( QFile::exists( filename ) )
  {
    QString dbFileName = tmpFileName();
    QFile::copy( filename, dbFileName );

    mValid = openDB( dbFileName );

    if ( mValid )
      mFileName = dbFileName;
  }

  return mValid;
}

void QgsAuxiliaryStorage::close()
{
  if ( mSqliteHandler )
  {
    QgsSLConnect::sqlite3_close_v2( mSqliteHandler );
    mSqliteHandler = nullptr;
  }

  mValid = false;
}

void QgsAuxiliaryStorage::remove()
{
  QFile f( mFileName );
  if ( !mFileName.isEmpty() && f.exists() )
  {
    f.remove();
    mFileName = QString();
  }
}

void QgsAuxiliaryStorage::copy( const QString &copy )
{
  close();
  QFile::copy( mFileName, copy );
  open( mFileName );
}

QgsAuxiliaryStorageJoin *QgsAuxiliaryStorage::createJoin( const QgsVectorLayer &layer )
{
  QgsAuxiliaryStorageJoin *join( nullptr );

  if ( mValid )
  {
    QString tableName( layer.id() );
    bool tableYetFilled = tableExists( tableName );
    if ( createTableIfNotExists( tableName ) )
    {
      join = new QgsAuxiliaryStorageJoin( mFileName, tableName, layer, tableYetFilled );
    }
  }

  return join;
}

bool QgsAuxiliaryStorage::isValid() const
{
  return mValid;
}

QString QgsAuxiliaryStorage::fileName() const
{
  return mFileName;
}


bool QgsAuxiliaryStorage::createTableIfNotExists( const QString &table )
{
  QString sql = QString( "CREATE TABLE IF NOT EXISTS '%1' ( 'ID' int64 PRIMARY KEY )" ).arg( table );
  int rc = sqlite3_exec( mSqliteHandler, sql.toStdString().c_str(), nullptr, nullptr, nullptr );
  if ( rc != SQLITE_OK )
  {
    QString err = QObject::tr( "Unable to create table:\n" );
    err += QString::fromUtf8( sqlite3_errmsg( mSqliteHandler ) );
    QgsDebugMsg( err );
    return false;
  }

  return true;
}

bool QgsAuxiliaryStorage::tableExists( const QString &table ) const
{
  QString sql = QString( "SELECT 1 FROM sqlite_master WHERE type='table' AND name='%1'" ).arg( table );
  int rows = 0;
  int columns = 0;
  char **results = nullptr;
  int rc = sqlite3_get_table( mSqliteHandler, sql.toStdString().c_str(), &results, &rows, &columns, nullptr );
  if ( rc != SQLITE_OK )
  {
    QString err = QObject::tr( "Unable to create table:\n" );
    err += QString::fromUtf8( sqlite3_errmsg( mSqliteHandler ) );
    QgsDebugMsg( err );
    return false;
  }

  sqlite3_free_table( results );
  if ( rows >= 1 )
    return true;

  return false;
}

bool QgsAuxiliaryStorage::openDB( const QString &filename )
{
  return !QgsSLConnect::sqlite3_open_v2( filename.toUtf8().constData(), &mSqliteHandler, SQLITE_OPEN_READWRITE, nullptr );
}

bool QgsAuxiliaryStorage::createDB( const QString &filename )
{
  QString errMsg;
  int rc;

  // open/create database
  rc = QgsSLConnect::sqlite3_open_v2( filename.toUtf8().constData(), &mSqliteHandler, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr );
  if ( rc )
  {
    errMsg = QObject::tr( "Could not create a new database\n" );
    errMsg += QString::fromUtf8( sqlite3_errmsg( mSqliteHandler ) );
    goto err;
  }

  // activating Foreign Key constraints
  rc = sqlite3_exec( mSqliteHandler, "PRAGMA foreign_keys = 1", nullptr, nullptr, nullptr );
  if ( rc != SQLITE_OK )
  {
    errMsg = QObject::tr( "Unable to activate FOREIGN_KEY constraints:\n" );
    errMsg += QString::fromUtf8( sqlite3_errmsg( mSqliteHandler ) );
    goto err;
  }

  // init spatial metadata
  rc = initializeSpatialMetadata( errMsg );
  if ( !rc )
  {
    goto err;
  }

  return true;

err:
  QgsSLConnect::sqlite3_close_v2( mSqliteHandler );
  QgsDebugMsg( errMsg );
  return false;
}

bool QgsAuxiliaryStorage::initializeSpatialMetadata( QString &err )
{
  char **results = nullptr;
  int rows, columns;
  bool above41 = false;
  int rc;
  int count = 0;

  QString sql = "select count(*) from sqlite_master";
  rc = sqlite3_get_table( mSqliteHandler, sql.toStdString().c_str(), &results, &rows, &columns, nullptr );
  if ( rc != SQLITE_OK )
  {
    err = QObject::tr( "Unable to execute '%1':\n" ).arg( sql );
    err += QString::fromUtf8( sqlite3_errmsg( mSqliteHandler ) );
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
    err = QObject::tr( "Invalid count" );
    return false;
  }

  sql = "select spatialite_version()";
  rc = sqlite3_get_table( mSqliteHandler, sql.toStdString().c_str(), &results, &rows, &columns, nullptr );
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
  else
  {
    err = QObject::tr( "Unable to execute '%1':\n" ).arg( sql );
    err += QString::fromUtf8( sqlite3_errmsg( mSqliteHandler ) );
    return false;
  }

  sqlite3_free_table( results );

  rc = sqlite3_exec( mSqliteHandler, above41 ? "SELECT InitSpatialMetadata(1)" : "SELECT InitSpatialMetadata()", nullptr, nullptr, nullptr );
  if ( rc != SQLITE_OK )
  {
    err = QObject::tr( "Unable to initialize SpatialMetadata:\n" );
    err += QString::fromUtf8( sqlite3_errmsg( mSqliteHandler ) );
    return false;
  }

  spatial_ref_sys_init( mSqliteHandler, 0 );

  return true;
}
