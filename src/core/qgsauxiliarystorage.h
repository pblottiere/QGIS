/***************************************************************************
                          qgsauxiliarystorage.h  -  description
                           -------------------
    begin                : May 18, 2017
    copyright            : (C) 2017 by Paul Blottiere
    email                : paul.blottiere@oslandia.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSAUXILIARYSTORAGE_H
#define QGSAUXILIARYSTORAGE_H

#include "qgis_core.h"
#include "qgsvectorlayer.h"
#include "qgsvectorlayerjoininfo.h"
#include "qgsproperty.h"
#include "qgsdatasourceuri.h"

#include <QString>

class CORE_EXPORT QgsAuxiliaryField : public QgsField
{
  public:
    QgsAuxiliaryField( const QgsPropertyDefinition &def );

    QgsPropertyDefinition propertyDefinition() const;

    using QgsField::name SIP_SKIP;

    static QString name( const QgsPropertyDefinition &def, bool joined = false );

  private:
    QgsAuxiliaryField( const QgsField &f ); // only for auxiliary layer

    void init( const QgsPropertyDefinition &def );

    QgsPropertyDefinition mPropertyDefinition;

    friend class QgsAuxiliaryLayer;
};

typedef QList<QgsAuxiliaryField> QgsAuxiliaryFields;

class CORE_EXPORT QgsAuxiliaryLayer : public QgsVectorLayer
{
    Q_OBJECT

  public:
    QgsAuxiliaryLayer( const QString &pkField, const QString &filename, const QString &table, const QgsVectorLayer *vlayer );

    virtual ~QgsAuxiliaryLayer();

    //! QgsAuxiliaryStorage cannot be copied.
    QgsAuxiliaryLayer( const QgsAuxiliaryLayer &rhs ) = delete;

    //! QgsAuxiliaryStorage cannot be copied.
    QgsAuxiliaryLayer &operator=( QgsAuxiliaryLayer const &rhs ) = delete;

    QgsVectorLayer *toSpatialLayer() const;

    bool clear();

    QgsVectorLayerJoinInfo joinInfo() const;

    bool exists( const QgsPropertyDefinition &definition ) const;

    bool addAuxiliaryField( const QgsPropertyDefinition &definition );

    QgsAuxiliaryFields auxiliaryFields() const;

    virtual bool deleteAttribute( int attr ) override;

  private:
    QgsVectorLayerJoinInfo mJoinInfo;
    const QgsVectorLayer *mLayer;
};

class CORE_EXPORT QgsAuxiliaryStorage
{
  public:

    QgsAuxiliaryStorage( const QgsProject &project, bool copy = true );

    QgsAuxiliaryStorage( const QString &filename = QString(), bool copy = true );

    virtual ~QgsAuxiliaryStorage();

    bool isValid() const;

    QString fileName() const;

    QString currentFileName() const;

    bool saveAs( const QString &filename ) const;

    bool saveAs( const QgsProject &project ) const;

    bool save() const;

    QgsAuxiliaryLayer *createAuxiliaryLayer( const QgsVectorLayer *layer );

    QgsAuxiliaryLayer *createAuxiliaryLayer( const QgsField &field, const QgsVectorLayer *layer );

    static bool deleteTable( const QgsDataSourceUri &uri );

    static QString extension();

  private:
    sqlite3 *open( const QString &filename = QString() );
    sqlite3 *open( const QgsProject &project );

    void initTmpFileName();

    static QString filenameForProject( const QgsProject &project );
    static sqlite3 *createDB( const QString &filename );
    static sqlite3 *openDB( const QString &filename );
    static void close( sqlite3 *handler );
    static bool createTable( const QString &type, const QString &table, sqlite3 *handler );
    static bool tableExists( const QString &table, sqlite3 *handler );

    static bool exec( const QString &sql, sqlite3 *handler );
    static void debugMsg( const QString &sql, sqlite3 *handler );

    bool mValid;
    QString mFileName; // original filename
    QString mTmpFileName; // temporary filename used in copy mode
    bool mCopy;
};

#endif
