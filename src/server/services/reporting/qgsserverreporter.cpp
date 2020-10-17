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

#include "qgsjsonutils.h"
#include "qgsconfigcache.h"
#include "qgsserverreporter.h"


QgsServerReporting::QgsServerReporting( QgsServerInterface *serverIface )
  : mServerIface( serverIface )
{
}

void QgsServerReporting::run()
{
  mTimer = new QTimer();
  mTimer->setSingleShot( false );
  mTimer->setInterval( 1000 );

  QgsServerReporter reporter( mServerIface );
  connect( mTimer, &QTimer::timeout, &reporter, &QgsServerReporter::report );
  mTimer->start( 1000 );

  exec();
}

QgsServerReporter::QgsServerReporter( QgsServerInterface *serverIface )
  : mUrl( "http://localhost:8989" )
  , mServerIface( serverIface )
{
  mNam = new QNetworkAccessManager();
}

void QgsServerReporter::report()
{
  const QList<QString> projects = QgsConfigCache::instance()->projects();
  json json_projects = json::array();
  for ( const QString &project : projects )
  {
    json_projects.push_back( project.toStdString() );
  }

  const QgsServerSettings *settings = mServerIface->serverSettings();

  json data;
  data["projects"] = json_projects;
  data["settings"]["max"] = settings->apiWfs3MaxLimit();

  const std::string data_str = data.dump();
  const QByteArray data_json( data_str.data(), int( data_str.size() ) );

  QNetworkRequest request( mUrl );
  request.setHeader( QNetworkRequest::ContentTypeHeader,
                     QStringLiteral( "application/json" ) );
  mNam->post( request, data_json );
}
