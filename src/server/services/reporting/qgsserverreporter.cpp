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
#include <iostream>

#include "qgsserverreporter.h"


void QgsServerReporting::run()
{
  mTimer = new QTimer();
  mTimer->setSingleShot( false );
  mTimer->setInterval( 1000 );

  QgsServerReporter reporter;
  connect( mTimer, &QTimer::timeout, &reporter, &QgsServerReporter::report );
  mTimer->start( 1000 );

  exec();
}

void QgsServerReporter::report()
{
  std::cout << "REPORT!!!!" << std::endl;
}
