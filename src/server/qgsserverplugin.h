/***************************************************************************
                              qgsserverplugins.h
                              -------------------------
  begin                : August 28, 2014
  copyright            : (C) 2014 by Alessandro Pasotti - ItOpen
  email                : apasotti at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSSERVERPLUGIN_H
#define QGSSERVERPLUGIN_H

#include <QString>
#include <QVariant>

#include "qgis_sip.h"
#include "qgis_server.h"

SIP_IF_MODULE( HAVE_SERVER_PYTHON_PLUGINS )

/**
 * \ingroup server
 * \brief Initializes Python server plugins and stores a list of server plugin names
 * \since QGIS 2.8
 */
class SERVER_EXPORT QgsServerPlugin
{
  public:

    /**
     * Default constructor for QgsServerPlugin.
     */
    QgsServerPlugin( const QString &name );
    virtual ~QgsServerPlugin() = default;

    virtual QVariant run( const QString &action, const QVariant &data ) const;

  private:
    QString mName;
};

#endif // QGSSERVERPLUGIN_H
