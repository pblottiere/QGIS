/***************************************************************************
                           qgsreporting.cpp

  Reporting service
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

#include "qgsmodule.h"
#include "qgsserverreporter.h"

// Module
class QgsReportingApi: public QgsServerApi
{
  public:
    QgsReportingApi( QgsServerInterface *iface )
      : QgsServerApi( iface )
    {
    }

    void executeRequest( const QgsServerApiContext & ) const {};

    const QString name() const { return QString( "reporting" ); };
    const QString description() const { return QString(); };
    const QString rootPath() const { return QString( "/reporting" ); };

};

class QgsReportingModule: public QgsServiceModule
{
  public:
    void registerSelf( QgsServiceRegistry &registry, QgsServerInterface *serverIface ) override
    {
      QgsReportingApi *api = new QgsReportingApi( serverIface );
      registry.registerApi( api );

      mReporting = new QgsServerReporting( serverIface );
      QObject::connect( mReporting, &QgsServerReporting::finished, mReporting, &QObject::deleteLater );
      mReporting->start();
    }

  private:
    QgsServerReporting *mReporting = nullptr;
};

// Entry points
QGISEXTERN QgsServiceModule *QGS_ServiceModule_Init()
{
  static QgsReportingModule sModule;
  return &sModule;
}

QGISEXTERN void QGS_ServiceModule_Exit( QgsServiceModule * )
{
  // Nothing to do
}
