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

#include <QString>

class CORE_EXPORT QgsAuxiliaryStorage : public QgsVectorLayer
{
    Q_OBJECT

  public:
    enum AuxiliaryField
    {
      X = 0,
      Y = 1
    };

    QgsAuxiliaryStorage( const QString &filename, const QgsVectorLayer &layer, const QString &table );
    virtual ~QgsAuxiliaryStorage();

    //! QgsAuxiliaryStorage cannot be copied.
    QgsAuxiliaryStorage( const QgsAuxiliaryStorage &rhs ) = delete;
    //! QgsAuxiliaryStorage cannot be copied.
    QgsAuxiliaryStorage &operator=( QgsAuxiliaryStorage const &rhs ) = delete;

    static QgsAuxiliaryStorage *create( const QgsVectorLayer &layer );

  private:
    static bool createDB( const QString &filename, const QString &table );
    static bool initializeSpatialMetadata( sqlite3 *sqlite_handle, QString &err );
    static bool createDataDefinedPropertyTable( const QString &table, sqlite3 *sqlite_handle, QString &err );

    //QString mFileName;
    //sqlite3 *mSqliteHandler;
};

#endif
