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

#include <QDebug>
#include <iostream>

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
  : mServerIface( serverIface )
{
}

void QgsServerReporter::report()
{
  const QList<QString> projects = QgsConfigCache::instance()->projects();
  const QgsServerSettings *settings = mServerIface->serverSettings();
}
