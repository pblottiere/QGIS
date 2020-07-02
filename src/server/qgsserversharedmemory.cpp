/***************************************************************************
                        qgsserversharedmemory.cpp
                        -------------------------

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

#include "qgsserversharedmemory.h"

#include <QSharedMemory>

QgsServerSharedMemory::QgsServerSharedMemory( const QString &key, bool readOnly )
  : mSharedMemory( new QSharedMemory() )
{
}

QgsServerSharedMemory::~QgsServerSharedMemory()
{
  mSharedMemory->detach();
}

bool QgsServerSharedMemory::write( const QgsServerSharedMemory::Settings &settings )
{
}

QgsServerSharedMemory::Settings QgsServerSharedMemory::read( const QString &uid ) const
{
}

QList<QgsServerSharedMemory::Settings> QgsServerSharedMemory::readAll() const
{
}

int QgsServerSharedMemory::size() const
{
}
