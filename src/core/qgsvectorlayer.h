
/***************************************************************************
                          qgsvectorlayer.h  -  description
                             -------------------
    begin                : Oct 29, 2003
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

#ifndef QGSVECTORLAYER_H
#define QGSVECTORLAYER_H

#include "qgis_core.h"
#include <QMap>
#include <QSet>
#include <QList>
#include <QStringList>
#include <QFont>
#include <QMutex>

#include "qgis.h"
#include "qgsmaplayer.h"
#include "qgsfeature.h"
#include "qgsfeaturerequest.h"
#include "qgsfeaturesource.h"
#include "qgsfields.h"
#include "qgsvectordataprovider.h"
#include "qgsvectorsimplifymethod.h"
#include "qgseditformconfig.h"
#include "qgsattributetableconfig.h"
#include "qgsaggregatecalculator.h"
#include "qgsfeatureiterator.h"
#include "qgsexpressioncontextgenerator.h"

class QPainter;
class QImage;

class QgsAbstractGeometrySimplifier;
class QgsActionManager;
class QgsConditionalLayerStyles;
class QgsCoordinateTransform;
class QgsCurve;
class QgsDiagramLayerSettings;
class QgsDiagramRenderer;
class QgsEditorWidgetWrapper;
class QgsExpressionFieldBuffer;
class QgsFeatureRenderer;
class QgsGeometry;
class QgsGeometryVertexIndex;
class QgsMapToPixel;
class QgsRectangle;
class QgsRectangle;
class QgsRelation;
class QgsRelationManager;
class QgsSingleSymbolRenderer;
class QgsSymbol;
class QgsVectorLayerJoinInfo;
class QgsVectorLayerEditBuffer;
class QgsVectorLayerJoinBuffer;
class QgsVectorLayerFeatureCounter;
class QgsAbstractVectorLayerLabeling;
class QgsPoint;
class QgsFeedback;

typedef QList<int> QgsAttributeList;
typedef QSet<int> QgsAttributeIds;


/** \ingroup core
 * Represents a vector layer which manages a vector based data sets.
 *
 * The QgsVectorLayer is instantiated by specifying the name of a data provider,
 * such as postgres or wfs, and url defining the specific data set to connect to.
 * The vector layer constructor in turn instantiates a QgsVectorDataProvider subclass
 * corresponding to the provider type, and passes it the url. The data provider
 * connects to the data source.
 *
 * The QgsVectorLayer provides a common interface to the different data types. It also
 * manages editing transactions.
 *
 *  Sample usage of the QgsVectorLayer class:
 *
 * \code
 *     QString uri = "point?crs=epsg:4326&field=id:integer";
 *     QgsVectorLayer *scratchLayer = new QgsVectorLayer(uri, "Scratch point layer",  "memory");
 * \endcode
 *
 * The main data providers supported by QGIS are listed below.
 *
 * \section providers Vector data providers
 *
 * \subsection memory Memory data providerType (memory)
 *
 * The memory data provider is used to construct in memory data, for example scratch
 * data or data generated from spatial operations such as contouring. There is no
 * inherent persistent storage of the data. The data source uri is constructed. The
 * url specifies the geometry type ("point", "linestring", "polygon",
 * "multipoint","multilinestring","multipolygon"), optionally followed by url parameters
 * as follows:
 *
 * - crs=definition
 *   Defines the coordinate reference system to use for the layer.
 *   definition is any string accepted by QgsCoordinateReferenceSystem::createFromString()
 *
 * - index=yes
 *   Specifies that the layer will be constructed with a spatial index
 *
 * - field=name:type(length,precision)
 *   Defines an attribute of the layer. Multiple field parameters can be added
 *   to the data provider definition. type is one of "integer", "double", "string".
 *
 * An example url is "Point?crs=epsg:4326&field=id:integer&field=name:string(20)&index=yes"
 *
 * \subsection ogr OGR data provider (ogr)
 *
 * Accesses data using the OGR drivers (http://www.gdal.org/ogr/ogr_formats.html). The url
 * is the OGR connection string. A wide variety of data formats can be accessed using this
 * driver, including file based formats used by many GIS systems, database formats, and
 * web services. Some of these formats are also supported by custom data providers listed
 * below.
 *
 * \subsection spatialite SpatiaLite data provider (spatialite)
 *
 * Access data in a SpatiaLite database. The url defines the connection parameters, table,
 * geometry column, and other attributes. The url can be constructed using the
 * QgsDataSourceUri class.
 *
 * \subsection postgres PostgreSQL data provider (postgres)
 *
 * Connects to a PostgreSQL database. The url defines the connection parameters, table,
 * geometry column, and other attributes. The url can be constructed using the
 * QgsDataSourceUri class.
 *
 * \subsection mssql Microsoft SQL server data provider (mssql)
 *
 * Connects to a Microsoft SQL server database. The url defines the connection parameters, table,
 * geometry column, and other attributes. The url can be constructed using the
 * QgsDataSourceUri class.
 *
 * \subsection wfs WFS (web feature service) data provider (wfs)
 *
 * Used to access data provided by a web feature service.
 *
 * The url can be a HTTP url to a WFS server (legacy, e.g. http://foobar/wfs?TYPENAME=xxx&SRSNAME=yyy[&FILTER=zzz]), or,
 * starting with QGIS 2.16, a URI constructed using the QgsDataSourceUri class with the following parameters :
 * - url=string (mandatory): HTTP url to a WFS server endpoint. e.g http://foobar/wfs
 * - typename=string (mandatory): WFS typename
 * - srsname=string (recommended): SRS like 'EPSG:XXXX'
 * - username=string
 * - password=string
 * - authcfg=string
 * - version=auto/1.0.0/1.1.0/2.0.0
 *  -sql=string: full SELECT SQL statement with optional WHERE, ORDER BY and possibly with JOIN if supported on server
 * - filter=string: QGIS expression or OGC/FES filter
 * - restrictToRequestBBOX=1: to download only features in the view extent (or more generally
 *   in the bounding box of the feature iterator)
 * - maxNumFeatures=number
 * - IgnoreAxisOrientation=1: to ignore EPSG axis order for WFS 1.1 or 2.0
 * - InvertAxisOrientation=1: to invert axis order
 * - hideDownloadProgressDialog=1: to hide the download progress dialog
 *
 * The ‘FILTER’ query string parameter can be used to filter
 * the WFS feature type. The ‘FILTER’ key value can either be a QGIS expression
 * or an OGC XML filter. If the value is set to a QGIS expression the driver will
 * turn it into OGC XML filter before passing it to the WFS server. Beware the
 * QGIS expression filter only supports” =, !=, <, >, <=, >=, AND, OR, NOT, LIKE, IS NULL”
 * attribute operators, “BBOX, Disjoint, Intersects, Touches, Crosses, Contains, Overlaps, Within”
 * spatial binary operators and the QGIS local “geomFromWKT, geomFromGML”
 * geometry constructor functions.
 *
 * Also note:
 *
 * - You can use various functions available in the QGIS Expression list,
 *   however the function must exist server side and have the same name and arguments to work.
 *
 * - Use the special $geometry parameter to provide the layer geometry column as input
 *   into the spatial binary operators e.g intersects($geometry, geomFromWKT('POINT (5 6)'))
 *
 * \subsection delimitedtext Delimited text file data provider (delimitedtext)
 *
 * Accesses data in a delimited text file, for example CSV files generated by
 * spreadsheets. The contents of the file are split into columns based on specified
 * delimiter characters.  Each record may be represented spatially either by an
 * X and Y coordinate column, or by a WKT (well known text) formatted columns.
 *
 * The url defines the filename, the formatting options (how the
 * text in the file is divided into data fields, and which fields contain the
 * X,Y coordinates or WKT text definition.  The options are specified as url query
 * items.
 *
 * At its simplest the url can just be the filename, in which case it will be loaded
 * as a CSV formatted file.
 *
 * The url may include the following items:
 *
 * - encoding=UTF-8
 *
 *   Defines the character encoding in the file.  The default is UTF-8.  To use
 *   the default encoding for the operating system use "System".
 *
 * - type=(csv|regexp|whitespace|plain)
 *
 *   Defines the algorithm used to split records into columns. Records are
 *   defined by new lines, except for csv format files for which quoted fields
 *   may span multiple records.  The default type is csv.
 *
 *   -  "csv" splits the file based on three sets of characters:
 *      delimiter characters, quote characters,
 *      and escape characters.  Delimiter characters mark the end
 *      of a field. Quote characters enclose a field which can contain
 *      delimiter characters, and newlines.  Escape characters cause the
 *      following character to be treated literally (including delimiter,
 *      quote, and newline characters).  Escape and quote characters must
 *      be different from delimiter characters. Escape characters that are
 *      also quote characters are treated specially - they can only
 *      escape themselves within quotes.  Elsewhere they are treated as
 *      quote characters.  The defaults for delimiter, quote, and escape
 *      are ',', '"', '"'.
 *   -  "regexp" splits each record using a regular expression (see QRegExp
 *      documentation for details).
 *   -  "whitespace" splits each record based on whitespace (on or more whitespace
 *      characters.  Leading whitespace in the record is ignored.
 *   -  "plain" is provided for backwards compatibility.  It is equivalent to
 *      CSV except that the default quote characters are single and double quotes,
 *      and there is no escape characters.
 *
 * - delimiter=characters
 *
 *   Defines the delimiter characters used for csv and plain type files, or the
 *   regular expression for regexp type files.  It is a literal string of characters
 *   except that "\t" may be used to represent a tab character.
 *
 * - quote=characters
 *
 *   Defines the characters that are used as quote characters for csv and plain type
 *   files.
 *
 * - escape=characters
 *
 *   Defines the characters used to escape delimiter, quote, and newline characters.
 *
 * - skipLines=n
 *
 *   Defines the number of lines to ignore at the beginning of the file (default 0)
 *
 * - useHeader=(yes|no)
 *
 *   Defines whether the first record in the file (after skipped lines) contains
 *   column names (default yes)
 *
 * - trimFields=(yes|no)
 *
 *   If yes then leading and trailing whitespace will be removed from fields
 *
 * - skipEmptyFields=(yes|no)
 *
 *   If yes then empty fields will be discarded (equivalent to concatenating consecutive
 *   delimiters)
 *
 * - maxFields=#
 *
 *   Specifies the maximum number of fields to load for each record.  Additional
 *   fields will be discarded.  Default is 0 - load all fields.
 *
 * - decimalPoint=c
 *
 *   Defines a character that is used as a decimal point in the numeric columns
 *   The default is '.'.
 *
 * - xField=column yField=column
 *
 *   Defines the name of the columns holding the x and y coordinates for XY point geometries.
 *   If the useHeader is no (ie there are no column names), then this is the column
 *   number (with the first column as 1).
 *
 * - xyDms=(yes|no)
 *
 *   If yes then the X and Y coordinates are interpreted as
 *   degrees/minutes/seconds format (fairly permissively),
 *   or degree/minutes format.
 *
 * - wktField=column
 *
 *   Defines the name of the columns holding the WKT geometry definition for WKT geometries.
 *   If the useHeader is no (ie there are no column names), then this is the column
 *   number (with the first column as 1).
 *
 * - geomType=(point|line|polygon|none)
 *
 *   Defines the geometry type for WKT type geometries.  QGIS will only display one
 *   type of geometry for the layer - any others will be ignored when the file is
 *   loaded.  By default the provider uses the type of the first geometry in the file.
 *   Use geomType to override this type.
 *
 *   geomType can also be set to none, in which case the layer is loaded without
 *   geometries.
 *
 * - subset=expression
 *
 *   Defines an expression that will identify a subset of records to display
 *
 * - crs=crsstring
 *
 *   Defines the coordinate reference system used for the layer.  This can be
 *   any string accepted by QgsCoordinateReferenceSystem::createFromString()
 *
 * -subsetIndex=(yes|no)
 *
 *   Determines whether the provider generates an index to improve the efficiency
 *   of subsets.  The default is yes
 *
 * -spatialIndex=(yes|no)
 *
 *   Determines whether the provider generates a spatial index.  The default is no.
 *
 * -watchFile=(yes|no)
 *
 *   Defines whether the file will be monitored for changes. The default is
 *   to monitor for changes.
 *
 * - quiet
 *
 *   Errors encountered loading the file will not be reported in a user dialog if
 *   quiet is included (They will still be shown in the output log).
 *
 * \subsection gpx GPX data provider (gpx)
 *
 * Provider reads tracks, routes, and waypoints from a GPX file.  The url
 * defines the name of the file, and the type of data to retrieve from it
 * ("track", "route", or "waypoint").
 *
 * An example url is "/home/user/data/holiday.gpx?type=route"
 *
 * \subsection grass Grass data provider (grass)
 *
 * Provider to display vector data in a GRASS GIS layer.
 *
 * TODO QGIS3: Remove virtual from non-inherited methods (like isModified)
 * \see QgsVectorLayerUtils()
 */
