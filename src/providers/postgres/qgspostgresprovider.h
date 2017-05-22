/***************************************************************************
      qgspostgresprovider.h  -  Data provider for PostgreSQL/PostGIS layers
                             -------------------
    begin                : Jan 2, 2004
    copyright            : (C) 2003 by Gary E.Sherman
    email                : sherman at mrcc.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSPOSTGRESPROVIDER_H
#define QGSPOSTGRESPROVIDER_H

#include "qgsvectordataprovider.h"
#include "qgsrectangle.h"
#include "qgsvectorlayerexporter.h"
#include "qgspostgresconn.h"
#include "qgsfields.h"
#include <memory>

class QgsFeature;
class QgsField;
class QgsGeometry;

class QgsPostgresFeatureIterator;
class QgsPostgresSharedData;
class QgsPostgresTransaction;

#include "qgsdatasourceuri.h"

/**
  \class QgsPostgresProvider
  \brief Data provider for PostgreSQL/PostGIS layers.

  This provider implements the
  interface defined in the QgsDataProvider class to provide access to spatial
  data residing in a PostgreSQL/PostGIS enabled database.
  */
class QgsPostgresProvider : public QgsVectorDataProvider
{
    Q_OBJECT

  public:

    /** Import a vector layer into the database
     * \param options options for provider, specified via a map of option name
     * to value. Valid options are lowercaseFieldNames (set to true to convert
     * field names to lowercase), dropStringConstraints (set to true to remove
     * length constraints on character fields).
     */
    static QgsVectorLayerExporter::ExportError createEmptyLayer(
      const QString &uri,
      const QgsFields &fields,
      QgsWkbTypes::Type wkbType,
      const QgsCoordinateReferenceSystem &srs,
      bool overwrite,
      QMap<int, int> *oldToNewAttrIdxMap,
      QString *errorMessage = nullptr,
      const QMap<QString, QVariant> *options = nullptr
    );

    /**
     * Constructor for the provider. The uri must be in the following format:
     * host=localhost dbname=test [user=gsherman [password=xxx] | authcfg=xxx] table=test.alaska (the_geom)
     * \param uri String containing the required parameters to connect to the database
     * and query the table.
     */
    explicit QgsPostgresProvider( QString const &uri = "" );


    virtual ~QgsPostgresProvider();

    virtual QgsAbstractFeatureSource *featureSource() const override;
    virtual QString storageType() const override;
    virtual QgsCoordinateReferenceSystem crs() const override;
    virtual QgsFeatureIterator getFeatures( const QgsFeatureRequest &request ) const override;
    QgsWkbTypes::Type wkbType() const override;

    /** Return the number of layers for the current data source
     * \note Should this be subLayerCount() instead?
     */
    size_t layerCount() const;

    long featureCount() const override;

    /**
     * Return a string representation of the endian-ness for the layer
     */
    static QString endianString();

    /**
     * Returns a list of unquoted column names from an uri key
     */
    static QStringList parseUriKey( const QString &key );

    /**
     * Changes the stored extent for this layer to the supplied extent.
     * For example, this is called when the extent worker thread has a result.
     */
    void setExtent( QgsRectangle &newExtent );

    virtual QgsRectangle extent() const override;
    virtual void updateExtents() override;

    /**
     * Determine the fields making up the primary key
     */
    bool determinePrimaryKey();

    /**
     * Determine the fields making up the primary key from the uri attribute keyColumn
     *
     * Fills mPrimaryKeyType and mPrimaryKeyAttrs
     * from mUri
     */
    void determinePrimaryKeyFromUriKeyColumn( bool checkPrimaryKeyUnicity = true );

