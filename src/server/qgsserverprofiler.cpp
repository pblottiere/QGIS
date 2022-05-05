/***************************************************************************
                          qgsserverprofiler.cpp
                          ---------------------
  begin                : December 02, 2020
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
#include "qgsapplication.h"
#include "qgsserverprofiler.h"


QgsServerProfilingUnit::QgsServerProfilingUnit( QgsServerProfiling &profiling ) :
  mProfiling( profiling )
{
}

QgsServerProfilingUnit::~QgsServerProfilingUnit()
{
  mProfiling.update();
  QgsApplication::profiler()->clear();
}

QgsServerProfiling::QgsServerProfiling()
  : mProfiler( QgsApplication::profiler() )
{
  std::cout << "PROFILING:" << std::endl;
  // store startup group in json data
  for ( const QString &group : mProfiler->groups() )
  {
    json data = json::array();

    for ( const QString &name : mProfiler->childGroups( group ) )
    {
      data[name.toStdString()] = mProfiler->profileTime( name );
    }

    mData[group.toStdString()] = data;
  }

  // clear profiler for a fresh start
}

void QgsServerProfiling::update()
{
  // add generic info

  // add specific server info
}

void QgsServerProfiling::dump() const
{
  // const QStringList sl = QgsApplication::profiler()->childGroups( QString(), QStringLiteral( "server" ) );
  // for ( const QString &s : sl )
  //   std::cout << s.toStdString() << ": " << QgsApplication::profiler()->profileTime( s, QStringLiteral( "server" ) ) << std::endl;
}

