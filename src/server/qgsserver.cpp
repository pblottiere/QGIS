/***************************************************************************
                              qgsserver.cpp
 A server application supporting WMS / WFS / WCS
                              -------------------
  begin                : July 04, 2006
  copyright            : (C) 2006 by Marco Hugentobler & Ionut Iosifescu Enescu
                       : (C) 2015 by Alessandro Pasotti
  email                : marco dot hugentobler at karto dot baug dot ethz dot ch
                       : elpaso at itopen dot it
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

//for CMAKE_INSTALL_PREFIX
#include "qgsconfig.h"
#include "qgsserver.h"
#include "qgsmslayercache.h"

#include "qgsmapsettings.h"
#include "qgsauthmanager.h"
#include "qgscapabilitiescache.h"
#include "qgsfontutils.h"
#include "qgsrequesthandler.h"
#include "qgsproject.h"
#include "qgsproviderregistry.h"
#include "qgslogger.h"
#include "qgswfsserver.h"
#include "qgswcsserver.h"
#include "qgsmapserviceexception.h"
#include "qgspallabeling.h"
#include "qgsnetworkaccessmanager.h"
#include "qgsserverlogger.h"
#include "qgseditorwidgetregistry.h"
#include "qgsserverrequest.h"
#include "qgsbufferserverresponse.h"
#include "qgsfilterresponsedecorator.h"
#include "qgsservice.h"

#include <QDomDocument>
#include <QNetworkDiskCache>
#include <QImage>
#include <QSettings>
#include <QDateTime>
#include <QScopedPointer>

// TODO: remove, it's only needed by a single debug message
#include <fcgi_stdio.h>
#include <stdlib.h>



// Server status static initialisers.
// Default values are for C++, SIP bindings will override their
// options in in init()

QString* QgsServer::sConfigFilePath = nullptr;
QgsCapabilitiesCache* QgsServer::sCapabilitiesCache = nullptr;
QgsServerInterfaceImpl* QgsServer::sServerInterface = nullptr;
// Initialization must run once for all servers
bool QgsServer::sInitialised =  false;
QgsServerSettings QgsServer::sSettings;

QgsServiceRegistry QgsServer::sServiceRegistry;

QgsServer::QgsServer( )
{
  // QgsApplication must exist
  if ( qobject_cast<QgsApplication*>( qApp ) == nullptr )
  {
    qFatal( "A QgsApplication must exist before a QgsServer instance can be created." );
    abort();
  }
  init();
}

QString& QgsServer::serverName()
{
  static QString* name = new QString( QStringLiteral( "qgis_server" ) );
  return *name;
}


QFileInfo QgsServer::defaultAdminSLD()
{
  return QFileInfo( QStringLiteral( "admin.sld" ) );
}


/**
 * @brief QgsServer::setupNetworkAccessManager
 */
