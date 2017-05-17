/***************************************************************************
  qgsziputils.cpp
  ---------------------
begin                : April 2017
copyright            : (C) 2017 by Paul Blottiere
email                : paul.blottiere@oslandia.com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <fstream>

#include <QFileInfo>
#include <QDir>

#include "zip.h"

#include "qgslogger.h"
#include "qgsziputils.h"

QStringList QgsZipUtils::unzip( const QString &zipFilename, const QString &dir )
{
  QStringList files;
  int err = 0;

  struct zip *z = zip_open( zipFilename.toStdString().c_str(), ZIP_CHECKCONS, &err );

  if ( err == ZIP_ER_OK && z != NULL )
  {
    int count = zip_get_num_files( z );
    if ( count != -1 )
    {
      struct zip_stat stat;

      for ( int i = 0; i < count; i++ )
      {
        zip_stat_index( z, i, 0, &stat );
        size_t len = stat.size;

        struct zip_file *file = zip_fopen_index( z, i, 0 );
        char *buf = new char[len];
        zip_fread( file, buf, len );
        zip_fclose( file );

        QFileInfo newFile( QDir( dir ), QString( stat.name ) );
        std::ofstream( newFile.absoluteFilePath().toStdString() ).write( buf, len );
        files.append( newFile.absoluteFilePath() );
      }
    }
    else
    {
      QgsLogger::warning( "Error getting number of files into zip archive" );
    }

    zip_close( z );
  }
  else
  {
    QgsLogger::warning( "Error opening zip archive" );
  }

  return files;
}

bool QgsZipUtils::zip( const QString &zipFilename, QStringList files )
{
  bool rc = false;
  int err = 0;

  struct zip *z = zip_open( zipFilename.toStdString().c_str(), ZIP_CREATE, &err );

  if ( err == ZIP_ER_OK && z != NULL )
  {
    Q_FOREACH ( QString file, files )
    {
      QFileInfo fileInfo( file );
      zip_source *src = zip_source_file( z, file.toStdString().c_str(), 0, 0 );
      if ( src != NULL )
      {
        if ( zip_file_add( z, fileInfo.fileName().toStdString().c_str(), src, 0 ) == -1 )
        {
          QgsLogger::warning( "Error adding file into zip archive" );
          rc = false;
        }
        else
        {
          rc = true;
        }
      }
      else
      {
        rc = false;
        QgsLogger::warning( "Error opening zip source file" );
      }
    }

    zip_close( z );
  }
  else
  {
    QgsLogger::warning( "Error creating zip archive" );
  }

  return rc;
}
