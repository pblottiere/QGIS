/***************************************************************************
                           qgsserverreporter.cpp

  Reporter
  -----------------------------
  begin                : 2020-10-15
  copyright            : (C) 2020 by Paul Blottiere
  email                : blottiere.paul@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <iostream>

#include <QNetworkRequest>

#include "inja/inja.hpp"
#include "nlohmann/json_fwd.hpp"

#include "qgsserverplugins.h"
#include "qgsjsonutils.h"
#include "qgsconfigcache.h"
#include "qgsserverreporter.h"
#include "qgsserverapi.h"


QgsServerReporting::QgsServerReporting( QgsServerInterface *serverIface )
  : mServerIface( serverIface )
{
}

void QgsServerReporting::run()
{
  mTimer = new QTimer();
  mTimer->setSingleShot( false );
  mTimer->setInterval( 3000 );

  QgsServerReporter reporter( mServerIface );
  connect( mTimer, &QTimer::timeout, &reporter, &QgsServerReporter::report );
  mTimer->start( 3000 );

  exec();
}

QgsServerReporter::QgsServerReporter( QgsServerInterface *serverIface )
  : mUrl( "http://localhost:8989" )
  , mServerIface( serverIface )
{
  mNam = new QNetworkAccessManager();
  mMutex.reset( new QMutex( QMutex::Recursive ) );
}

void QgsServerReporter::report()
{
  QMutexLocker locker( mMutex.get() );

  json data;
  const QList<QString> projects = QgsConfigCache::instance()->projects();
  json json_projects = json::array();
  for ( const QString &project : projects )
  {
    json_projects.push_back( project.toStdString() );
  }
  data["projects"] = json_projects;

  data["about"]["QT_VERSION"] = QT_VERSION_STR;
  data["about"]["PROJ_VERSION"] = PROJ_VERSION_MAJOR;
  data["about"]["QGIS_VERSION"] = VERSION;

  const QgsServerSettings *settings = mServerIface->serverSettings();
  data["settings"]["QGIS_SERVER_MAX_THREADS"] = settings->maxThreads();
  data["settings"]["QGIS_SERVER_PARALLEL_RENDERING"] = settings->parallelRendering();
  data["settings"]["QGIS_SERVER_CACHE_SIZE"] = settings->cacheSize();
  data["settings"]["QGIS_SERVER_API_WFS3_MAX_LIMIT"] = settings->apiWfs3MaxLimit();

  json json_plugins = json::array();
  data["plugins"] = json_plugins;

  QString data_str = QString::fromStdString( data.dump() );

  // call plugin
  QgsServerApi *api = mServerIface->serviceRegistry()->getApi( "reporting" );
  if ( api )
  {
    const QVariant d = api->plugin()->run( "coucou", QVariant( data_str ) );
    data_str = d.toString();
  }

  const QByteArray data_json( data_str.toUtf8(), int( data_str.size() ) );

  QNetworkRequest request( mUrl );
  request.setHeader( QNetworkRequest::ContentTypeHeader,
                     QStringLiteral( "application/json" ) );
  mNam->post( request, data_json );
}
