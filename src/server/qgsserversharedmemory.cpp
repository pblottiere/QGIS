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

#include <iostream>
#include "qgsserversharedmemory.h"


QgsServerSharedMemory::QgsServerSharedMemory( const QString &key )
{
  setKey( key );
  std::cout << "ERROR!!!!!!!!!!!!!!!!!!!!" << std::endl;
  std::cout << mSharedMemory.errorString().toStdString() << std::endl;
}

QgsServerSharedMemory::~QgsServerSharedMemory()
{
  std::cout << "DEST!!!!" << std::endl;
  mSharedMemory.detach();
}

void QgsServerSharedMemory::setKey( const QString &key )
{
  mSharedMemory.setKey( key );

  if ( mSharedMemory.isAttached() )
  {
    mSharedMemory.detach();
  }
  mSharedMemory.attach();
}

// bool QgsServerSharedMemory::write( const QgsServerSharedMemory::Settings &settings )
bool QgsServerSharedMemory::write()
{
  // const int n = 10;

  // struct SharedData {
  //   double fourDoubles[4];
  //   int andAnInt;
  //   unsigned char port[10];
  // };

  // struct Data {
  //   int n;
  //   SharedData data[500];
  // };

  std::cout << "write 0" << std::endl;
  mSharedMemory.create( 100 );

  mSharedMemory.lock();
  std::cout << "write BEFORE" << std::endl;
  char *r2 = ( char * )mSharedMemory.data();
  std::cout << r2 << std::endl;
  mSharedMemory.unlock();
  std::cout << "write BEFORE" << std::endl;

  std::cout << "write 1" << std::endl;
  mSharedMemory.lock();

  std::cout << "write 2" << std::endl;
  char *to = ( char * )mSharedMemory.data();
  memcpy( to, "COUCOU", 6 );
  std::cout << "write 3" << std::endl;

  // Data* d = static_cast<Data*>(mSharedMemory->data());
  // d->n = n;
  // d->data[0].fourDoubles[0] = 1.0;
  // memcpy(d->data[0].port, "coucou0", 7);
  // memcpy(d->data[1].port, "coucou1", 7);

  // SharedData* p = static_cast<SharedData*>(mSharedMemory->data());
  // p->fourDoubles[0] = 1.0;
  // p->andAnInt = 42;
  // memcpy(p->port, "coucou", 6);
  mSharedMemory.unlock();
  std::cout << "write 4" << std::endl;

  mSharedMemory.lock();
  std::cout << "write 5" << std::endl;
  char *r = ( char * )mSharedMemory.data();
  std::cout << r << std::endl;
  mSharedMemory.unlock();
  std::cout << "write 6" << std::endl;
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