class CORE_EXPORT QgsVectorLayer : public QgsMapLayer, public QgsExpressionContextGenerator, public QgsFeatureSink, public QgsFeatureSource
{
    Q_OBJECT

    Q_PROPERTY( QString displayExpression READ displayExpression WRITE setDisplayExpression NOTIFY displayExpressionChanged )
    Q_PROPERTY( QString mapTipTemplate READ mapTipTemplate WRITE setMapTipTemplate NOTIFY mapTipTemplateChanged )
    Q_PROPERTY( QgsEditFormConfig editFormConfig READ editFormConfig WRITE setEditFormConfig NOTIFY editFormConfigChanged )
    Q_PROPERTY( bool readOnly READ isReadOnly WRITE setReadOnly NOTIFY readOnlyChanged )
    Q_PROPERTY( double opacity READ opacity WRITE setOpacity NOTIFY opacityChanged )

  public:

    //! Result of an edit operation
    enum EditResult
    {
      Success = 0, //!< Edit operation was successful
      EmptyGeometry = 1, //!< Edit operation resulted in an empty geometry
      EditFailed = 2, //!< Edit operation failed
      FetchFeatureFailed = 3, //!< Unable to fetch requested feature
      InvalidLayer = 4, //!< Edit failed due to invalid layer
    };

    //! Selection behavior
    enum SelectBehavior
    {
      SetSelection, //!< Set selection, removing any existing selection
      AddToSelection, //!< Add selection to current selection
      IntersectSelection, //!< Modify current selection to include only select features which match
      RemoveFromSelection, //!< Remove from current selection
    };

    /** Constructor - creates a vector layer
     *
     * The QgsVectorLayer is constructed by instantiating a data provider.  The provider
     * interprets the supplied path (url) of the data source to connect to and access the
     * data.
     *
     * \param  path  The path or url of the parameter.  Typically this encodes
     *               parameters used by the data provider as url query items.
     * \param  baseName The name used to represent the layer in the legend
     * \param  providerLib  The name of the data provider, e.g., "memory", "postgres"
     * \param  loadDefaultStyleFlag whether to load the default style
     *
     */
    QgsVectorLayer( const QString &path = QString(), const QString &baseName = QString(),
                    const QString &providerLib = "ogr", bool loadDefaultStyleFlag = true );


    virtual ~QgsVectorLayer();

    //! QgsVectorLayer cannot be copied.
    QgsVectorLayer( const QgsVectorLayer &rhs ) = delete;
    //! QgsVectorLayer cannot be copied.
    QgsVectorLayer &operator=( QgsVectorLayer const &rhs ) = delete;

    /** Returns a new instance equivalent to this one. A new provider is
     *  created for the same data source and renderers for features and diagrams
     *  are cloned too. Moreover, each attributes (transparency, extent, selected
     *  features and so on) are identicals.
     * \returns a new layer instance
     * \since QGIS 3.0
     */
    virtual QgsVectorLayer *clone() const override SIP_FACTORY;

    //! Returns the permanent storage type for this layer as a friendly name.
    QString storageType() const;

    //! Capabilities for this layer in a friendly format.
    QString capabilitiesString() const;

    //! Returns a comment for the data in the layer
    QString dataComment() const;

    /**
     * This is a shorthand for accessing the displayExpression if it is a simple field.
     * If the displayExpression is more complex than a simple field, a null string will
     * be returned.
     *
     * \see displayExpression
     */
    QString displayField() const;

    /** Set the preview expression, used to create a human readable preview string.
     *  Used e.g. in the attribute table feature list. Uses QgsExpression.
     *
     *  \param displayExpression The expression which will be used to preview features
     *                           for this layer
     */
    void setDisplayExpression( const QString &displayExpression );

    /**
     *  Get the preview expression, used to create a human readable preview string.
     *  Uses QgsExpression
     *
     *  \returns The expression which will be used to preview features for this layer
     */
    QString displayExpression() const;

    QgsVectorDataProvider *dataProvider() override;
    const QgsVectorDataProvider *dataProvider() const override SIP_SKIP;

    //! Sets the textencoding of the data provider
    void setProviderEncoding( const QString &encoding );

    //! Setup the coordinate system transformation for the layer
    void setCoordinateSystem();

    /** Joins another vector layer to this layer
      \param joinInfo join object containing join layer id, target and source field
      \note since 2.6 returns bool indicating whether the join can be added */
    bool addJoin( const QgsVectorLayerJoinInfo &joinInfo );

    /** Removes a vector layer join
      \returns true if join was found and successfully removed */
    bool removeJoin( const QString &joinLayerId );

    /**
     * Accessor to the join buffer object
     * \since QGIS 2.14.7
     */
    QgsVectorLayerJoinBuffer *joinBuffer() { return mJoinBuffer; }
    const QList<QgsVectorLayerJoinInfo> vectorJoins() const;