    QgsFields fields() const override;
    QString dataComment() const override;
    QVariant minimumValue( int index ) const override;
    QVariant maximumValue( int index ) const override;
    virtual void uniqueValues( int index, QList<QVariant> &uniqueValues, int limit = -1 ) const override;
    virtual QStringList uniqueStringsMatching( int index, const QString &substring, int limit = -1,
        QgsFeedback *feedback = nullptr ) const override;
    virtual void enumValues( int index, QStringList &enumList ) const override;
    bool isValid() const override;
    virtual bool isSaveAndLoadStyleToDatabaseSupported() const override { return true; }
    virtual bool isDeleteStyleFromDatabaseSupported() const override { return true; }
    QgsAttributeList attributeIndexes() const override;
    QgsAttributeList pkAttributeIndexes() const override { return mPrimaryKeyAttrs; }
    QString defaultValueClause( int fieldId ) const override;
    QVariant defaultValue( int fieldId ) const override;
    bool skipConstraintCheck( int fieldIndex, QgsFieldConstraints::Constraint constraint, const QVariant &value = QVariant() ) const override;
    bool addFeatures( QgsFeatureList &flist ) override;
    bool deleteFeatures( const QgsFeatureIds &id ) override;
    bool truncate() override;
    bool addAttributes( const QList<QgsField> &attributes ) override;
    bool deleteAttributes( const QgsAttributeIds &name ) override;
    virtual bool renameAttributes( const QgsFieldNameMap &renamedAttributes ) override;
    bool changeAttributeValues( const QgsChangedAttributesMap &attr_map ) override;
    bool changeGeometryValues( const QgsGeometryMap &geometry_map ) override;
    bool changeFeatures( const QgsChangedAttributesMap &attr_map, const QgsGeometryMap &geometry_map ) override;

    //! Get the postgres connection
    PGconn *pgConnection();

    //! Get the table name associated with this provider instance
    QString getTableName();

    QString subsetString() const override;
    bool setSubsetString( const QString &theSQL, bool updateFeatureCount = true ) override;
    virtual bool supportsSubsetString() const override { return true; }
    QgsVectorDataProvider::Capabilities capabilities() const override;

    /** The Postgres provider does its own transforms so we return
     * true for the following three functions to indicate that transforms
     * should not be handled by the QgsCoordinateTransform object. See the
     * documentation on QgsVectorDataProvider for details on these functions.
     */
    // XXX For now we have disabled native transforms in the PG provider since
    //     it appears there are problems with some of the projection definitions
    bool supportsNativeTransform() {return false;}

    QString name() const override;
    QString description() const override;
    virtual QgsTransaction *transaction() const override;

    /**
     * Convert the postgres string representation into the given QVariant type.
     * \param type the wanted type
     * \param subType if type is a collection, the wanted element type
     * \param value the value to convert
     * \returns a QVariant of the given type or a null QVariant
     */
    static QVariant convertValue( QVariant::Type type, QVariant::Type subType, const QString &value );

    virtual QList<QgsRelation> discoverRelations( const QgsVectorLayer *self, const QList<QgsVectorLayer *> &layers ) const override;
    virtual QgsAttrPalIndexNameHash palAttributeIndexNames() const override;

  signals:

    /**
     *   This is emitted whenever the worker thread has fully calculated the
     *   PostGIS extents for this layer, and its event has been received by this
     *   provider.
     */
    void fullExtentCalculated();

    /**
     *   This is emitted when this provider is satisfied that all objects
     *   have had a chance to adjust themselves after they'd been notified that
     *   the full extent is available.
     *
     *   \note  It currently isn't being emitted because we don't have an easy way
     *          for the overview canvas to only be repainted.  In the meantime
     *          we are satisfied for the overview to reflect the new extent
     *          when the user adjusts the extent of the main map canvas.
     */
    void repaintRequested();

  private:

    bool declareCursor( const QString &cursorName,
                        const QgsAttributeList &fetchAttributes,
                        bool fetchGeometry,
                        QString whereClause );

    bool getFeature( QgsPostgresResult &queryResult,
                     int row,
                     bool fetchGeometry,
                     QgsFeature &feature,
                     const QgsAttributeList &fetchAttributes );

    QString geomParam( int offset ) const;

    /** Get parametrized primary key clause
     * \param offset specifies offset to use for the pk value parameter
     * \param alias specifies an optional alias given to the subject table
     */
    QString pkParamWhereClause( int offset, const char *alias = nullptr ) const;
    QString whereClause( QgsFeatureId featureId ) const;
    QString whereClause( QgsFeatureIds featureIds ) const;
    QString filterWhereClause() const;

