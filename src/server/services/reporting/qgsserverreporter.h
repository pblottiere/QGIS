/***************************************************************************
                           qgsserverreporter.h

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

#include <QUrl>
#include <QTimer>
#include <QThread>
#include <QNetworkAccessManager>

#include "qgis_server.h"
#include "qgsserverinterface.h"

#ifndef QGSREPORTER
#define QGSREPORTER

class QgsServerReporter;

class SERVER_EXPORT QgsServerReporting : public QThread
{
    Q_OBJECT

  public:
    QgsServerReporting( QgsServerInterface *serverIface );
    void run() override;

  private:
    QTimer *mTimer = nullptr;
    QgsServerInterface *mServerIface = nullptr;
};


class SERVER_EXPORT QgsServerReporter : public QObject
{
    Q_OBJECT

  public:
    QgsServerReporter( QgsServerInterface *serverIface );

    void report();

  private:
    QUrl mUrl;
    QNetworkAccessManager *mNam = nullptr;
    QgsServerInterface *mServerIface = nullptr;
};

#endif