    /**
     * Sets the list of dependencies.
     * \see dependencies()
     *
     * \param layers set of QgsMapLayerDependency. Only user-defined dependencies will be added
     * \returns false if a dependency cycle has been detected
     * \since QGIS 3.0
     */
    virtual bool setDependencies( const QSet<QgsMapLayerDependency> &layers ) override;

    /**
     * Gets the list of dependencies. This includes data dependencies set by the user (\see setDataDependencies)
     * as well as dependencies given by the provider
     *
     * \returns a set of QgsMapLayerDependency
     * \since QGIS 3.0
     */
    virtual QSet<QgsMapLayerDependency> dependencies() const override;

    /**
     * Add a new field which is calculated by the expression specified
     *
     * \param exp The expression which calculates the field
     * \param fld The field to calculate
     *
     * \returns The index of the new field
     *
     * \since QGIS 2.9
     */
    int addExpressionField( const QString &exp, const QgsField &fld );

    /**
     * Remove an expression field
     *
     * \param index The index of the field
     *
     * \since QGIS 2.6
     */
    void removeExpressionField( int index );

    /**
     * Returns the expression used for a given expression field
     *
     * \param index An index of an epxression based (virtual) field
     *
     * \returns The expression for the field at index
     *
     * \since QGIS 2.9
     */
    QString expressionField( int index ) const;

    /**
     * Changes the expression used to define an expression based (virtual) field
     *
     * \param index The index of the expression to change
     *
     * \param exp The new expression to set
     *
     * \since QGIS 2.9
     */
    void updateExpressionField( int index, const QString &exp );

    /**
     * Get all layer actions defined on this layer.
     *
     * The pointer which is returned directly points to the actions object
     * which is used by the layer, so any changes are immediately applied.
     */
    QgsActionManager *actions() { return mActions; }

    /**
     * Get all layer actions defined on this layer.
     *
     * The pointer which is returned is const.
     */
    const QgsActionManager *actions() const SIP_SKIP { return mActions; }

    /**
     * The number of features that are selected in this layer
     *
     * \returns See description
     */
    int selectedFeatureCount() const;

    /**
     * Select features found within the search rectangle (in layer's coordinates)
     * \param rect search rectangle
     * \param behavior selection type, allows adding to current selection, removing
     * from selection, etc.
     * \see invertSelectionInRectangle(QgsRectangle & rect)
     * \see selectByExpression()
     * \see selectByIds()
     */
    void selectByRect( QgsRectangle &rect, SelectBehavior behavior = SetSelection );

    /** Select matching features using an expression.
     * \param expression expression to evaluate to select features
     * \param behavior selection type, allows adding to current selection, removing
     * from selection, etc.
     * \since QGIS 2.16
     * \see selectByRect()
     * \see selectByIds()
     */
    void selectByExpression( const QString &expression, SelectBehavior behavior = SetSelection );

    /** Select matching features using a list of feature IDs. Will emit the
     * selectionChanged() signal with the clearAndSelect flag set.
     * \param ids feature IDs to select
     * \param behavior selection type, allows adding to current selection, removing
     * from selection, etc.
     * \since QGIS 2.16
     * \see selectByRect()
     * \see selectByExpression()
     */
    void selectByIds( const QgsFeatureIds &ids, SelectBehavior behavior = SetSelection );

    /**
     * Modifies the current selection on this layer
     *
     * \param selectIds    Select these ids
     * \param deselectIds  Deselect these ids
     *
     * \see   select(QgsFeatureIds)
     * \see   select(QgsFeatureId)
     * \see   deselect(QgsFeatureIds)
     * \see   deselect(QgsFeatureId)
     * \see selectByExpression()
     */
    void modifySelection( const QgsFeatureIds &selectIds, const QgsFeatureIds &deselectIds );

    //! Select not selected features and deselect selected ones
    void invertSelection();

    //! Select all the features
    void selectAll();

    //! Get all feature Ids
    QgsFeatureIds allFeatureIds() const;

    /**
     * Invert selection of features found within the search rectangle (in layer's coordinates)
     *
     * \param rect  The rectangle in which the selection of features will be inverted
     *
     * \see   invertSelection()
     */
    void invertSelectionInRectangle( QgsRectangle &rect );

    /**
     * Get a copy of the user-selected features
     *
     * \returns A list of QgsFeature
     *
     * \see    selectedFeatureIds()
     * \see    getSelectedFeatures() which is more memory friendly when handling large selections
     */
    QgsFeatureList selectedFeatures() const;

    /**
     * Get an iterator of the selected features
     *
     * \param request You may specify a request, e.g. to limit the set of requested attributes.
     *                Any filter on the request will be discarded.
     *
     * \returns Iterator over the selected features
     *
     * \see    selectedFeatureIds()
     * \see    selectedFeatures()
     */
    QgsFeatureIterator getSelectedFeatures( QgsFeatureRequest request = QgsFeatureRequest() ) const;

    /**
     * Return reference to identifiers of selected features
     *
     * \returns A list of QgsFeatureId
     * \see selectedFeatures()
     */
    const QgsFeatureIds &selectedFeatureIds() const;

    //! Returns the bounding box of the selected features. If there is no selection, QgsRectangle(0,0,0,0) is returned
    QgsRectangle boundingBoxOfSelected() const;

    /** Returns whether the layer contains labels which are enabled and should be drawn.
     * \returns true if layer contains enabled labels
     * \since QGIS 2.9
     */
    bool labelsEnabled() const;

    /** Returns whether the layer contains diagrams which are enabled and should be drawn.
     * \returns true if layer contains enabled diagrams
     * \since QGIS 2.9
     */
    bool diagramsEnabled() const;

    //! Sets diagram rendering object (takes ownership)
    void setDiagramRenderer( QgsDiagramRenderer *r SIP_TRANSFER );
    const QgsDiagramRenderer *diagramRenderer() const { return mDiagramRenderer; }

    void setDiagramLayerSettings( const QgsDiagramLayerSettings &s );
    const QgsDiagramLayerSettings *diagramLayerSettings() const { return mDiagramLayerSettings; }

    //! Return renderer.
    QgsFeatureRenderer *renderer() { return mRenderer; }

    /** Return const renderer.
     * \note not available in Python bindings
     */
    const QgsFeatureRenderer *renderer() const SIP_SKIP { return mRenderer; }

    /**
     * Set renderer which will be invoked to represent this layer.
     * Ownership is transferred.
     */
    void setRenderer( QgsFeatureRenderer *r SIP_TRANSFER );

    //! Returns point, line or polygon
    QgsWkbTypes::GeometryType geometryType() const;

    //! Returns the WKBType or WKBUnknown in case of error
    QgsWkbTypes::Type wkbType() const override;

    //! Return the provider type for this layer
    QString providerType() const;

    QgsCoordinateReferenceSystem sourceCrs() const override;
    QString sourceName() const override;

    /** Reads vector layer specific state from project file Dom node.
     * \note Called by QgsMapLayer::readXml().
     */
    virtual bool readXml( const QDomNode &layer_node, const QgsReadWriteContext &context ) override;

    /** Write vector layer specific state to project file Dom node.
     * \note Called by QgsMapLayer::writeXml().
     */
    virtual bool writeXml( QDomNode &layer_node, QDomDocument &doc, const QgsReadWriteContext &context ) const override;

    /** Resolve references to other layers (kept as layer IDs after reading XML) into layer objects.
     * \since QGIS 3.0
     */
    void resolveReferences( QgsProject *project );

    /**
     * Save named and sld style of the layer to the style table in the db.
     * \param name
     * \param description
     * \param useAsDefault
     * \param uiFileContent
     * \param msgError
     */
    virtual void saveStyleToDatabase( const QString &name, const QString &description,
                                      bool useAsDefault, const QString &uiFileContent,
                                      QString &msgError SIP_OUT );

    /**
     * Lists all the style in db split into related to the layer and not related to
     * \param ids the list in which will be stored the style db ids
     * \param names the list in which will be stored the style names
     * \param descriptions the list in which will be stored the style descriptions
     * \param msgError
     * \returns the number of styles related to current layer
     */
    virtual int listStylesInDatabase( QStringList &ids SIP_OUT, QStringList &names SIP_OUT,
                                      QStringList &descriptions SIP_OUT, QString &msgError SIP_OUT );

    /**
     * Will return the named style corresponding to style id provided
     */
    virtual QString getStyleFromDatabase( const QString &styleId, QString &msgError SIP_OUT );

    /**
     * Delete a style from the database
     * \since QGIS 3.0
     * \param styleId the provider's layer_styles table id of the style to delete
     * \param msgError reference to string that will be updated with any error messages
     * \returns true in case of success
     */
    virtual bool deleteStyleFromDatabase( const QString &styleId, QString &msgError SIP_OUT );

    /**
     * Load a named style from file/local db/datasource db
     * \param theURI the URI of the style or the URI of the layer
     * \param resultFlag will be set to true if a named style is correctly loaded
     * \param loadFromLocalDb if true forces to load from local db instead of datasource one
     */
    virtual QString loadNamedStyle( const QString &theURI, bool &resultFlag SIP_OUT, bool loadFromLocalDb );