void QgsServer::setupNetworkAccessManager()
{
  QSettings settings;
  QgsNetworkAccessManager *nam = QgsNetworkAccessManager::instance();
  QNetworkDiskCache *cache = new QNetworkDiskCache( nullptr );
  qint64 cacheSize = sSettings.cacheSize();
  QString cacheDirectory = sSettings.cacheDirectory();
  cache->setCacheDirectory( cacheDirectory );
  cache->setMaximumCacheSize( cacheSize );
  QgsMessageLog::logMessage( QStringLiteral( "cacheDirectory: %1" ).arg( cache->cacheDirectory() ), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  QgsMessageLog::logMessage( QStringLiteral( "maximumCacheSize: %1" ).arg( cache->maximumCacheSize() ), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  nam->setCache( cache );
}

/**
 * @brief QgsServer::defaultProjectFile
 * @return the default project file
 */
QFileInfo QgsServer::defaultProjectFile()
{
  QDir currentDir;
  fprintf( FCGI_stderr, "current directory: %s\n", currentDir.absolutePath().toUtf8().constData() );
  QStringList nameFilterList;
  nameFilterList << QStringLiteral( "*.qgs" );
  QFileInfoList projectFiles = currentDir.entryInfoList( nameFilterList, QDir::Files, QDir::Name );
  for ( int x = 0; x < projectFiles.size(); x++ )
  {
    QgsMessageLog::logMessage( projectFiles.at( x ).absoluteFilePath(), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  }
  if ( projectFiles.size() < 1 )
  {
    return QFileInfo();
  }
  return projectFiles.at( 0 );
}

/**
 * @brief QgsServer::printRequestParameters prints the request parameters
 * @param parameterMap
 * @param logLevel
 */
void QgsServer::printRequestParameters( const QMap< QString, QString>& parameterMap, QgsMessageLog::MessageLevel logLevel )
{
  if ( logLevel > QgsMessageLog::INFO )
  {
    return;
  }

  QMap< QString, QString>::const_iterator pIt = parameterMap.constBegin();
  for ( ; pIt != parameterMap.constEnd(); ++pIt )
  {
    QgsMessageLog::logMessage( pIt.key() + ":" + pIt.value(), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  }
}

/**
 * @brief QgsServer::printRequestInfos prints debug information about the request
 */
void QgsServer::printRequestInfos()
{
  QgsMessageLog::logMessage( QStringLiteral( "********************new request***************" ), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  if ( getenv( "REMOTE_ADDR" ) )
  {
    QgsMessageLog::logMessage( "remote ip: " + QString( getenv( "REMOTE_ADDR" ) ), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  }
  if ( getenv( "REMOTE_HOST" ) )
  {
    QgsMessageLog::logMessage( "remote ip: " + QString( getenv( "REMOTE_HOST" ) ), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  }
  if ( getenv( "REMOTE_USER" ) )
  {
    QgsMessageLog::logMessage( "remote user: " + QString( getenv( "REMOTE_USER" ) ), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  }
  if ( getenv( "REMOTE_IDENT" ) )
  {
    QgsMessageLog::logMessage( "REMOTE_IDENT: " + QString( getenv( "REMOTE_IDENT" ) ), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  }
  if ( getenv( "CONTENT_TYPE" ) )
  {
    QgsMessageLog::logMessage( "CONTENT_TYPE: " + QString( getenv( "CONTENT_TYPE" ) ), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  }
  if ( getenv( "AUTH_TYPE" ) )
  {
    QgsMessageLog::logMessage( "AUTH_TYPE: " + QString( getenv( "AUTH_TYPE" ) ), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  }
  if ( getenv( "HTTP_USER_AGENT" ) )
  {
    QgsMessageLog::logMessage( "HTTP_USER_AGENT: " + QString( getenv( "HTTP_USER_AGENT" ) ), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  }
  if ( getenv( "HTTP_PROXY" ) )
  {
    QgsMessageLog::logMessage( "HTTP_PROXY: " + QString( getenv( "HTTP_PROXY" ) ), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  }
  if ( getenv( "HTTPS_PROXY" ) )
  {
    QgsMessageLog::logMessage( "HTTPS_PROXY: " + QString( getenv( "HTTPS_PROXY" ) ), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  }
  if ( getenv( "NO_PROXY" ) )
  {
    QgsMessageLog::logMessage( "NO_PROXY: " + QString( getenv( "NO_PROXY" ) ), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  }
}

/**
 * @brief QgsServer::configPath
 * @param defaultConfigPath
 * @param parameters
 * @return config file path
 */
QString QgsServer::configPath( const QString& defaultConfigPath, const QMap<QString, QString>& parameters )
{
  QString cfPath( defaultConfigPath );
  QString projectFile = sSettings.projectFile();
  if ( !projectFile.isEmpty() )
  {
    cfPath = projectFile;
    QgsDebugMsg( QString( "QGIS_PROJECT_FILE:%1" ).arg( cfPath ) );
  }
  else
  {
    QMap<QString, QString>::const_iterator paramIt = parameters.find( QStringLiteral( "MAP" ) );
    if ( paramIt == parameters.constEnd() )
    {
      QgsMessageLog::logMessage( QStringLiteral( "Using default configuration file path: %1" ).arg( defaultConfigPath ), QStringLiteral( "Server" ), QgsMessageLog::INFO );
    }
    else
    {
      cfPath = paramIt.value();
      QgsDebugMsg( QString( "MAP:%1" ).arg( cfPath ) );
    }
  }
  return cfPath;
}


/**
 * Server initialization
 */
bool QgsServer::init( )
{
  if ( sInitialised )
  {
    return false;
  }

  QCoreApplication::setOrganizationName( QgsApplication::QGIS_ORGANIZATION_NAME );
  QCoreApplication::setOrganizationDomain( QgsApplication::QGIS_ORGANIZATION_DOMAIN );
  QCoreApplication::setApplicationName( QgsApplication::QGIS_APPLICATION_NAME );

#if !defined(Q_OS_WIN)
  // Set the QGIS_PREFIX_PATH environnemnt instead of calling directly
  // setPrefixPath: this will allow running server from build directory
  // and get the paths accordingly
  setenv( "QGIS_PREFIX_PATH", CMAKE_INSTALL_PREFIX, 1 );
#endif

  //Default prefix path may be altered by environment variable
  QgsApplication::init();

#if defined(SERVER_SKIP_ECW)
  QgsMessageLog::logMessage( "Skipping GDAL ECW drivers in server.", "Server", QgsMessageLog::INFO );
  QgsApplication::skipGdalDriver( "ECW" );
  QgsApplication::skipGdalDriver( "JP2ECW" );
#endif

  // reload settings to take into account QCoreApplication and QgsApplication
  // configuration
  sSettings.load();

  // init and configure logger
  QgsServerLogger::instance();
  QgsServerLogger::instance()->setLogLevel( sSettings.logLevel() );
  QgsServerLogger::instance()->setLogFile( sSettings.logFile() );

  // init and configure cache
  QgsMSLayerCache::instance();
  QgsMSLayerCache::instance()->setMaxCacheLayers( sSettings.maxCacheLayers() );

  // log settings currently used
  sSettings.logSummary();

  setupNetworkAccessManager();
  QDomImplementation::setInvalidDataPolicy( QDomImplementation::DropInvalidChars );

  // Instantiate the plugin directory so that providers are loaded
  QgsProviderRegistry::instance( QgsApplication::pluginPath() );
  QgsMessageLog::logMessage( "Prefix  PATH: " + QgsApplication::prefixPath(), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  QgsMessageLog::logMessage( "Plugin  PATH: " + QgsApplication::pluginPath(), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  QgsMessageLog::logMessage( "PkgData PATH: " + QgsApplication::pkgDataPath(), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  QgsMessageLog::logMessage( "User DB PATH: " + QgsApplication::qgisUserDbFilePath(), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  QgsMessageLog::logMessage( "Auth DB PATH: " + QgsApplication::qgisAuthDbFilePath(), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  QgsMessageLog::logMessage( "SVG PATHS: " + QgsApplication::svgPaths().join( QDir::separator() ), QStringLiteral( "Server" ), QgsMessageLog::INFO );

  QgsApplication::createDB(); //init qgis.db (e.g. necessary for user crs)

  // Instantiate authentication system
  //   creates or uses qgis-auth.db in ~/.qgis3/ or directory defined by QGIS_AUTH_DB_DIR_PATH env variable
  //   set the master password as first line of file defined by QGIS_AUTH_PASSWORD_FILE env variable
  //   (QGIS_AUTH_PASSWORD_FILE variable removed from environment after accessing)
  QgsAuthManager::instance()->init( QgsApplication::pluginPath() );

  QString defaultConfigFilePath;
  QFileInfo projectFileInfo = defaultProjectFile(); //try to find a .qgs file in the server directory
  if ( projectFileInfo.exists() )
  {
    defaultConfigFilePath = projectFileInfo.absoluteFilePath();
    QgsMessageLog::logMessage( "Using default project file: " + defaultConfigFilePath, QStringLiteral( "Server" ), QgsMessageLog::INFO );
  }
  else
  {
    QFileInfo adminSLDFileInfo = defaultAdminSLD();
    if ( adminSLDFileInfo.exists() )
    {
      defaultConfigFilePath = adminSLDFileInfo.absoluteFilePath();
    }
  }
  // Store the config file path
  sConfigFilePath = new QString( defaultConfigFilePath );

  //create cache for capabilities XML
  sCapabilitiesCache = new QgsCapabilitiesCache();

#ifdef ENABLE_MS_TESTS
  QgsFontUtils::loadStandardTestFonts( QStringList() << QStringLiteral( "Roman" ) << QStringLiteral( "Bold" ) );
#endif

  QgsEditorWidgetRegistry::initEditors();

  sServerInterface = new QgsServerInterfaceImpl( sCapabilitiesCache, &sServiceRegistry, &sSettings );

  // Load service module
  QString modulePath =  QgsApplication::libexecPath() + "server";
  qDebug() << "Initializing server modules from " << modulePath << endl;
  sServiceRegistry.init( modulePath,  sServerInterface );

  sInitialised = true;
  QgsMessageLog::logMessage( QStringLiteral( "Server initialized" ), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  return true;
}



void QgsServer::putenv( const QString &var, const QString &val )
{
#ifdef _MSC_VER
  _putenv_s( var.toStdString().c_str(), val.toStdString().c_str() );
#else
  setenv( var.toStdString().c_str(), val.toStdString().c_str(), 1 );
#endif
  sSettings.load( var );
}

/**
 * @brief Handles the request
 * @param queryString
 * @return response headers and body
 */

void QgsServer::handleRequest( QgsServerRequest& request, QgsServerResponse& response )
{
  QgsMessageLog::MessageLevel logLevel = QgsServerLogger::instance()->logLevel();
  QTime time; //used for measuring request time if loglevel < 1
  QgsProject::instance()->removeAllMapLayers();

  qApp->processEvents();

  if ( logLevel == QgsMessageLog::INFO )
  {
    time.start();
    printRequestInfos();
  }

  //Pass the filters to the requestHandler, this is needed for the following reasons:
  // Allow server request to call sendResponse plugin hook if enabled
  QgsFilterResponseDecorator theResponse( sServerInterface->filters(), response );

  //Request handler
  QgsRequestHandler theRequestHandler( request, theResponse );

  try
  {
    // TODO: split parse input into plain parse and processing from specific services
    theRequestHandler.parseInput();
  }
  catch ( QgsMapServiceException& e )
  {
    QgsMessageLog::logMessage( "Parse input exception: " + e.message(), QStringLiteral( "Server" ), QgsMessageLog::CRITICAL );
    theRequestHandler.setServiceException( e );
  }

  // Set the request handler into the interface for plugins to manipulate it
  sServerInterface->setRequestHandler( &theRequestHandler );

  // Call  requestReady() method (if enabled)
  theResponse.start();

  QMap<QString, QString> parameterMap = request.parameters();
  printRequestParameters( parameterMap, logLevel );

  QgsAccessControl* accessControl = sServerInterface->accessControls();

  //Config file path
  QString configFilePath = configPath( *sConfigFilePath, parameterMap );

#ifdef HAVE_SERVER_PYTHON_PLUGINS
  // XXX Why this is enabled only fol plugins ?
  sServerInterface->setConfigFilePath( configFilePath );
#endif

  //Service parameter
  QString serviceString = parameterMap.value( QStringLiteral( "SERVICE" ) );

  if ( serviceString.isEmpty() )
  {
    // SERVICE not mandatory for WMS 1.3.0 GetMap & GetFeatureInfo
    QString requestString = parameterMap.value( QStringLiteral( "REQUEST" ) );
    if ( requestString == QLatin1String( "GetMap" ) || requestString == QLatin1String( "GetFeatureInfo" ) )
    {
      serviceString = QStringLiteral( "WMS" );
    }
  }

  QString versionString = parameterMap.value( QStringLiteral( "VERSION" ) );

  //possibility for client to suggest a download filename
  QString outputFileName = parameterMap.value( QStringLiteral( "FILE_NAME" ) );
  if ( !outputFileName.isEmpty() )
  {
    theRequestHandler.setHeader( QStringLiteral( "Content-Disposition" ), "attachment; filename=\"" + outputFileName + "\"" );
  }

  // Enter core services main switch
  if ( !theRequestHandler.exceptionRaised() )
  {
    // Lookup for service

    QgsService* service = sServiceRegistry.getService( serviceString, versionString );
    if ( service )
    {
      service->executeRequest( request, theResponse );
    }
    else if ( serviceString == QLatin1String( "WCS" ) )
    {
      QgsWCSProjectParser* p = QgsConfigCache::instance()->wcsConfiguration(
                                 configFilePath
                                 , accessControl
                               );
      if ( !p )
      {
        theRequestHandler.setServiceException( QgsMapServiceException( QStringLiteral( "Project file error" ), QStringLiteral( "Error reading the project file" ) ) );
      }
      else
      {
        QgsWCSServer wcsServer(
          configFilePath
          , sSettings
          , parameterMap
          , p
          , &theRequestHandler
          , accessControl
        );
        wcsServer.executeRequest();
      }
    }
    else if ( serviceString == QLatin1String( "WFS" ) )
    {
      QgsWfsProjectParser* p = QgsConfigCache::instance()->wfsConfiguration(
                                 configFilePath
                                 , accessControl
                               );
      if ( !p )
      {
        theRequestHandler.setServiceException( QgsMapServiceException( QStringLiteral( "Project file error" ), QStringLiteral( "Error reading the project file" ) ) );
      }
      else
      {
        QgsWfsServer wfsServer(
          configFilePath
          , sSettings
          , parameterMap
          , p
          , &theRequestHandler
          , accessControl
        );
        wfsServer.executeRequest();
      }
    }
    else
    {
      theRequestHandler.setServiceException( QgsMapServiceException( QStringLiteral( "Service configuration error" ), QStringLiteral( "Service unknown or unsupported" ) ) );
    } // end switch
  } // end if not exception raised

  // Terminate the response
  theResponse.finish();

  // We are done using theRequestHandler in plugins, make sure we don't access
  // to a deleted request handler from Python bindings
  sServerInterface->clearRequestHandler();


  if ( logLevel == QgsMessageLog::INFO )
  {
    QgsMessageLog::logMessage( "Request finished in " + QString::number( time.elapsed() ) + " ms", QStringLiteral( "Server" ), QgsMessageLog::INFO );
  }
}

QPair<QByteArray, QByteArray> QgsServer::handleRequest( const QString& queryString )
{
  /*
   * This is mainly for python bindings, passing QUERY_STRING
   * to handleRequest without using os.environment
   *
   * XXX To be removed because query string is now handled in QgsServerRequest
   *
   */
  if ( ! queryString.isEmpty() )
    putenv( QStringLiteral( "QUERY_STRING" ), queryString );

  QgsServerRequest::Method method = QgsServerRequest::GetMethod;
  QByteArray ba;

  // XXX This is mainly used in tests
  char* requestMethod = getenv( "REQUEST_METHOD" );
  if ( requestMethod && strcmp( requestMethod, "POST" ) == 0 )
  {
    method = QgsServerRequest::PostMethod;
    const char* data = getenv( "REQUEST_BODY" );
    if ( data )
    {
      ba.append( data );
    }
  }

  QUrl url;
  url.setQuery( queryString );

  QgsBufferServerRequest request( url, method, &ba );
  QgsBufferServerResponse response;

  handleRequest( request, response );

  /*
   * XXX For compatibility only:
   * We should return a (moved) QgsBufferServerResponse instead
   */
  QByteArray headerBuffer;
  QMap<QString, QString>::const_iterator it;
  for ( it = response.headers().constBegin(); it != response.headers().constEnd(); ++it )
  {
    headerBuffer.append( it.key().toUtf8() );
    headerBuffer.append( ": " );
    headerBuffer.append( it.value().toUtf8() );
    headerBuffer.append( "\n" );
  }
  headerBuffer.append( "\n" );

  // TODO: check that this is not an evil bug!
  return QPair<QByteArray, QByteArray>( headerBuffer, response.body() );

}

#ifdef HAVE_SERVER_PYTHON_PLUGINS
void QgsServer::initPython()
{
  // Init plugins
  if ( ! QgsServerPlugins::initPlugins( sServerInterface ) )
  {
    QgsMessageLog::logMessage( QStringLiteral( "No server python plugins are available" ), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  }
  else
  {
    QgsMessageLog::logMessage( QStringLiteral( "Server python plugins loaded" ), QStringLiteral( "Server" ), QgsMessageLog::INFO );
  }
}
#endif

