/***************************************************************************
                           dummy.cpp

  Sample service implementation
  -----------------------------
  begin                : 2016-12-13
  copyright            : (C) 2016 by David Marteau
  email                : david dot marteau at 3liz dot com
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
#include <QTimer>
#include "qgsmodule.h"
#include "monitor.h"

// Module
// class QgsReporter: public QObject
// {
//   Q_OBJECT
//   public:
//     QgsReporter()
//     {
//       std::cout << "REPORTER CST" << std::endl;
//       mTimer.setSingleShot(false);
//       mTimer.setInterval( 1 );
//       QObject::connect( &mTimer, &QTimer::timeout, this, &QgsReporter::send, Qt::DirectConnection);
//     }
//
//     ~QgsReporter()
//     {
//       std::cout << "REPORTER DST" << std::endl;
//     }
//
//     void go()
//     {
//       mTimer.start(1);
//     }
//
//   public slots:
//     void send()
//     {
//       std::cout << "SEND" << std::endl;
//     }
//
//   private:
//     QTimer mTimer;
// };

class QgsMonitorModule: public QgsServiceModule
{
  public:
    QgsMonitorModule()
    {
      std::cout << "MONITOR MODULE CST!!!" << std::endl;
      // QThread* thread = new QThread;
      // Worker2* worker = new Worker2();
      // worker->moveToThread(thread);
      // QObject::connect(thread, &QThread::started, worker, &Worker2::process);
      // thread->start();
    }

    ~QgsMonitorModule()
    {
      std::cout << "MONITOR MODULE DST!!!" << std::endl;
      // mThread->quit();
      // delete mMonitor;
      // delete mThread;
    }

    void registerSelf( QgsServiceRegistry &registry, QgsServerInterface *serverIface ) override
    {
      std::cout << "MONITOR MODULE REGISTER!!!" << std::endl;
      qDebug() << QThread::currentThreadId();

      MyObject2 object;
      object.startWorkInAThread();

      // mMonitor = new QgsServerMonitor();
      // mMonitor->init();

      // mThread = new QThread();
      // mMonitor->moveToThread(mThread);

      // mThread->start();
      // mMonitor->init();

      // mMonitor.moveToThread(mThread);
      // mThread.start();
      // mMonitor.start();
      // mMonitor.go();
      // mReporter.send();
      // mReporter.go();
      // registry.registerApi( landingPageApi );
      // mMonitor = new QgsServerMonitor();
      // QObject::connect(&mTimer, &QTimer::timeout, QgsConfigSender::monitor);
      // mTimer.start(3000);
    }

  private:
    // QgsReporter mReporter;
    QThread *mThread = nullptr;
    QgsServerMonitor *mMonitor = nullptr;
};

// Entry points
QGISEXTERN QgsServiceModule *QGS_ServiceModule_Init()
{
  static QgsMonitorModule sModule;
  return &sModule;
}
QGISEXTERN void QGS_ServiceModule_Exit( QgsServiceModule * )
{
  // Nothing to do
}