    /**
     * Calls loadNamedStyle( theURI, resultFlag, false );
     * Retained for backward compatibility
     */
    virtual QString loadNamedStyle( const QString &theURI, bool &resultFlag SIP_OUT ) override;

    /** Read the symbology for the current layer from the Dom node supplied.
     * \param layerNode node that will contain the symbology definition for this layer.
     * \param errorMessage reference to string that will be updated with any error messages
     * \param context reading context (used for transform from relative to absolute paths)
     * \returns true in case of success.
     */
    bool readSymbology( const QDomNode &layerNode, QString &errorMessage, const QgsReadWriteContext &context ) override;

    /** Read the style for the current layer from the Dom node supplied.
     * \param node node that will contain the style definition for this layer.
     * \param errorMessage reference to string that will be updated with any error messages
     * \param context reading context (used for transform from relative to absolute paths)
     * \returns true in case of success.
     */
    bool readStyle( const QDomNode &node, QString &errorMessage, const QgsReadWriteContext &context ) override;

    /** Write the symbology for the layer into the docment provided.
     *  \param node the node that will have the style element added to it.
     *  \param doc the document that will have the QDomNode added.
     *  \param errorMessage reference to string that will be updated with any error messages
     *  \param context writing context (used for transform from absolute to relative paths)
     *  \returns true in case of success.
     */
    bool writeSymbology( QDomNode &node, QDomDocument &doc, QString &errorMessage, const QgsReadWriteContext &context ) const override;

    /** Write just the style information for the layer into the document
     *  \param node the node that will have the style element added to it.
     *  \param doc the document that will have the QDomNode added.
     *  \param errorMessage reference to string that will be updated with any error messages
     *  \param context writing context (used for transform from absolute to relative paths)
     *  \returns true in case of success.
     */
    bool writeStyle( QDomNode &node, QDomDocument &doc, QString &errorMessage, const QgsReadWriteContext &context ) const override;

    /**
     * Writes the symbology of the layer into the document provided in SLD 1.1 format
     * \param node the node that will have the style element added to it.
     * \param doc the document that will have the QDomNode added.
     * \param errorMessage reference to string that will be updated with any error messages
     * \param props a open ended set of properties that can drive/inform the SLD encoding
     * \returns true in case of success
     */
    bool writeSld( QDomNode &node, QDomDocument &doc, QString &errorMessage, const QgsStringMap &props = QgsStringMap() ) const;

    bool readSld( const QDomNode &node, QString &errorMessage ) override;

    /**
     * Number of features rendered with specified legend key. Features must be first
     * calculated by countSymbolFeatures()
     * \returns number of features rendered by symbol or -1 if failed or counts are not available
     */
    long featureCount( const QString &legendKey ) const;

    /**
     * Update the data source of the layer. The layer's renderer and legend will be preserved only
     * if the geometry type of the new data source matches the current geometry type of the layer.
     * \param dataSource new layer data source
     * \param baseName base name of the layer
     * \param provider provider string
     * \param loadDefaultStyleFlag set to true to reset the layer's style to the default for the
     * data source
     * \since QGIS 2.10
     */
    void setDataSource( const QString &dataSource, const QString &baseName, const QString &provider, bool loadDefaultStyleFlag = false );

    /**
     * Count features for symbols.
     * The method will return immediately. You will need to connect to the
     * symbolFeatureCountMapChanged() signal to be notified when the freshly updated
     * feature counts are ready.
     *
     * \note If you need to wait for the results, create and start your own QgsVectorLayerFeatureCounter
     *       task and call waitForFinished().
     *
     * \since This is asynchronous since QGIS 3.0
     */
    bool countSymbolFeatures();

    /**
     * Set the string (typically sql) used to define a subset of the layer
     * \param subset The subset string. This may be the where clause of a sql statement
     *               or other definition string specific to the underlying dataprovider
     *               and data store.
     * \returns true, when setting the subset string was successful, false otherwise
     */
    virtual bool setSubsetString( const QString &subset );

    /**
     * Get the string (typically sql) used to define a subset of the layer
     * \returns The subset string or null QString if not implemented by the provider
     */
    virtual QString subsetString() const;

    /**
     * Query the layer for features specified in request.
     * \param request feature request describing parameters of features to return
     * \returns iterator for matching features from provider
     */
    QgsFeatureIterator getFeatures( const QgsFeatureRequest &request = QgsFeatureRequest() ) const override;

    /**
     * Query the layer for features matching a given expression.
     */
    inline QgsFeatureIterator getFeatures( const QString &expression )
    {
      return getFeatures( QgsFeatureRequest( expression ) );
    }

    /**
     * Query the layer for the feature with the given id.
     * If there is no such feature, the returned feature will be invalid.
     */
    inline QgsFeature getFeature( QgsFeatureId fid ) const
    {
      QgsFeature feature;
      getFeatures( QgsFeatureRequest( fid ) ).nextFeature( feature );
      return feature;
    }

    /**
     * Query the layer for the features with the given ids.
     */
    inline QgsFeatureIterator getFeatures( const QgsFeatureIds &fids )
    {
      return getFeatures( QgsFeatureRequest( fids ) );
    }

    /**
     * Query the layer for the features which intersect the specified rectangle.
     */
    inline QgsFeatureIterator getFeatures( const QgsRectangle &rectangle )
    {
      return getFeatures( QgsFeatureRequest( rectangle ) );
    }

    bool addFeature( QgsFeature &feature, QgsFeatureSink::Flags flags = 0 ) override;

    /** Updates an existing feature. This method needs to query the datasource
        on every call. Consider using changeAttributeValue() or
        changeGeometry() instead.
        \param f  Feature to update
        \returns   True in case of success and False in case of error
     */
    bool updateFeature( QgsFeature &f );

    /** Insert a new vertex before the given vertex number,
     *  in the given ring, item (first number is index 0), and feature
     *  Not meaningful for Point geometries
     */
    bool insertVertex( double x, double y, QgsFeatureId atFeatureId, int beforeVertex );

    /** Insert a new vertex before the given vertex number,
     *  in the given ring, item (first number is index 0), and feature
     *  Not meaningful for Point geometries
     */
    bool insertVertex( const QgsPoint &point, QgsFeatureId atFeatureId, int beforeVertex );

    /** Moves the vertex at the given position number,
     *  ring and item (first number is index 0), and feature
     *  to the given coordinates
     */
    bool moveVertex( double x, double y, QgsFeatureId atFeatureId, int atVertex );

    /** Moves the vertex at the given position number,
     * ring and item (first number is index 0), and feature
     * to the given coordinates
     * \note available in Python as moveVertexV2
     */
    bool moveVertex( const QgsPoint &p, QgsFeatureId atFeatureId, int atVertex ) SIP_PYNAME( moveVertexV2 );

    /** Deletes a vertex from a feature.
     * \param featureId ID of feature to remove vertex from
     * \param vertex index of vertex to delete
     * \since QGIS 2.14
     */
    EditResult deleteVertex( QgsFeatureId featureId, int vertex );

    /** Deletes the selected features
     *  \returns true in case of success and false otherwise
     */
    bool deleteSelectedFeatures( int *deletedCount = nullptr );

    /** Adds a ring to polygon/multipolygon features
     * \param ring ring to add
     * \param featureId if specified, feature ID for feature ring was added to will be stored in this parameter
     * \returns
     *  0 in case of success,
     *  1 problem with feature type,
     *  2 ring not closed,
     *  3 ring not valid,
     *  4 ring crosses existing rings,
     *  5 no feature found where ring can be inserted
     *  6 layer not editable
     */
    // TODO QGIS 3.0 returns an enum instead of a magic constant
    int addRing( const QList<QgsPointXY> &ring, QgsFeatureId *featureId = nullptr );

    /** Adds a ring to polygon/multipolygon features (takes ownership)
     * \param ring ring to add
     * \param featureId if specified, feature ID for feature ring was added to will be stored in this parameter
     * \returns
     *  0 in case of success
     *  1 problem with feature type
     *  2 ring not closed
     *  6 layer not editable
     * \note available in Python as addCurvedRing
     */
    // TODO QGIS 3.0 returns an enum instead of a magic constant
    int addRing( QgsCurve *ring SIP_TRANSFER, QgsFeatureId *featureId = nullptr ) SIP_PYNAME( addCurvedRing );

    /** Adds a new part polygon to a multipart feature
     * \returns
     *   0 in case of success,
     *   1 if selected feature is not multipart,
     *   2 if ring is not a valid geometry,
     *   3 if new polygon ring not disjoint with existing rings,
     *   4 if no feature was selected,
     *   5 if several features are selected,
     *   6 if selected geometry not found
     *   7 layer not editable
     */
    // TODO QGIS 3.0 returns an enum instead of a magic constant
    int addPart( const QList<QgsPointXY> &ring );

