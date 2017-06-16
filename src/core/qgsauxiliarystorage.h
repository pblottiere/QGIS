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
#include "qgsproperty.h"

#include <QString>

class CORE_EXPORT QgsAuxiliaryStorageJoin : public QgsVectorLayer
{
    Q_OBJECT

  public:
    QgsAuxiliaryStorageJoin( const QString &filename, const QString &table, const QgsVectorLayer &layer, bool yetFilled = false );
    virtual ~QgsAuxiliaryStorageJoin();

    //! QgsAuxiliaryStorage cannot be copied.
    QgsAuxiliaryStorageJoin( const QgsAuxiliaryStorageJoin &rhs ) = delete;

    //! QgsAuxiliaryStorage cannot be copied.
    QgsAuxiliaryStorageJoin &operator=( QgsAuxiliaryStorageJoin const &rhs ) = delete;

    bool createProperty( const QgsPropertyDefinition &definition );

    bool propertyExists( const QgsPropertyDefinition &definition ) const;

    QString propertyFieldName( const QgsPropertyDefinition &definition ) const;

};

class CORE_EXPORT QgsAuxiliaryStorage
{
  public:
    QgsAuxiliaryStorage();
    virtual ~QgsAuxiliaryStorage();

    bool isValid() const;
    QString fileName() const;
    QgsAuxiliaryStorageJoin *createJoin( const QgsVectorLayer &layer );
    bool open( const QString &filename = QString() );
    void close();

  private:
    bool createDB( const QString &filename );
    bool openDB( const QString &filename );
    bool initializeSpatialMetadata( QString &err );

    bool createTableIfNotExists( const QString &table );
    bool tableExists( const QString &table ) const;

    bool mValid;
    QString mFileName;
    sqlite3 *mSqliteHandler;
};

#endif