    bool hasSufficientPermsAndCapabilities();

    QgsField field( int index ) const;

    /** Load the field list
     */
    bool loadFields();

    /** Set the default widget type for the fields
     */
    void setEditorWidgets();

    //! Convert a QgsField to work with PG
    static bool convertField( QgsField &field, const QMap<QString, QVariant> *options = nullptr );

    /** Parses the enum_range of an attribute and inserts the possible values into a stringlist
    \param enumValues the stringlist where the values are appended
    \param attributeName the name of the enum attribute
    \returns true in case of success and fals in case of error (e.g. if the type is not an enum type)*/
    bool parseEnumRange( QStringList &enumValues, const QString &attributeName ) const;

    /** Parses the possible enum values of a domain type (given in the check constraint of the domain type)
     * \param enumValues Reference to list that receives enum values
     * \param attributeName Name of the domain type attribute
     * \returns true in case of success and false in case of error (e.g. if the attribute is not a domain type or does not have a check constraint)
     */
    bool parseDomainCheckConstraint( QStringList &enumValues, const QString &attributeName ) const;

    /** Return the type of primary key for a PK field
     *
     * \param fld the field to determine PK type of
     * \returns the PrimaryKeyType
     *
     * \note that this only makes sense for single-field primary keys,
     *       whereas multi-field keys always need the PktFidMap
     *       primary key type.
     */
    QgsPostgresPrimaryKeyType pkType( const QgsField &fld ) const;

    /**
     * Search all the layers using the given table.
     */
    static QList<QgsVectorLayer *> searchLayers( const QList<QgsVectorLayer *> &layers, const QString &connectionInfo, const QString &schema, const QString &tableName );

    //! Old-style mapping of index to name for QgsPalLabeling fix
    QgsAttrPalIndexNameHash mAttrPalIndexName;

    QgsFields mAttributeFields;
    QString mDataComment;

    //! Data source URI struct for this layer
    QgsDataSourceUri mUri;

    /**
     * Flag indicating if the layer data source is a valid PostgreSQL layer
     */
    bool mValid;

    /**
     * provider references query (instead of a table)
     */
    bool mIsQuery;

    /**
     * Name of the table with no schema
     */
    QString mTableName;

    /**
     * Name of the table or subquery
     */
    QString mQuery;

    /**
     * Name of the schema
     */
    QString mSchemaName;

    /**
     * SQL statement used to limit the features retrieved
     */
    QString mSqlWhereClause;

    /**
     * Data type for the primary key
     */
    QgsPostgresPrimaryKeyType mPrimaryKeyType;

    /**
     * Data type for the spatial column
     */
    QgsPostgresGeometryColumnType mSpatialColType;

    /**
     * List of primary key attributes for fetching features.
     */
    QList<int> mPrimaryKeyAttrs;
    QString mPrimaryKeyDefault;

    QString mGeometryColumn;          //! name of the geometry column
    mutable QgsRectangle mLayerExtent;        //! Rectangle that contains the extent (bounding box) of the layer

    QgsWkbTypes::Type mDetectedGeomType;  //! geometry type detected in the database
    bool mForce2d;                    //! geometry type needs to be forced to 2d (e.g., ZM)
    QgsWkbTypes::Type mRequestedGeomType; //! geometry type requested in the uri
    QString mDetectedSrid;            //! Spatial reference detected in the database
    QString mRequestedSrid;           //! Spatial reference requested in the uri

    std::shared_ptr<QgsPostgresSharedData> mShared;  //!< Mutable data shared between provider and feature sources

    bool getGeometryDetails();

    //! @{ Only used with TopoGeometry layers

    struct TopoLayerInfo
    {
      QString topologyName;
      long    layerId;
    };

    TopoLayerInfo mTopoLayerInfo;

    bool getTopoLayerInfo();

    void dropOrphanedTopoGeoms();

    //! @}

    /* Use estimated metadata. Uses fast table counts, geometry type and extent determination */
    bool mUseEstimatedMetadata;