    /** Adds a new part polygon to a multipart feature
     * \returns
     *   0 in case of success,
     *   1 if selected feature is not multipart,
     *   2 if ring is not a valid geometry,
     *   3 if new polygon ring not disjoint with existing rings,
     *   4 if no feature was selected,
     *   5 if several features are selected,
     *   6 if selected geometry not found
     *   7 layer not editable
     * \note available in Python bindings as addPartV2
     */
    // TODO QGIS 3.0 returns an enum instead of a magic constant
    int addPart( const QgsPointSequence &ring ) SIP_PYNAME( addPartV2 );

    //! \note available in Python as addCurvedPart
    int addPart( QgsCurve *ring SIP_TRANSFER ) SIP_PYNAME( addCurvedPart );

    /** Translates feature by dx, dy
     *  \param featureId id of the feature to translate
     *  \param dx translation of x-coordinate
     *  \param dy translation of y-coordinate
     *  \returns 0 in case of success
     */
    int translateFeature( QgsFeatureId featureId, double dx, double dy );

    /** Splits parts cut by the given line
     *  \param splitLine line that splits the layer features
     *  \param topologicalEditing true if topological editing is enabled
     *  \returns
     *   0 in case of success,
     *   4 if there is a selection but no feature split
     */
    // TODO QGIS 3.0 returns an enum instead of a magic constant
    int splitParts( const QList<QgsPointXY> &splitLine, bool topologicalEditing = false );

    /** Splits features cut by the given line
     *  \param splitLine line that splits the layer features
     *  \param topologicalEditing true if topological editing is enabled
     *  \returns
     *   0 in case of success,
     *   4 if there is a selection but no feature split
     */
    // TODO QGIS 3.0 returns an enum instead of a magic constant
    int splitFeatures( const QList<QgsPointXY> &splitLine, bool topologicalEditing = false );

    /** Adds topological points for every vertex of the geometry.
     * \param geom the geometry where each vertex is added to segments of other features
     * \note geom is not going to be modified by the function
     * \returns 0 in case of success
     */
    int addTopologicalPoints( const QgsGeometry &geom );

    /** Adds a vertex to segments which intersect point p but don't
     * already have a vertex there. If a feature already has a vertex at position p,
     * no additional vertex is inserted. This method is useful for topological
     * editing.
     * \param p position of the vertex
     * \returns 0 in case of success
     */
    int addTopologicalPoints( const QgsPointXY &p );

    /** Access to labeling configuration. May be null if labeling is not used.
     * \since QGIS 3.0
     */
    const QgsAbstractVectorLayerLabeling *labeling() const { return mLabeling; }

    /** Set labeling configuration. Takes ownership of the object.
     * \since QGIS 3.0
     */
    void setLabeling( QgsAbstractVectorLayerLabeling *labeling SIP_TRANSFER );

    //! Returns true if the provider is in editing mode
    virtual bool isEditable() const override;

    //! Returns true if this is a geometry layer and false in case of NoGeometry (table only) or UnknownGeometry
    virtual bool isSpatial() const override;

    //! Returns true if the provider has been modified since the last commit
    virtual bool isModified() const;

    //! Synchronises with changes in the datasource
    virtual void reload() override;

    /** Return new instance of QgsMapLayerRenderer that will be used for rendering of given context
     * \since QGIS 2.4
     */
    virtual QgsMapLayerRenderer *createMapRenderer( QgsRenderContext &rendererContext ) override SIP_FACTORY;

    QgsRectangle extent() const override;
    QgsRectangle sourceExtent() const override;

    /**
     * Returns the list of fields of this layer.
     * This also includes fields which have not yet been saved to the provider.
     *
     * \returns A list of fields
     */
    inline QgsFields fields() const override { return mFields; }

    /**
     * Returns the list of fields of this layer.
     * This also includes fields which have not yet been saved to the provider.
     * Alias for fields()
     *
     * \returns A list of fields
     */
    inline QgsFields pendingFields() const { return mFields; }

    /**
     * Returns list of attribute indexes. i.e. a list from 0 ... fieldCount()
     * Alias for attributeList()
     */
    inline QgsAttributeList pendingAllAttributesList() const { return mFields.allAttributesList(); }

    /**
     * Returns list of attribute indexes. i.e. a list from 0 ... fieldCount()
     * Alias for attributeList()
     */
    inline QgsAttributeList attributeList() const { return mFields.allAttributesList(); }

    /**
     * Returns list of attributes making up the primary key
     * Alias for pkAttributeList()
     */
    inline QgsAttributeList pendingPkAttributesList() const { return pkAttributeList(); }

    //! Returns list of attributes making up the primary key
    QgsAttributeList pkAttributeList() const;

    /**
     * Returns feature count including changes which have not yet been committed
     * Alias for featureCount()
     */
    inline long pendingFeatureCount() const { return featureCount(); }

    /**
     * Returns feature count including changes which have not yet been committed
     * If you need only the count of committed features call this method on this layer's provider.
     */
    long featureCount() const override;

    /** Make layer read-only (editing disabled) or not
     *  \returns false if the layer is in editing yet
     */
    bool setReadOnly( bool readonly = true );

    //! Change feature's geometry
    bool changeGeometry( QgsFeatureId fid, const QgsGeometry &geom );

    /**
     * Changes an attribute value (but does not commit it)
     *
     * \param fid   The feature id of the feature to be changed
     * \param field The index of the field to be updated
     * \param newValue The value which will be assigned to the field
     * \param oldValue The previous value to restore on undo (will otherwise be retrieved)
     *
     * \returns true in case of success
     */
    bool changeAttributeValue( QgsFeatureId fid, int field, const QVariant &newValue, const QVariant &oldValue = QVariant() );

    /** Add an attribute field (but does not commit it)
     * returns true if the field was added
     */
    bool addAttribute( const QgsField &field );

    /**
     * Sets an alias (a display name) for attributes to display in dialogs
     *
     * \since QGIS 3.0
     */
    void setFieldAlias( int index, const QString &aliasString );

    /**
     * Removes an alias (a display name) for attributes to display in dialogs
     *
     * \since QGIS 3.0
     */
    void removeFieldAlias( int index );

    /** Renames an attribute field  (but does not commit it).
     * \param index attribute index
     * \param newName new name of field
     * \since QGIS 2.16
     */
    bool renameAttribute( int index, const QString &newName );

    /**
     * Returns the alias of an attribute name or a null string if there is no alias.
     *
     * \see {attributeDisplayName( int attributeIndex )} which returns the field name
     *      if no alias is defined.
     */
    QString attributeAlias( int index ) const;

    //! Convenience function that returns the attribute alias if defined or the field name else
    QString attributeDisplayName( int index ) const;

    //! Returns a map of field name to attribute alias
    QgsStringMap attributeAliases() const;

    /**
     * A set of attributes that are not advertised in WMS requests with QGIS server.
     */
    QSet<QString> excludeAttributesWms() const { return mExcludeAttributesWMS; }

    /**
     * A set of attributes that are not advertised in WMS requests with QGIS server.
     */
    void setExcludeAttributesWms( const QSet<QString> &att ) { mExcludeAttributesWMS = att; }

    /**
     * A set of attributes that are not advertised in WFS requests with QGIS server.
     */
    QSet<QString> excludeAttributesWfs() const { return mExcludeAttributesWFS; }

    /**
     * A set of attributes that are not advertised in WFS requests with QGIS server.
     */
    void setExcludeAttributesWfs( const QSet<QString> &att ) { mExcludeAttributesWFS = att; }

    //! Delete an attribute field (but does not commit it)
    bool deleteAttribute( int attr );

    /**
     * Deletes a list of attribute fields (but does not commit it)
     *
     * \param  attrs the indices of the attributes to delete
     * \returns true if at least one attribute has been deleted
     *
     */
    bool deleteAttributes( QList<int> attrs );

    bool addFeatures( QgsFeatureList &features, QgsFeatureSink::Flags flags = 0 ) override;

    //! Delete a feature from the layer (but does not commit it)
    bool deleteFeature( QgsFeatureId fid );

    /**
     * Deletes a set of features from the layer (but does not commit it)
     * \param fids The feature ids to delete
     *
     * \returns false if the layer is not in edit mode or does not support deleting
     *         in case of an active transaction depends on the provider implementation
     */
    bool deleteFeatures( const QgsFeatureIds &fids );

    /**
     * Attempts to commit any changes to disk.  Returns the result of the attempt.
     * If a commit fails, the in-memory changes are left alone.
     *
     * This allows editing to continue if the commit failed on e.g. a
     * disallowed value in a Postgres database - the user can re-edit and try
     * again.
     *
     * The commits occur in distinct stages,
     * (add attributes, add features, change attribute values, change
     * geometries, delete features, delete attributes)
     * so if a stage fails, it's difficult to roll back cleanly.
     * Therefore any error message also includes which stage failed so
     * that the user has some chance of repairing the damage cleanly.
     * \see commitErrors()
     */
    bool commitChanges();

    /** Returns a list containing any error messages generated when attempting
     * to commit changes to the layer.
     * \see commitChanges()
     */
    QStringList commitErrors() const;

