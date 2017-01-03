/***************************************************************************
                          qgsserver.h
  QGIS Server main class.
                        -------------------
  begin                : June 05, 2015
  copyright            : (C) 2015 by Alessandro Pasotti
  email                : a dot pasotti at itopen dot it

  Based on previous work from:

  begin                : July 04, 2006
  copyright            : (C) 2006 by Marco Hugentobler & Ionut Iosifescu Enescu
  email                : marco dot hugentobler at karto dot baug dot ethz dot ch

  ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#ifndef QGSSERVER_H
#define QGSSERVER_H

#include <QFileInfo>
#include "qgsrequesthandler.h"
#include "qgsapplication.h"
#include "qgsconfigcache.h"
#include "qgscapabilitiescache.h"
#include "qgsmapsettings.h"
#include "qgsmessagelog.h"

#ifdef HAVE_SERVER_PYTHON_PLUGINS
#include "qgsserverplugins.h"
#include "qgsserverfilter.h"
#include "qgsserverinterfaceimpl.h"
#endif

class QgsProject;


/** \ingroup server
 * The QgsServer class provides OGC web services.
 */
class SERVER_EXPORT QgsServer
{
  public:

    /** Creates the server instance
     * @param captureOutput set to false for stdout output (FCGI)
     */
    QgsServer( bool captureOutput = true );

    /** Set environment variable
     * @param var environment variable name
     * @param val value
     * @note added in 2.14
     */
    void putenv( const QString &var, const QString &val );

    /** Handles the request. The output is normally printed trough FCGI printf
     * by the request handler or, in case the server has been invoked from python
     * bindings, a flag is set that captures all the output headers and body, instead
     * of printing it returns the output as a QPair of QByteArray.
     * The query string is normally read from environment
     * but can be also passed in args and in this case overrides the environment
     * variable
     *
     * @param queryString optional QString containing the query string
     * @return the response headers and body QPair of QByteArray if called from python bindings, empty otherwise
     */
    QPair<QByteArray, QByteArray> handleRequest( const QString& queryString = QString() );

#ifdef HAVE_SERVER_PYTHON_PLUGINS
    //! Returns a pointer to the server interface
    QgsServerInterfaceImpl* serverInterface() { return sServerInterface; }

    //! Intialize python
    //! Note: not in python bindings
    void initPython( );
#endif

  private:

    //! Server initialization
    static bool init();

    // All functions that where previously in the main file are now
    // static methods of this class
    static QString configPath( const QString& defaultConfigPath,
                               const QMap<QString, QString>& parameters );
    // Mainly for debug
    static void dummyMessageHandler( QtMsgType type, const char *msg );
    // Mainly for debug
    static void printRequestInfos();

    /**
     * @brief QgsServer::printRequestParameters prints the request parameters
     * @param parameterMap
     * @param logLevel
     */
    static void printRequestParameters(
      const QMap< QString, QString>& parameterMap,
      QgsMessageLog::MessageLevel logLevel );

    static QFileInfo defaultProjectFile();
    static QFileInfo defaultAdminSLD();
    static void setupNetworkAccessManager();
    //! Create and return a request handler instance
    static QgsRequestHandler* createRequestHandler( const bool captureOutput = false );

    // Return the server name
    static QString &serverName();

    // Status
    static QString* sConfigFilePath;
    static QgsCapabilitiesCache* sCapabilitiesCache;
#ifdef HAVE_SERVER_PYTHON_PLUGINS
    static QgsServerInterfaceImpl* sServerInterface;
#endif
    //! Initialization must run once for all servers
    static bool sInitialised;
    static bool sCaptureOutput;

    // map of QgsProject
    QMap<QString, const QgsProject*> mProjectStore;
};
#endif // QGSSERVER_H

