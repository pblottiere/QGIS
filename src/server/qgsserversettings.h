/***************************************************************************
                    qgsserversettings.h
                    -------------------
  begin                : December 19, 2016
  copyright            : (C) 2016 by Paul Blottiere
  email                : paul dot blottiere at oslandia dot com

***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#ifndef QGSSERVERSETTINGS_H
#define QGSSERVERSETTINGS_H

#include <QObject>
#include <QMetaEnum>

#include "qgsmessagelog.h"
#include "qgis_server.h"

/**
  * QgsServerSettingsEnv provides some enum describing the environment
  * currently supported for configuration.
  * @note added in QGIS 3.0
  */
class QgsServerSettingsEnv : public QObject
{
    Q_OBJECT

  public:
    enum Source
    {
      DEFAULT_VALUE,
      ENVIRONMENT_VARIABLE,
      INI_FILE
    };
    Q_ENUM( Source )

    enum EnvVar
    {
      QGIS_OPTIONS_PATH,
      QGIS_SERVER_PARALLEL_RENDERING,
      QGIS_SERVER_MAX_THREADS,
      QGIS_SERVER_LOG_LEVEL,
      QGIS_SERVER_LOG_FILE,
      QGIS_PROJECT_FILE,
      MAX_CACHE_LAYERS,
      QGIS_SERVER_CACHE_DIRECTORY,
      QGIS_SERVER_CACHE_SIZE
    };
    Q_ENUM( EnvVar )
};

/** \ingroup server
 * QgsServerSettings provides a way to retrieve settings by prioritizing
 * according to environment variables, ini file and default values.
 * @note added in QGIS 3.0
 */
class SERVER_EXPORT QgsServerSettings
{
  public:
    struct Setting
    {
      QgsServerSettingsEnv::EnvVar envVar;
      QgsServerSettingsEnv::Source src;
      QString descr;
      QString iniKey;
      QVariant::Type type;
      QVariant defaultVal;
      QVariant val;
    };

    /** Constructor.
      */
    QgsServerSettings();

    /** Load settings according to current environment variables.
      */
    void load();

    /** Load setting for a specific environment variable name.
      * @return true if loading is successful, false in case of an invalid name.
      */
    bool load( const QString& envVarName );

    /** Log a summary of settings curently loaded.
      */
    void logSummary() const;

    /** Returns the ini file loaded by QSetting.
      * @return the path of the ini file or an empty string if none is loaded.
      */
    QString iniFile() const;

    /** Returns parallel rendering setting.
      * @return true if parallel rendering is activated, false otherwise.
      */
    bool parallelRendering() const;

    /** Returns the maximum number of threads to use.
      * @return the number of threads.
      */
    int maxThreads() const;

    /**
      * Returns the maximum number of cached layers.
      * @return the number of cached layers.
      */
    int maxCacheLayers() const;

    /** Returns the log level.
      * @return the log level.
      */
    QgsMessageLog::MessageLevel logLevel() const;

    /** Returns the QGS project file to use.
      * @return the path of the QGS project or an empty string if none is defined.
      */
    QString projectFile() const;

    /** Returns the log file.
      * @return the path of the log file or an empty string if none is defined.
      */
    QString logFile() const;

    /** Returns the cache size.
      * @return the cache size.
      */
    qint64 cacheSize() const;

    /** Returns the cache directory.
      * @return the directory.
      */
    QString cacheDirectory() const;

  private:
    void initSettings();
    QVariant value( QgsServerSettingsEnv::EnvVar envVar ) const;
    QMap<QgsServerSettingsEnv::EnvVar, QString> getEnv() const;
    void loadQSettings( const QString& envOptPath ) const;
    void prioritize( const QMap<QgsServerSettingsEnv::EnvVar, QString>& env );

    QMap< QgsServerSettingsEnv::EnvVar, Setting > mSettings;
};

#endif