    /** Stop editing and discard the edits
     * \param deleteBuffer whether to delete editing buffer
     */
    bool rollBack( bool deleteBuffer = true );

    /**
     * Get relations, where the foreign key is on this layer
     *
     * \param idx Only get relations, where idx forms part of the foreign key
     * \returns A list of relations
     */
    QList<QgsRelation> referencingRelations( int idx ) const;

    //! Buffer with uncommitted editing operations. Only valid after editing has been turned on.
    QgsVectorLayerEditBuffer *editBuffer() { return mEditBuffer; }

    //! Buffer with uncommitted editing operations. Only valid after editing has been turned on.
    //! \note not available in Python bindings
    const QgsVectorLayerEditBuffer *editBuffer() const SIP_SKIP { return mEditBuffer; }

    /**
     * Create edit command for undo/redo operations
     * \param text text which is to be displayed in undo window
     */
    void beginEditCommand( const QString &text );

    //! Finish edit command and add it to undo/redo stack
    void endEditCommand();

    //! Destroy active command and reverts all changes in it
    void destroyEditCommand();

    //! Editing vertex markers
    enum VertexMarkerType
    {
      SemiTransparentCircle,
      Cross,
      NoMarker
    };

    //! Draws a vertex symbol at (screen) coordinates x, y. (Useful to assist vertex editing.)
    static void drawVertexMarker( double x, double y, QPainter &p, QgsVectorLayer::VertexMarkerType type, int vertexSize );

    //! Assembles mUpdatedFields considering provider fields, joined fields and added fields
    void updateFields();

    /** Returns the calculated default value for the specified field index. The default
     * value may be taken from a client side default value expression (see setDefaultValueExpression())
     * or taken from the underlying data provider.
     * \param index field index
     * \param feature optional feature to use for default value evaluation. If passed,
     * then properties from the feature (such as geometry) can be used when calculating
     * the default value.
     * \param context optional expression context to evaluate expressions again. If not
     * specified, a default context will be created
     * \returns calculated default value
     * \since QGIS 3.0
     * \see setDefaultValueExpression()
     */
    QVariant defaultValue( int index, const QgsFeature &feature = QgsFeature(),
                           QgsExpressionContext *context = nullptr ) const;

    /** Sets an expression to use when calculating the default value for a field.
     * \param index field index
     * \param expression expression to evaluate when calculating default values for field. Pass
     * an empty expression to clear the default.
     * \since QGIS 3.0
     * \see defaultValue()
     * \see defaultValueExpression()
     */
    void setDefaultValueExpression( int index, const QString &expression );

    /** Returns the expression used when calculating the default value for a field.
     * \param index field index
     * \returns expression evaluated when calculating default values for field, or an
     * empty string if no default is set
     * \since QGIS 3.0
     * \see defaultValue()
     * \see setDefaultValueExpression()
     */
    QString defaultValueExpression( int index ) const;

    /**
     * Returns any constraints which are present for a specified
     * field index. These constraints may be inherited from the layer's data provider
     * or may be set manually on the vector layer from within QGIS.
     * \since QGIS 3.0
     * \see setFieldConstraint()
     */
    QgsFieldConstraints::Constraints fieldConstraints( int fieldIndex ) const;

    /**
     * Returns a map of constraint with their strength for a specific field of the layer.
     * \param fieldIndex field index
     * \since QGIS 3.0
     */
    QMap< QgsFieldConstraints::Constraint, QgsFieldConstraints::ConstraintStrength> fieldConstraintsAndStrength( int fieldIndex ) const;

    /**
     * Sets a constraint for a specified field index. Any constraints inherited from the layer's
     * data provider will be kept intact and cannot be modified. Ie, calling this method only allows for new
     * constraints to be added on top of the existing provider constraints.
     * \since QGIS 3.0
     * \see fieldConstraints()
     * \see removeFieldConstraint()
     */
    void setFieldConstraint( int index, QgsFieldConstraints::Constraint constraint, QgsFieldConstraints::ConstraintStrength strength = QgsFieldConstraints::ConstraintStrengthHard );

    /**
     * Removes a constraint for a specified field index. Any constraints inherited from the layer's
     * data provider will be kept intact and cannot be removed.
     * \since QGIS 3.0
     * \see fieldConstraints()
     * \see setFieldConstraint()
     */
    void removeFieldConstraint( int index, QgsFieldConstraints::Constraint constraint );

    /**
     * Returns the constraint expression for for a specified field index, if set.
     * \since QGIS 3.0
     * \see fieldConstraints()
     * \see constraintDescription()
     * \see setConstraintExpression()
     */
    QString constraintExpression( int index ) const;

    /**
     * Returns the descriptive name for the constraint expression for a specified field index.
     * \since QGIS 3.0
     * \see constraints()
     * \see constraintExpression()
     * \see setConstraintExpression()
     */
    QString constraintDescription( int index ) const;

    /**
     * Set the constraint expression for the specified field index. An optional descriptive name for the constraint
     * can also be set. Setting an empty expression will clear any existing expression constraint.
     * \since QGIS 3.0
     * \see constraintExpression()
     * \see constraintDescription()
     * \see constraints()
     */
    void setConstraintExpression( int index, const QString &expression, const QString &description = QString() );

    /**
     * \copydoc editorWidgetSetup
     */
    void setEditorWidgetSetup( int index, const QgsEditorWidgetSetup &setup );

    /**
     * The editor widget setup defines which QgsFieldFormatter and editor widget will be used
     * for the field at `index`.
     *
     * \since QGIS 3.0
     */
    QgsEditorWidgetSetup editorWidgetSetup( int index ) const;

    /** Calculates a list of unique values contained within an attribute in the layer. Note that
     * in some circumstances when unsaved changes are present for the layer then the returned list
     * may contain outdated values (for instance when the attribute value in a saved feature has
     * been changed inside the edit buffer then the previous saved value will be included in the
     * returned list).
     * \param fieldIndex column index for attribute
     * \param limit maximum number of values to return (or -1 if unlimited)
     * \see minimumValue()
     * \see maximumValue()
     */
    QSet<QVariant> uniqueValues( int fieldIndex, int limit = -1 ) const override;

    /**
     * Returns unique string values of an attribute which contain a specified subset string. Subset
     * matching is done in a case-insensitive manner. Note that
     * in some circumstances when unsaved changes are present for the layer then the returned list
     * may contain outdated values (for instance when the attribute value in a saved feature has
     * been changed inside the edit buffer then the previous saved value will be included in the
     * returned list).
     * \param index column index for attribute
     * \param substring substring to match (case insensitive)
     * \param limit maxmum number of the values to return, or -1 to return all unique values
     * \param feedback optional feedback object for canceling request
     * \returns list of unique strings containing substring
     */
    QStringList uniqueStringsMatching( int index, const QString &substring, int limit = -1,
                                       QgsFeedback *feedback = nullptr ) const;

    /** Returns the minimum value for an attribute column or an invalid variant in case of error.
     * Note that in some circumstances when unsaved changes are present for the layer then the
     * returned value may be outdated (for instance when the attribute value in a saved feature has
     * been changed inside the edit buffer then the previous saved value may be returned as the minimum).
     * \see maximumValue()
     * \see uniqueValues()
     */
    QVariant minimumValue( int index ) const override;

    /** Returns the maximum value for an attribute column or an invalid variant in case of error.
     * Note that in some circumstances when unsaved changes are present for the layer then the
     * returned value may be outdated (for instance when the attribute value in a saved feature has
     * been changed inside the edit buffer then the previous saved value may be returned as the maximum).
     * \see minimumValue()
     * \see uniqueValues()
     */
    QVariant maximumValue( int index ) const override;

    /** Calculates an aggregated value from the layer's features.
     * \param aggregate aggregate to calculate
     * \param fieldOrExpression source field or expression to use as basis for aggregated values.
     * \param parameters parameters controlling aggregate calculation
     * \param context expression context for expressions and filters
     * \param ok if specified, will be set to true if aggregate calculation was successful
     * \returns calculated aggregate value
     * \since QGIS 2.16
     */
    QVariant aggregate( QgsAggregateCalculator::Aggregate aggregate,
                        const QString &fieldOrExpression,
                        const QgsAggregateCalculator::AggregateParameters &parameters = QgsAggregateCalculator::AggregateParameters(),
                        QgsExpressionContext *context = nullptr,
                        bool *ok = nullptr ) const;

    /** Fetches all values from a specified field name or expression.
     * \param fieldOrExpression field name or an expression string
     * \param ok will be set to false if field or expression is invalid, otherwise true
     * \param selectedOnly set to true to get values from selected features only
     * \param feedback optional feedback object to allow cancelation
     * \returns list of fetched values
     * \since QGIS 2.9
     * \see getDoubleValues
     */
    QList< QVariant > getValues( const QString &fieldOrExpression, bool &ok, bool selectedOnly = false, QgsFeedback *feedback = nullptr ) const;