    bool mSelectAtIdDisabled; //! Disable support for SelectAtId

    struct PGFieldNotFound {}; //! Exception to throw

    struct PGException
    {
        explicit PGException( QgsPostgresResult &r )
          : mWhat( r.PQresultErrorMessage() )
        {}

        QString errorMessage() const
        {
          return mWhat;
        }

      private:
        QString mWhat;
    };

    // A function that determines if the given columns contain unique entries
    bool uniqueData( const QString &quotedColNames );

    QgsVectorDataProvider::Capabilities mEnabledCapabilities;

    void appendGeomParam( const QgsGeometry &geom, QStringList &param ) const;
    void appendPkParams( QgsFeatureId fid, QStringList &param ) const;

    QString paramValue( const QString &fieldvalue, const QString &defaultValue ) const;

    QgsPostgresConn *mConnectionRO; //! read-only database connection (initially)
    QgsPostgresConn *mConnectionRW; //! read-write database connection (on update)

    QgsPostgresConn *connectionRO() const;
    QgsPostgresConn *connectionRW();

    void disconnectDb();

    static QString quotedIdentifier( const QString &ident ) { return QgsPostgresConn::quotedIdentifier( ident ); }
    static QString quotedValue( const QVariant &value ) { return QgsPostgresConn::quotedValue( value ); }

    friend class QgsPostgresFeatureSource;

    QgsPostgresTransaction *mTransaction = nullptr;

    void setTransaction( QgsTransaction *transaction ) override;

    QHash<int, QString> mDefaultValues;
};


//! Assorted Postgres utility functions
class QgsPostgresUtils
{
  public:
    static QString whereClause( QgsFeatureId featureId,
                                const QgsFields &fields,
                                QgsPostgresConn *conn,
                                QgsPostgresPrimaryKeyType pkType,
                                const QList<int> &pkAttrs,
                                const std::shared_ptr<QgsPostgresSharedData> &sharedData );

    static QString whereClause( const QgsFeatureIds &featureIds,
                                const QgsFields &fields,
                                QgsPostgresConn *conn,
                                QgsPostgresPrimaryKeyType pkType,
                                const QList<int> &pkAttrs,
                                const std::shared_ptr<QgsPostgresSharedData> &sharedData );

    static QString andWhereClauses( const QString &c1, const QString &c2 );

    static const qint64 INT32PK_OFFSET = 4294967296;

    // We shift negative 32bit integers to above the max 32bit
    // positive integer to support the whole range of int32 values
    // See https://issues.qgis.org/issues/14262
    static qint64 int32pk_to_fid( qint32 x )
    {
      return x >= 0 ? x : x + INT32PK_OFFSET;
    }

    static qint32 fid_to_int32pk( qint64 x )
    {
      return x <= ( ( INT32PK_OFFSET ) / 2.0 ) ? x : -( INT32PK_OFFSET - x );
    }
};

/** Data shared between provider class and its feature sources. Ideally there should
 *  be as few members as possible because there could be simultaneous reads/writes
 *  from different threads and therefore locking has to be involved. */
class QgsPostgresSharedData
{
  public:
    QgsPostgresSharedData();

    long featuresCounted();
    void setFeaturesCounted( long count );
    void addFeaturesCounted( long diff );
    void ensureFeaturesCountedAtLeast( long fetched );

    // FID lookups
    QgsFeatureId lookupFid( const QVariantList &v ); // lookup existing mapping or add a new one
    QVariantList removeFid( QgsFeatureId fid );
    void insertFid( QgsFeatureId fid, const QVariantList &k );
    QVariantList lookupKey( QgsFeatureId featureId );
    void clear();

  protected:
    QMutex mMutex; //!< Access to all data members is guarded by the mutex

    long mFeaturesCounted;    //! Number of features in the layer

    QgsFeatureId mFidCounter;                    // next feature id if map is used
    QMap<QVariantList, QgsFeatureId> mKeyToFid;      // map key values to feature id
    QMap<QgsFeatureId, QVariantList> mFidToKey;      // map feature id back to key values
};

#endif
