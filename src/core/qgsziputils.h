/***************************************************************************
  qgsziputils.h
  ---------------------
begin                : April 2017
copyright            : (C) 2017 by Paul Blottiere
email                : paul.blottiere@oslandia.com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSZIPUTILS_H
#define QGSZIPUTILS_H

#include "qgis_core.h"
#include <QStringList>

namespace QgsZipUtils
{
  CORE_EXPORT QStringList unzip( const QString &zipFilename, const QString &dir );

  CORE_EXPORT bool zip( const QString &zipFilename, QStringList files );
}

#endif //QGSZIPUTILS_H