    /** Fetches all double values from a specified field name or expression. Null values or
     * invalid expression results are skipped.
     * \param fieldOrExpression field name or an expression string evaluating to a double value
     * \param ok will be set to false if field or expression is invalid, otherwise true
     * \param selectedOnly set to true to get values from selected features only
     * \param nullCount optional pointer to integer to store number of null values encountered in
     * \param feedback optional feedback object to allow cancelation
     * \returns list of fetched values
     * \since QGIS 2.9
     * \see getValues
     */
    QList< double > getDoubleValues( const QString &fieldOrExpression, bool &ok, bool selectedOnly = false, int *nullCount = nullptr, QgsFeedback *feedback = nullptr ) const;

    //! Set the blending mode used for rendering each feature
    void setFeatureBlendMode( QPainter::CompositionMode blendMode );
    //! Returns the current blending mode for features
    QPainter::CompositionMode featureBlendMode() const;

    /**
     * Sets the \a opacity for the vector layer, where \a opacity is a value between 0 (totally transparent)
     * and 1.0 (fully opaque).
     * \see opacity()
     * \see opacityChanged()
     * \since QGIS 3.0
     */
    void setOpacity( double opacity );

    /**
     * Returns the opacity for the vector layer, where opacity is a value between 0 (totally transparent)
     * and 1.0 (fully opaque).
     * \see setOpacity()
     * \see opacityChanged()
     * \since QGIS 3.0
     */
    double opacity() const;

    QString htmlMetadata() const override;

    /** Set the simplification settings for fast rendering of features
     *  \since QGIS 2.2
     */
    void setSimplifyMethod( const QgsVectorSimplifyMethod &simplifyMethod ) { mSimplifyMethod = simplifyMethod; }

    /** Returns the simplification settings for fast rendering of features
     *  \since QGIS 2.2
     */
    inline const QgsVectorSimplifyMethod &simplifyMethod() const { return mSimplifyMethod; }

    /** Returns whether the VectorLayer can apply the specified simplification hint
     *  \note Do not use in 3rd party code - may be removed in future version!
     *  \since QGIS 2.2
     */
    bool simplifyDrawingCanbeApplied( const QgsRenderContext &renderContext, QgsVectorSimplifyMethod::SimplifyHint simplifyHint ) const;

    /**
     * \brief Return the conditional styles that are set for this layer. Style information is
     * used to render conditional formatting in the attribute table.
     * \returns Return a QgsConditionalLayerStyles object holding the conditional attribute
     * style information. Style information is generic and can be used for anything.
     * \since QGIS 2.12
     */
    QgsConditionalLayerStyles *conditionalStyles() const;

    /**
     * Get the attribute table configuration object.
     * This defines the appearance of the attribute table.
     */
    QgsAttributeTableConfig attributeTableConfig() const;

    /**
     * Set the attribute table configuration object.
     * This defines the appearance of the attribute table.
     */
    void setAttributeTableConfig( const QgsAttributeTableConfig &attributeTableConfig );

    /**
     * The mapTip is a pretty, html representation for feature information.
     *
     * It may also contain embedded expressions.
     *
     * \since QGIS 3.0
     */
    QString mapTipTemplate() const;

    /**
     * The mapTip is a pretty, html representation for feature information.
     *
     * It may also contain embedded expressions.
     *
     * \since QGIS 3.0
     */
    void setMapTipTemplate( const QString &mapTipTemplate );

    QgsExpressionContext createExpressionContext() const override;

    /**
     * Get the configuration of the form used to represent this vector layer.
     * This is a writable configuration that can directly be changed in place.
     *
     * \returns The configuration of this layers' form
     *
     * \since QGIS 2.14
     */
    QgsEditFormConfig editFormConfig() const;

    /**
     * Get the configuration of the form used to represent this vector layer.
     * This is a writable configuration that can directly be changed in place.
     *
     * \returns The configuration of this layers' form
     *
     * \since QGIS 3.0
     */
    void setEditFormConfig( const QgsEditFormConfig &editFormConfig );

  public slots:

    /**
     * Select feature by its ID
     *
     * \param featureId  The id of the feature to select
     *
     * \see select(QgsFeatureIds)
     */
    void select( QgsFeatureId featureId );

    /**
     * Select features by their ID
     *
     * \param featureIds The ids of the features to select
     *
     * \see select(QgsFeatureId)
     */
    void select( const QgsFeatureIds &featureIds );

    /**
     * Deselect feature by its ID
     *
     * \param featureId  The id of the feature to deselect
     *
     * \see deselect(QgsFeatureIds)
     */
    void deselect( const QgsFeatureId featureId );

    /**
     * Deselect features by their ID
     *
     * \param featureIds The ids of the features to deselect
     *
     * \see deselect(QgsFeatureId)
     */
    void deselect( const QgsFeatureIds &featureIds );

    /**
     * Clear selection
     *
     * \see selectByIds()
     */
    void removeSelection();

    /** Update the extents for the layer. This is necessary if features are
     *  added/deleted or the layer has been subsetted.
     */
    virtual void updateExtents();

    /**
     * Make layer editable.
     * This starts an edit session on this layer. Changes made in this edit session will not
     * be made persistent until commitChanges() is called and can be reverted by calling
     * rollBack().
     */
    bool startEditing();


  protected slots:
    void invalidateSymbolCountedFlag();

  signals:

    /**
     * This signal is emitted when selection was changed
     *
     * \param selected        Newly selected feature ids
     * \param deselected      Ids of all features which have previously been selected but are not any more
     * \param clearAndSelect  In case this is set to true, the old selection was dismissed and the new selection corresponds to selected
     */
    void selectionChanged( const QgsFeatureIds &selected, const QgsFeatureIds &deselected, const bool clearAndSelect );

    //! This signal is emitted when modifications has been done on layer
    void layerModified();

    //! Is emitted, when layer is checked for modifications. Use for last-minute additions
    void beforeModifiedCheck() const;

    //! Is emitted, before editing on this layer is started
    void beforeEditingStarted();

    //! Is emitted, when editing on this layer has started
    void editingStarted();

    //! Is emitted, when edited changes successfully have been written to the data provider
    void editingStopped();

    //! Is emitted, before changes are committed to the data provider
    void beforeCommitChanges();

    //! Is emitted, before changes are rolled back
    void beforeRollBack();

    /**
     * Will be emitted, when a new attribute has been added to this vector layer.
     * Applies only to types QgsFields::OriginEdit, QgsFields::OriginProvider and QgsFields::OriginExpression
     *
     * \param idx The index of the new attribute
     *
     * \see updatedFields()
     */
    void attributeAdded( int idx );

    /**
     * Will be emitted, when an expression field is going to be added to this vector layer.
     * Applies only to types QgsFields::OriginExpression
     *
     * \param fieldName The name of the attribute to be added
     */
    void beforeAddingExpressionField( const QString &fieldName );

    /**
     * Will be emitted, when an attribute has been deleted from this vector layer.
     * Applies only to types QgsFields::OriginEdit, QgsFields::OriginProvider and QgsFields::OriginExpression
     *
     * \param idx The index of the deleted attribute
     *
     * \see updatedFields()
     */
    void attributeDeleted( int idx );

    /**
     * Will be emitted, when an expression field is going to be deleted from this vector layer.
     * Applies only to types QgsFields::OriginExpression
     *
     * \param idx The index of the attribute to be deleted
     */
    void beforeRemovingExpressionField( int idx );

    /**
     * Emitted when a new feature has been added to the layer
     *
     * \param fid The id of the new feature
     */
    void featureAdded( QgsFeatureId fid );

    /**
     * Emitted when a feature has been deleted.
     *
     * If you do expensive operations in a slot connected to this, you should prefer to use
     * featuresDeleted( const QgsFeatureIds& ).
     *
     * \param fid The id of the feature which has been deleted
     */
    void featureDeleted( QgsFeatureId fid );

    /**
     * Emitted when features have been deleted.
     *
     * If features are deleted within an edit command, this will only be emitted once at the end
     * to allow connected slots to minimize the overhead.
     * If features are deleted outside of an edit command, this signal will be emitted once per feature.
     *
     * \param fids The feature ids that have been deleted.
     */
    void featuresDeleted( const QgsFeatureIds &fids );

    /**
     * Is emitted, whenever the fields available from this layer have been changed.
     * This can be due to manually adding attributes or due to a join.
     */
    void updatedFields();


    /**
     * Is emitted whenever an attribute value change is done in the edit buffer.
     * Note that at this point the attribute change is not yet saved to the provider.
     *
     * \param fid The id of the changed feature
     * \param idx The attribute index of the changed attribute
     * \param value The new value of the attribute
     */
    void attributeValueChanged( QgsFeatureId fid, int idx, const QVariant &value );

    /**
     * Is emitted whenever a geometry change is done in the edit buffer.
     * Note that at this point the geometry change is not yet saved to the provider.
     *
     * \param fid The id of the changed feature
     * \param geometry The new geometry
     */
    void geometryChanged( QgsFeatureId fid, const QgsGeometry &geometry );

