/***************************************************************************
                              qgsserverlogger.cpp
                              -------------------
  begin                : May 5, 2014
  copyright            : (C) 2014 by Marco Hugentobler
  email                : marco dot hugentobler at sourcepole dot ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsserverlogger.h"
#include "qgsapplication.h"
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QTime>

#include <cstdlib>

QgsServerLogger *QgsServerLogger::sInstance = nullptr;

QgsServerLogger *QgsServerLogger::instance()
{
  if ( !sInstance )
  {
    sInstance = new QgsServerLogger();
  }
  return sInstance;
}

QgsServerLogger::QgsServerLogger()
  : mLogFile( nullptr )
{
  connect( QgsApplication::messageLog(), static_cast<void ( QgsMessageLog::* )( const QString &, const QString &, Qgis::MessageLevel )>( &QgsMessageLog::messageReceived ), this,
           &QgsServerLogger::logMessage );
}

void QgsServerLogger::setLogLevel( Qgis::MessageLevel level )
{
  mLogLevel = level;
}

void QgsServerLogger::setLogFile( const QString &f )
{
  if ( ! f.isEmpty() )
  {
    if ( mLogFile.exists() )
    {
      mTextStream.flush();
      mLogFile.close();
    }

    mLogFile.setFileName( f );
    if ( mLogFile.open( QIODevice::Append ) )
    {
      mTextStream.setDevice( &mLogFile );
    }
  }
}

void QgsServerLogger::logMessage( const QString &message, const QString &tag, Qgis::MessageLevel level )
{
  Q_UNUSED( tag );
  if ( !mLogFile.isOpen() || mLogLevel > level )
  {
    return;
  }

  mTextStream << ( "[" + QString::number( qlonglong( QCoreApplication::applicationPid() ) ) + "]["
                   + QTime::currentTime().toString() + "] " + message + "\n" );
  mTextStream.flush();
}

QgsServerLoggerBis *QgsServerLoggerBis::sInstance = nullptr;

QgsServerLoggerBis *QgsServerLoggerBis::instance()
{
  if ( !sInstance )
  {
    sInstance = new QgsServerLoggerBis();
  }
  return sInstance;
}

QgsServerLoggerBis::QgsServerLoggerBis()
  : QgsMessageLogConsole()
{
}

void QgsServerLoggerBis::logMessage( const QString &message, const QString &tag, Qgis::MessageLevel level )
{
  if ( mLogLevel > level )
  {
    return;
  }

  if ( mLogFile.isOpen() )
  {
    const QString pid = QString::number( QCoreApplication::applicationPid() );
    const QString time = QTime::currentTime().toString();
    mTextStream << QString( "[%1][%2] %3" ).arg( pid, time, message );
    mTextStream.flush();
  }
  else
  {
    // log to std::cerr by default
    QgsMessageLogConsole::logMessage( message, tag, level );
  }
}

void QgsServerLoggerBis::setLogLevel( const Qgis::MessageLevel level )
{
  mLogLevel = level;
}

bool QgsServerLoggerBis::setLogFile( const QString &filename )
{
  mTextStream.flush();
  mLogFile.close();
  mLogFile.setFileName( "" );

  if ( QFile( filename ).exists() )
  {
    mLogFile.setFileName( filename );
    if ( mLogFile.open( QIODevice::Append ) )
    {
      mTextStream.setDevice( &mLogFile );
    }
  }

  return ( ( mTextStream.device() == 0 ) ? false : true );
}
