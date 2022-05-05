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

// #include "qgstaskmanager.h"
#include "qgis_server.h"
#include <iostream>
#include <QThread>
#include <QObject>
#include <QTimer>

#ifndef COUCOU
#define COUCOU

// class QgsConfigSender: public QgsTask
// {
//   public:
//     QgsConfigSender();
//     bool run();
// };

class SERVER_EXPORT QgsServerMonitor : public QObject
{
    Q_OBJECT

  public:
    QgsServerMonitor();
    ~QgsServerMonitor();

    void init();
    // void process();

  public slots:
    void loop();
    void monitor();

  private:
    QTimer *mTimer = nullptr;
    QThread *mThread = nullptr;
};

class SERVER_EXPORT WorkerThread2 : public QThread
{
    Q_OBJECT
  public:
    void run() override;

    void log();

  signals:
    void resultReady( const QString &s );

  public slots:
    void coucou();

  private:
    QTimer *mTimer = nullptr;
};

class SERVER_EXPORT Worker2 : public QObject
{
    Q_OBJECT

  public:
    Worker2();
    ~Worker2();

    void monitor();

  public slots:
    void process();

  signals:
    void finished();
    void error( QString err );

  private:
    QTimer *mTimer = nullptr;
    // add your variables here
};

class SERVER_EXPORT MyObject2 : public QObject
{
    Q_OBJECT

  public:
    MyObject2();
    ~MyObject2();

    void startWorkInAThread();

    void log();

  public slots:
    void handleResults( const QString & );

  private:
    WorkerThread2 *mThread = nullptr;

};

class Logger : public QObject
{
    Q_OBJECT

  public slots:
    void log();

};

#endif