    //! This signal is emitted, when attributes are deleted from the provider
    void committedAttributesDeleted( const QString &layerId, const QgsAttributeList &deletedAttributes );
    //! This signal is emitted, when attributes are added to the provider
    void committedAttributesAdded( const QString &layerId, const QList<QgsField> &addedAttributes );
    //! This signal is emitted, when features are added to the provider
    void committedFeaturesAdded( const QString &layerId, const QgsFeatureList &addedFeatures );
    //! This signal is emitted, when features are deleted from the provider
    void committedFeaturesRemoved( const QString &layerId, const QgsFeatureIds &deletedFeatureIds );
    //! This signal is emitted, when attribute value changes are saved to the provider
    void committedAttributeValuesChanges( const QString &layerId, const QgsChangedAttributesMap &changedAttributesValues );
    //! This signal is emitted, when geometry changes are saved to the provider
    void committedGeometriesChanges( const QString &layerId, const QgsGeometryMap &changedGeometries );

    //! Emitted when the font family defined for labeling layer is not found on system
    void labelingFontNotFound( QgsVectorLayer *layer, const QString &fontfamily );

    //! Signal emitted when setFeatureBlendMode() is called
    void featureBlendModeChanged( QPainter::CompositionMode blendMode );

    /**
     * Emitted when the layer's opacity is changed, where \a opacity is a value between 0 (transparent)
     * and 1 (opaque).
     * \since QGIS 3.0
     * \see setOpacity()
     * \see opacity()
     */
    void opacityChanged( double opacity );

    /**
     * Signal emitted when a new edit command has been started
     *
     * \param text Description for this edit command
     */
    void editCommandStarted( const QString &text );

    /**
     * Signal emitted, when an edit command successfully ended
     * \note This does not mean it is also committed, only that it is written
     * to the edit buffer. See beforeCommitChanges()
     */
    void editCommandEnded();

    /**
     * Signal emitted, whan an edit command is destroyed
     * \note This is not a rollback, it is only related to the current edit command.
     * See beforeRollBack()
     */
    void editCommandDestroyed();

    /**
     * Signal emitted whenever the symbology (QML-file) for this layer is being read.
     * If there is custom style information saved in the file, you can connect to this signal
     * and update the layer style accordingly.
     *
     * \param element The XML layer style element.
     *
     * \param errorMessage Write error messages into this string.
     */
    void readCustomSymbology( const QDomElement &element, QString &errorMessage );

    /**
     * Signal emitted whenever the symbology (QML-file) for this layer is being written.
     * If there is custom style information you want to save to the file, you can connect
     * to this signal and update the element accordingly.
     *
     * \param element  The XML element where you can add additional style information to.
     * \param doc      The XML document that you can use to create new XML nodes.
     * \param errorMessage Write error messages into this string.
     */
    void writeCustomSymbology( QDomElement &element, QDomDocument &doc, QString &errorMessage ) const;

    /**
     * Emitted when the map tip changes
     *
     * \since QGIS 3.0
     */
    void mapTipTemplateChanged();

    /**
     * Emitted when the display expression changes
     *
     * \since QGIS 3.0
     */
    void displayExpressionChanged();

    /**
     * Signals an error related to this vector layer.
     */
    void raiseError( const QString &msg );

    /**
     * Will be emitted whenever the edit form configuration of this layer changes.
     *
     * \since QGIS 3.0
     */
    void editFormConfigChanged();

    /**
     * Emitted when the read only state of this layer is changed.
     * Only applies to manually set readonly state, not to the edit mode.
     *
     * \since QGIS 3.0
     */
    void readOnlyChanged();

    /**
     * Emitted when the feature count for symbols on this layer has been recalculated.
     *
     * \since QGIS 3.0
     */
    void symbolFeatureCountMapChanged();

  private slots:
    void onJoinedFieldsChanged();
    void onFeatureDeleted( QgsFeatureId fid );
    void onRelationsLoaded();
    void onSymbolsCounted();

  protected:
    //! Set the extent
    void setExtent( const QgsRectangle &rect ) override;

  private:                       // Private methods

    /**
     * Returns true if the provider is in read-only mode
     */
    virtual bool isReadOnly() const override;

    /** Bind layer to a specific data provider
     * \param provider should be "postgres", "ogr", or ??
     * @todo XXX should this return bool?  Throw exceptions?
     */
    bool setDataProvider( QString const &provider );

    //! Goes through all features and finds a free id (e.g. to give it temporarily to a not-committed feature)
    QgsFeatureId findFreeId();

    //! Read labeling from SLD
    void readSldLabeling( const QDomNode &node );

    //! Read simple labeling from layer's custom properties (QGIS 2.x projects)
    QgsAbstractVectorLayerLabeling *readLabelingFromCustomProperties();

#ifdef SIP_RUN
    QgsVectorLayer( const QgsVectorLayer &rhs );
#endif

  private:                       // Private attributes
    QgsConditionalLayerStyles *mConditionalStyles = nullptr;

    //! Pointer to data provider derived from the abastract base class QgsDataProvider
    QgsVectorDataProvider *mDataProvider = nullptr;

    //! The preview expression used to generate a human readable preview string for features
    QString mDisplayExpression;

    QString mMapTipTemplate;

    //! Data provider key
    QString mProviderKey;

    //! The user-defined actions that are accessed from the Identify Results dialog box
    QgsActionManager *mActions = nullptr;

    //! Flag indicating whether the layer is in read-only mode (editing disabled) or not
    bool mReadOnly;

    /** Set holding the feature IDs that are activated.  Note that if a feature
        subsequently gets deleted (i.e. by its addition to mDeletedFeatureIds),
        it always needs to be removed from mSelectedFeatureIds as well.
     */
    QgsFeatureIds mSelectedFeatureIds;

    //! Field map to commit
    QgsFields mFields;

    //! Map that stores the aliases for attributes. Key is the attribute name and value the alias for that attribute
    QgsStringMap mAttributeAliasMap;

    //! Map which stores default value expressions for fields
    QgsStringMap mDefaultExpressionMap;

    //! Map which stores constraints for fields
    QMap< QString, QgsFieldConstraints::Constraints > mFieldConstraints;

    //! Map which stores constraint strength for fields
    QMap< QPair< QString, QgsFieldConstraints::Constraint >, QgsFieldConstraints::ConstraintStrength > mFieldConstraintStrength;

    //! Map which stores expression constraints for fields. Value is a pair of expression/description.
    QMap< QString, QPair< QString, QString > > mFieldConstraintExpressions;

    QMap< QString, QgsEditorWidgetSetup > mFieldWidgetSetups;

    //! Holds the configuration for the edit form
    QgsEditFormConfig mEditFormConfig;

    //! Attributes which are not published in WMS
    QSet<QString> mExcludeAttributesWMS;

    //! Attributes which are not published in WFS
    QSet<QString> mExcludeAttributesWFS;

    //! Geometry type as defined in enum WkbType (qgis.h)
    QgsWkbTypes::Type mWkbType;

    //! Renderer object which holds the information about how to display the features
    QgsFeatureRenderer *mRenderer = nullptr;

    //! Simplification object which holds the information about how to simplify the features for fast rendering
    QgsVectorSimplifyMethod mSimplifyMethod;

    //! Labeling configuration
    QgsAbstractVectorLayerLabeling *mLabeling = nullptr;

    //! Whether 'labeling font not found' has be shown for this layer (only show once in QgsMessageBar, on first rendering)
    bool mLabelFontNotFoundNotified;

    //! Blend mode for features
    QPainter::CompositionMode mFeatureBlendMode;

    //! Layer opacity
    double mLayerOpacity = 1.0;

    //! Flag if the vertex markers should be drawn only for selection (true) or for all features (false)
    bool mVertexMarkerOnlyForSelection;

    QStringList mCommitErrors;

    //! stores information about uncommitted changes to layer
    QgsVectorLayerEditBuffer *mEditBuffer = nullptr;
    friend class QgsVectorLayerEditBuffer;

    //stores information about joined layers
    QgsVectorLayerJoinBuffer *mJoinBuffer = nullptr;

    //! stores information about expression fields on this layer
    QgsExpressionFieldBuffer *mExpressionFieldBuffer = nullptr;

    //diagram rendering object. 0 if diagram drawing is disabled
    QgsDiagramRenderer *mDiagramRenderer = nullptr;

    //stores infos about diagram placement (placement type, priority, position distance)
    QgsDiagramLayerSettings *mDiagramLayerSettings = nullptr;

    mutable bool mValidExtent;
    mutable bool mLazyExtent;

    // Features in renderer classes counted
    bool mSymbolFeatureCounted;

    // Feature counts for each renderer legend key
    QHash<QString, long> mSymbolFeatureCountMap;

    //! True while an undo command is active
    bool mEditCommandActive;

    QgsFeatureIds mDeletedFids;

    QgsAttributeTableConfig mAttributeTableConfig;

    mutable QMutex mFeatureSourceConstructorMutex;

    QgsVectorLayerFeatureCounter *mFeatureCounter = nullptr;

    friend class QgsVectorLayerFeatureSource;
};

#endif
