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

/**
 * \class QgsAuxiliaryField
 *
 * \ingroup core
 *
 * \brief Class allowing to manage fields from of auxiliary layers
 *
 * \since QGIS 3.0
 */
class CORE_EXPORT QgsAuxiliaryField : public QgsField
{
  public:

    /**
     * Constructor
     *
     * \param def Definition of the property to be stored by this auxiliary
     *  field.
     */
    QgsAuxiliaryField( const QgsPropertyDefinition &def );

    /**
     * Destructor
     */
    virtual ~QgsAuxiliaryField() = default;

    /**
     * Returns the property definition corresponding to this field.
     */
    QgsPropertyDefinition propertyDefinition() const;

    /**
     * Returns the name of the field.
     */
    using QgsField::name SIP_SKIP;

    /**
     * Returns the name of the auxiliary field for a property definition.
     *
     * \returns def The property definition
     * \returns joined The join prefix is tok into account if true
     */
    static QString name( const QgsPropertyDefinition &def, bool joined = false );

  private:
    QgsAuxiliaryField( const QgsField &f ); // only for auxiliary layer

    void init( const QgsPropertyDefinition &def );

    QgsPropertyDefinition mPropertyDefinition;

    friend class QgsAuxiliaryLayer;
};

typedef QList<QgsAuxiliaryField> QgsAuxiliaryFields;

/**
 * \class QgsAuxiliaryLayer
 *
 * \ingroup core
 *
 * \brief Class allowing to manage the auxiliary storage for a vector layer
 *
 * \since QGIS 3.0
 */
class CORE_EXPORT QgsAuxiliaryLayer : public QgsVectorLayer
{
    Q_OBJECT

  public:

    /**
     * Constructor
     *
     * \param pkField The primary key to use for joining
     * \param filename The database path
     * \param table The table name
     * \param vlayer The target vector layer
     */
    QgsAuxiliaryLayer( const QString &pkField, const QString &filename, const QString &table, const QgsVectorLayer *vlayer );

    /**
     * Destructor
     */
    virtual ~QgsAuxiliaryLayer() = default;

    /**
     * Copy constructor deactivated
     */
    QgsAuxiliaryLayer( const QgsAuxiliaryLayer &rhs ) = delete;

    QgsAuxiliaryLayer &operator=( QgsAuxiliaryLayer const &rhs ) = delete;

    /**
     * An auxiliary layer is not spatial. This method returns a spatial
     * representation of auxiliary data.
     *
     * \returns A new spatial vector layer
     */
    QgsVectorLayer *toSpatialLayer() const;

    /**
     * Delete all features from the layer. Changes are automatically committed
     * and the layer remains editable.
     *
      \returns true if changes are committed without error, false otherwise.
     */
    bool clear();

    /**
     * Returns information to use for joining with primary key and so on.
     */
    QgsVectorLayerJoinInfo joinInfo() const;

    /**
     * Returns true if the property is stored in the layer yet, false
     * otherwise.
     *
     * \param definition The property definition to check
     *
     * \returns true if the property is stored, false otherwise
     */
    bool exists( const QgsPropertyDefinition &definition ) const;

    /**
     * Add an an auxiliary field for the given property.
     *
     * \param definition The definition of the property to add
     *
     * \returns true if the auxiliary field is well added, false otherwise
     */
    bool addAuxiliaryField( const QgsPropertyDefinition &definition );

    /**
     * Returns a list of all auxiliary fields currently managed by the layer.
     */
    QgsAuxiliaryFields auxiliaryFields() const;

    /**
     * Remove attribute from the layer and commit changes. The layer remains
     * editable.
     *
     * \param attr The index of the attribute to remove
     *
     * \returns true if the attribute is well deleted, false otherwise
     */
    virtual bool deleteAttribute( int attr ) override;

  private:
    QgsVectorLayerJoinInfo mJoinInfo;
    const QgsVectorLayer *mLayer;
};

/**
 * \class QgsAuxiliaryStorage
 *
 * \ingroup core
 *
 * \brief Class providing some utility methods to manage auxiliary storage.
 *
 * \since QGIS 3.0
 */
class CORE_EXPORT QgsAuxiliaryStorage
{
  public:

    /**
     * Constructor.
     *
     * \param project The project for which the auxiliary storage has to be used
     * \param copy Parameter indicating if a copy of the database has to be used
     */
    QgsAuxiliaryStorage( const QgsProject &project, bool copy = true );

    /**
     * Constructor.
     *
     * \param filename The path of the database
     * \param copy Parameter indicating if a copy of the database has to be used
     */
    QgsAuxiliaryStorage( const QString &filename = QString(), bool copy = true );

    /**
     * Destructor.
     */
    virtual ~QgsAuxiliaryStorage();

    /**
     * Returns the status of the auxiliary storage currently definied.
     *
     * \returns true if the auxiliary storage is valid, false otherwise
     */
    bool isValid() const;

    /**
     * Returns the target filename of the database.
     */
    QString fileName() const;

    /**
     * Returns the path of current database used. It may be different from the
     * target filename if the auxiliary storage is opened in copy mode.
     */
    QString currentFileName() const;

    /**
     * Saves the current database to a new path.
     *
     * \returns true if everything is saved, false otherwise
     */
    bool saveAs( const QString &filename ) const;

    /**
     * Saves the current database to a new path for a specific project.
     * Actually, the current filename of the project is used to deduce the
     * path of the database to save.
     *
     * \returns true if everything is saved, false otherwise
     */
    bool saveAs( const QgsProject &project ) const;

    /**
     * Saves the current database.
     *
     * \returns true if everything is saved, false otherwise
     */
    bool save() const;

    /**
     * Creates an auxiliary layer for a vector layer. A new table is created if
     * necessary. The first primary key coming from the data provider is used
     * to construct the auxiliary layer. If there's no primary key defined in
     * the data source, a nullptr is returned instead of a new layer.
     *
     * \param layer The vector layer for which the auxiliary layer has to be created
     *
     * \returns A new auxiliary layer.
     */
    QgsAuxiliaryLayer *createAuxiliaryLayer( const QgsVectorLayer *layer );

    /**
     * Creates an auxiliary layer for a vector layer. A new table is created if
     * necessary. The primary key to use to construct the auxiliary layer is
     * given in parameter.
     *
     * \param layer The vector layer for which the auxiliary layer has to be created
     *
     * \returns A new auxiliary layer.
     */
    QgsAuxiliaryLayer *createAuxiliaryLayer( const QgsField &field, const QgsVectorLayer *layer );

    /**
     * Removes a table from the auxiliary storage.
     *
     * \returns true if the table is well deleted, false otherwise
     */
    static bool deleteTable( const QgsDataSourceUri &uri );

    /**
     * Returns the extension used for auxiliary databases.
     */
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
