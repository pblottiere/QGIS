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
#include <stdio.h>
#include <iostream>
#include "monitor.h"
#include "qgsapplication.h"


// QgsConfigSender::QgsConfigSender()
//       : QgsTask( "server_config" )
// {
//       std::cout << "QgsConfigSender::COUCOU!!!!" << std::endl;
//       std::cout << QThread::currentThreadId() << std::endl;
//       // connect(&mTimer, &QTimer::timeout, this, &QgsConfigSender::);
//       // mTimer.start(2000);
// }
//
// bool QgsConfigSender::run()
// {
//   std::cout << "run 0" << std::endl;
//   std::cout << QThread::currentThreadId() << std::endl;
//   // QEventLoop loop;
//   // connect(&timer, &QTimer::timeout, this, &QgsConfigSender::coucou);
//   // std::cout << "run 1" << std::endl;
//   // timer.start(1000);
//   // std::cout << "run 2" << std::endl;
//   // loop.exec();
//   // std::cout << "run 3" << std::endl;
//   return true;
// }

QgsServerMonitor::QgsServerMonitor()
{
  std::cout << "Monitor cst" << std::endl;
}

QgsServerMonitor::~QgsServerMonitor()
{
  std::cout << "Monitor dst" << std::endl;
}

void QgsServerMonitor::init()
{
  std::cout << "init" << std::endl;
  QgsApplication::setMaxThreads( 10 );
  qDebug() << QThread::currentThreadId();
  mThread = new QThread();
  connect( mThread, &QThread::started, this, &QgsServerMonitor::loop );
  mThread->start();
  QgsApplication::instance()->processEvents();
  qApp->processEvents();

  std::cout << "start called" << std::endl;

  // mTimer->start();
  // QMetaObject::invokeMethod( mTimer, "start", Qt::QueuedConnection );
  // mTimer->start();
  // for ( int i = 0; i < 500000; i++ )
  // {
  //   // std::cout << mTimer->remainingTime() << std::endl;
  //   std::cout << mTimer << std::endl;
  // }
}

void QgsServerMonitor::loop()
{
  std::cout << "loop" << std::endl;
  qDebug() << QThread::currentThreadId();
  mTimer = new QTimer();
  mTimer->setSingleShot( false );
  mTimer->setInterval( 1000 );
  connect( mTimer, &QTimer::timeout, this, &QgsServerMonitor::monitor, Qt::DirectConnection );
  mTimer->moveToThread( mThread );
}

void QgsServerMonitor::monitor()
{
  std::cout << "MONITOR" << std::endl;
  fflush( stdout );
  // QgsApplication::taskManager()->addTask( new QgsConfigSender() );
}

// --- CONSTRUCTOR ---
Worker2::Worker2()
{
  // you could copy data from constructor arguments to internal variables here.
}

// --- DECONSTRUCTOR ---
Worker2::~Worker2()
{
  // free resources
}

// --- PROCESS ---
// Start processing data.
void Worker2::process()
{
  // allocate resources using new here
  std::cout << "PROCESS!!!" << std::endl;
  qDebug( "Hello World!" );
  // emit finished();

  // mTimer = new QTimer();
  // mTimer->setSingleShot(false);
  // mTimer->setInterval(1000);
  // connect(mTimer, &QTimer::timeout, this, &Worker2::monitor);

  // exec();

  // mTimer->moveToThread(mThread);

  while ( 1 )
  {
    QThread::msleep( 1000 );
    std::cout << "PLOUFFFF YESSS!!!!" << std::endl;
    qDebug() << QThread::currentThreadId();
  }
}

void Worker2::monitor()
{
  std::cout << "YESSS!!!" << std::endl;
}

MyObject2::MyObject2() {}

MyObject2::~MyObject2() {}

void MyObject2::startWorkInAThread()
{
  mThread = new WorkerThread2();
  connect( mThread, &WorkerThread2::resultReady, this, &MyObject2::handleResults );
  connect( mThread, &WorkerThread2::finished, mThread, &QObject::deleteLater );
  mThread->start();
  std::cout << "Thread started" << std::endl;
}

void MyObject2::handleResults( const QString & ) { std::cout << "HANDLE RESULT" << std::endl; }

void MyObject2::log()
{
  mThread->log();
}

void WorkerThread2::run()
{
  std::cout << "run 0" << std::endl;
  qDebug() << QThread::currentThreadId();
  QString result;
  /* ... here is the expensive or blocking operation ... */

  std::cout << "run 1" << std::endl;
  mTimer = new QTimer();
  std::cout << "run 2" << std::endl;
  mTimer->setSingleShot( false );
  std::cout << "run 3" << std::endl;
  mTimer->setInterval( 1000 );
  std::cout << "run 4" << std::endl;
  Logger logger;
  connect( mTimer, &QTimer::timeout, &logger, &Logger::log );
  std::cout << "run 5" << std::endl;
  mTimer->start( 1000 );
  std::cout << "run 6" << std::endl;
  exec();
  std::cout << "run 7" << std::endl;
}

void WorkerThread2::log()
{
  std::cout << "remaining time: " <<  mTimer->remainingTime() << std::endl;
}

void WorkerThread2::coucou()
{
  std::cout << "coucou" << std::endl;
  qDebug() << QThread::currentThreadId();
}

void Logger::log()
{
  std::cout << "LOG!!!" << std::endl;
  qDebug() << QThread::currentThreadId();
}
