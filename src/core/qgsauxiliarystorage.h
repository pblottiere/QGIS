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

#include <QString>

class CORE_EXPORT QgsAuxiliaryStorageJoin : public QgsVectorLayer
{
    Q_OBJECT

  public:
    struct QgsAuxiliaryStorageField
    {
      QString mTarget;
      QString mProperty;
      QString mType;
    };

    QgsAuxiliaryStorageJoin( const QString &pkField, const QString &filename, const QString &table, QgsVectorLayer *layer, bool yetFilled = false );
    virtual ~QgsAuxiliaryStorageJoin();

    //! QgsAuxiliaryStorage cannot be copied.
    QgsAuxiliaryStorageJoin( const QgsAuxiliaryStorageJoin &rhs ) = delete;

    //! QgsAuxiliaryStorage cannot be copied.
    QgsAuxiliaryStorageJoin &operator=( QgsAuxiliaryStorageJoin const &rhs ) = delete;

    QgsVectorLayerJoinInfo joinInfo() const;

    bool propertyExists( const QgsPropertyDefinition &definition ) const;

    QString propertyName( const QgsPropertyDefinition &definition ) const;

    bool createProperty( const QgsPropertyDefinition &definition );

    QString propertyFieldName( const QgsPropertyDefinition &definition ) const;

    bool changeAttributeValue( QgsFeatureId fid, int field, const QVariant &newValue, const QVariant &oldValue = QVariant() );

    QList<QgsAuxiliaryStorageField> storageFields() const;

  private:
    QgsVectorLayer *mLayer;
    QgsVectorLayerJoinInfo mJoinInfo;
};

class CORE_EXPORT QgsAuxiliaryStorage
{
  public:

    QgsAuxiliaryStorage( const QgsProject &project );

    QgsAuxiliaryStorage( const QString &filename = QString() );

    virtual ~QgsAuxiliaryStorage();

    bool isValid() const;

    QString fileName() const;

    bool isNew() const;

    void saveAs( const QString &filename ) const;

    void saveAs( const QgsProject &project ) const;

    QgsAuxiliaryStorageJoin *createStorageLayer( QgsVectorLayer *layer );

    QgsAuxiliaryStorageJoin *createStorageLayer( const QgsField &field, QgsVectorLayer *layer );

    static QString extension();

  private:
    sqlite3 *open( const QString &filename = QString() );
    sqlite3 *open( const QgsProject &project );
    void close( sqlite3 *handler );

    static QString filenameForProject( const QgsProject &project );
    static sqlite3 *createDB( const QString &filename );
    static sqlite3 *openDB( const QString &filename );
    static bool initializeSpatialMetadata( sqlite3 *handler );
    static bool createTable( const QString &type, const QString &table, sqlite3 *handler );
    static bool tableExists( const QString &table, sqlite3 *handler );

    static bool exec( const QString &sql, sqlite3 *handler );
    static void debugMsg( const QString &sql, sqlite3 *handler );

    bool mValid;
    bool mTmp;
    QString mFileName;
};

#endif
