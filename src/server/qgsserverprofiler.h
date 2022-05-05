/***************************************************************************
                          qgsserverprofiler.h
                          -------------------
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


#ifndef QGSSERVERPROFILER_H
#define QGSSERVERPROFILER_H

#include "qgis_server.h"
#include "qgsjsonutils.h"
#include "qgsruntimeprofiler.h"

#include "inja/inja.hpp"
#include "nlohmann/json_fwd.hpp"

/**
 * \ingroup server
 * The QgsServerProfiler class provides a profiler specifically configured for
 * QGIS Server.
 */
class SERVER_EXPORT QgsServerProfiling
{
  public:
    QgsServerProfiling();

    void update();

    void dump() const;

  private:
    json mData;
    QgsRuntimeProfiler *mProfiler = nullptr;
};

class SERVER_EXPORT QgsServerProfilingUnit
{
  public:
    QgsServerProfilingUnit( QgsServerProfiling &profiling );
    ~QgsServerProfilingUnit();

  private:
    QgsServerProfiling &mProfiling;
};

#endif // QGSSERVERPROFILER_H
