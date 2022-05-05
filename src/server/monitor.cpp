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
Worker::Worker()
{
  // you could copy data from constructor arguments to internal variables here.
}

// --- DECONSTRUCTOR ---
Worker::~Worker()
{
  // free resources
}

// --- PROCESS ---
// Start processing data.
void Worker::process()
{
  // allocate resources using new here
  std::cout << "PROCESS!!!" << std::endl;
  qDebug( "Hello World!" );
  emit finished();

  // mTimer = new QTimer();
  // mTimer->setSingleShot(false);
  // mTimer->setInterval(1000);
  // connect(mTimer, &QTimer::timeout, this, &Worker::monitor, Qt::DirectConnection);
  // mTimer->moveToThread(mThread);

  while ( 1 )
  {
    QThread::msleep( 1000 );
    std::cout << "PLOUFFFF" << std::endl;
  }
}

void Worker::monitor()
{
  std::cout << "YESSS!!!" << std::endl;
}

MyObject::MyObject() {}

MyObject::~MyObject() {}

void MyObject::startWorkInAThread()
{
  mThread = new WorkerThread();
  connect( mThread, &WorkerThread::resultReady, this, &MyObject::handleResults );
  connect( mThread, &WorkerThread::finished, mThread, &QObject::deleteLater );
  mThread->start();
  std::cout << "Thread started" << std::endl;
}

void MyObject::handleResults( const QString & ) { std::cout << "HANDLE RESULT" << std::endl; }

void MyObject::log()
{
  mThread->log();
}

void WorkerThread::run()
{
  std::cout << "run" << std::endl;
  qDebug() << QThread::currentThreadId();
  QString result;
  /* ... here is the expensive or blocking operation ... */

  mTimer = new QTimer();
  mTimer->setSingleShot( false );
  mTimer->setInterval( 1000 );
  connect( mTimer, &QTimer::timeout, this, &WorkerThread::coucou, Qt::DirectConnection );
  mTimer->start();

  while ( 1 )
  {
  }

  // emit resultReady(result);
  std::cout << "emit resultReady!!!" << std::endl;
}

void WorkerThread::log()
{
  std::cout << "remaining time: " <<  mTimer->remainingTime() << std::endl;
}

void WorkerThread::coucou()
{
  std::cout << "coucou" << std::endl;
}
