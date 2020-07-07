/***************************************************************************
                        qgsserversharedmemory.h
                        -----------------------

  begin                : 2020-06-27
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

#ifndef QGSSERVERSHAREDMEMORY_H
#define QGSSERVERSHAREDMEMORY_H

#include <memory>
#include <QString>
#include <QSharedMemory>

#include "qgis_server.h"
#include "qgis_sip.h"

class QSharedMemory;

#define MAX_INSTANCES 50

class SERVER_EXPORT QgsServerSharedMemory
{
  public:
    struct Settings
    {
      public:
        QString uid() const;

        QString mIpAdress;
        QString mPort;
    };

    QgsServerSharedMemory( const QString &key = QString() );

    ~QgsServerSharedMemory();

    void setKey( const QString &key );

    // bool write( const QgsServerSharedMemory::Settings &settings );
    bool write();

    QgsServerSharedMemory::Settings read( const QString &uid ) const;

    QList<QgsServerSharedMemory::Settings> readAll() const;

    int size() const;

  private:
    struct Data
    {
      Settings settings[MAX_INSTANCES];
    };

    QSharedMemory mSharedMemory;
};

#endif
