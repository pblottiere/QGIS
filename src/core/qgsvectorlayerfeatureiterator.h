/***************************************************************************
    qgsvectorlayerfeatureiterator.h
    ---------------------
    begin                : Dezember 2012
    copyright            : (C) 2012 by Martin Dobias
    email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSVECTORLAYERFEATUREITERATOR_H
#define QGSVECTORLAYERFEATUREITERATOR_H

#include "qgis_core.h"
#include "qgis_sip.h"
#include "qgsfeatureiterator.h"
#include "qgsfields.h"
#include "qgscoordinatereferencesystem.h"
#include "qgsfeaturesource.h"

#include <QSet>
#include <memory>

typedef QMap<QgsFeatureId, QgsFeature> QgsFeatureMap SIP_SKIP;

class QgsExpressionFieldBuffer;
class QgsVectorLayer;
class QgsVectorLayerEditBuffer;
class QgsVectorLayerJoinBuffer;
class QgsVectorLayerJoinInfo;
class QgsExpressionContext;

class QgsVectorLayerFeatureIterator;

#ifdef SIP_RUN
% ModuleHeaderCode
#include "qgsfeatureiterator.h"
% End
#endif

/** \ingroup core
 * Partial snapshot of vector layer's state (only the members necessary for access to features)
*/
class CORE_EXPORT QgsVectorLayerFeatureSource : public QgsAbstractFeatureSource
{
  public:

    /** Constructor for QgsVectorLayerFeatureSource.
     * \param layer source layer
     */
    explicit QgsVectorLayerFeatureSource( const QgsVectorLayer *layer );

    ~QgsVectorLayerFeatureSource();

    virtual QgsFeatureIterator getFeatures( const QgsFeatureRequest &request = QgsFeatureRequest() ) override;

    friend class QgsVectorLayerFeatureIterator SIP_SKIP;

    /**
     * Returns the fields that will be available for features that are retrieved from
     * this source.
     *
     * \since QGIS 3.0
     */
    QgsFields fields() const;

    /**
     * Returns the coordinate reference system for features retrieved from this source.
     * \since QGIS 3.0
     */
    QgsCoordinateReferenceSystem crs() const;

  protected:

    QgsAbstractFeatureSource *mProviderFeatureSource = nullptr;

    QgsVectorLayerJoinBuffer *mJoinBuffer = nullptr;

    QgsExpressionFieldBuffer *mExpressionFieldBuffer = nullptr;

    QgsFields mFields;

    bool mHasEditBuffer;

    // A deep-copy is only performed, if the original maps change
    // see here https://github.com/qgis/Quantum-GIS/pull/673
    // for explanation
    QgsFeatureMap mAddedFeatures;
    QgsGeometryMap mChangedGeometries;
    QgsFeatureIds mDeletedFeatureIds;
    QList<QgsField> mAddedAttributes;
    QgsChangedAttributesMap mChangedAttributeValues;
    QgsAttributeList mDeletedAttributeIds;

    QgsCoordinateReferenceSystem mCrs;
};

/** \ingroup core
 */
class CORE_EXPORT QgsVectorLayerFeatureIterator : public QgsAbstractFeatureIteratorFromSource<QgsVectorLayerFeatureSource>
{
  public:
    QgsVectorLayerFeatureIterator( QgsVectorLayerFeatureSource *source, bool ownSource, const QgsFeatureRequest &request );

    ~QgsVectorLayerFeatureIterator();

    //! reset the iterator to the starting position
    virtual bool rewind() override;

    //! end of iterating: free the resources / lock
    virtual bool close() override;

    virtual void setInterruptionChecker( QgsInterruptionChecker *interruptionChecker ) override SIP_SKIP;

    /** Join information prepared for fast attribute id mapping in QgsVectorLayerJoinBuffer::updateFeatureAttributes().
     * Created in the select() method of QgsVectorLayerJoinBuffer for the joins that contain fetched attributes
     */
    struct CORE_EXPORT FetchJoinInfo
    {
      const QgsVectorLayerJoinInfo *joinInfo;//!< Canonical source of information about the join
      QgsAttributeList attributes;      //!< Attributes to fetch
      int indexOffset;                  //!< At what position the joined fields start
      QgsVectorLayer *joinLayer;        //!< Resolved pointer to the joined layer
      int targetField;                  //!< Index of field (of this layer) that drives the join
      int joinField;                    //!< Index of field (of the joined layer) must have equal value

      void addJoinedAttributesCached( QgsFeature &f, const QVariant &joinValue ) const;
      void addJoinedAttributesDirect( QgsFeature &f, const QVariant &joinValue ) const;
      bool getFeature( const QgsFeatureRequest &request, QgsFeature &f ) const;
    };


  protected:
    //! fetch next feature, return true on success
    virtual bool fetchFeature( QgsFeature &feature ) override;

    //! Overrides default method as we only need to filter features in the edit buffer
    //! while for others filtering is left to the provider implementation.
    virtual bool nextFeatureFilterExpression( QgsFeature &f ) override { return fetchFeature( f ); }

    //! Setup the simplification of geometries to fetch using the specified simplify method
    virtual bool prepareSimplification( const QgsSimplifyMethod &simplifyMethod ) override;

    //! \note not available in Python bindings
    void rewindEditBuffer() SIP_SKIP;

    //! \note not available in Python bindings
    void prepareJoin( int fieldIdx ) SIP_SKIP;

    //! \note not available in Python bindings
    void prepareExpression( int fieldIdx ) SIP_SKIP;

    //! \note not available in Python bindings
    void prepareFields() SIP_SKIP;

    //! \note not available in Python bindings
    void prepareField( int fieldIdx ) SIP_SKIP;

    //! \note not available in Python bindings
    bool fetchNextAddedFeature( QgsFeature &f ) SIP_SKIP;
    //! \note not available in Python bindings
    bool fetchNextChangedGeomFeature( QgsFeature &f ) SIP_SKIP;
    //! \note not available in Python bindings
    bool fetchNextChangedAttributeFeature( QgsFeature &f ) SIP_SKIP;
    //! \note not available in Python bindings
    void useAddedFeature( const QgsFeature &src, QgsFeature &f ) SIP_SKIP;
    //! \note not available in Python bindings
    void useChangedAttributeFeature( QgsFeatureId fid, const QgsGeometry &geom, QgsFeature &f ) SIP_SKIP;
    //! \note not available in Python bindings
    bool nextFeatureFid( QgsFeature &f ) SIP_SKIP;
    //! \note not available in Python bindings
    void addJoinedAttributesByValue( QgsFeature &f ) SIP_SKIP;
    void addJoinedAttributesByFeatureId( QgsFeature &f ) SIP_SKIP;

    /**
     * Adds attributes that don't source from the provider but are added inside QGIS
     * Includes
     *  - Joined fields
     *  - Expression fields
     *
     * \param f The feature will be modified
     * \note not available in Python bindings
     */
    void addVirtualAttributes( QgsFeature &f ) SIP_SKIP;

    /** Adds an expression based attribute to a feature
     * \param f feature
     * \param attrIndex attribute index
     * \since QGIS 2.14
     * \note not available in Python bindings
     */
    void addExpressionAttribute( QgsFeature &f, int attrIndex ) SIP_SKIP;

    /** Update feature with uncommitted attribute updates.
     * \note not available in Python bindings
     */
    void updateChangedAttributes( QgsFeature &f ) SIP_SKIP;

    /** Update feature with uncommitted geometry updates.
     * \note not available in Python bindings
     */
    void updateFeatureGeometry( QgsFeature &f ) SIP_SKIP;

    QgsFeatureRequest mProviderRequest;
    QgsFeatureIterator mProviderIterator;
    QgsFeatureRequest mChangedFeaturesRequest;
    QgsFeatureIterator mChangedFeaturesIterator;

    QgsRectangle mFilterRect;
    QgsCoordinateTransform mTransform;

    // only related to editing
    QSet<QgsFeatureId> mFetchConsidered;
    QgsGeometryMap::ConstIterator mFetchChangedGeomIt;
    QgsFeatureMap::ConstIterator mFetchAddedFeaturesIt;

    bool mFetchedFid; // when iterating by FID: indicator whether it has been fetched yet or not

    /** Information about joins used in the current select() statement.
      Allows faster mapping of attribute ids compared to mVectorJoins */
    QMap<const QgsVectorLayerJoinInfo *, QgsVectorLayerFeatureIterator::FetchJoinInfo> mFetchJoinInfo;

    QMap<int, QgsExpression *> mExpressionFieldInfo;

    bool mHasVirtualAttributes;

  private:
#ifdef SIP_RUN
    QgsVectorLayerFeatureIterator( const QgsVectorLayerFeatureIterator &rhs );
#endif

    std::unique_ptr<QgsExpressionContext> mExpressionContext;

    QgsInterruptionChecker *mInterruptionChecker = nullptr;

    QList< int > mPreparedFields;
    QList< int > mFieldsToPrepare;

    //! Join list sorted by dependency
    QList< FetchJoinInfo > mOrderedJoinInfoList;

    /**
     * Will always return true. We assume that ordering has been done on provider level already.
     *
     */
    bool prepareOrderBy( const QList<QgsFeatureRequest::OrderByClause> &orderBys ) override;

    //! returns whether the iterator supports simplify geometries on provider side
    virtual bool providerCanSimplify( QgsSimplifyMethod::MethodType methodType ) const override;

    void createOrderedJoinList();

    /**
     * Performs any post-processing (such as transformation) and feature based validity checking, e.g. checking for geometry validity.
     */
    bool postProcessFeature( QgsFeature &feature );

    /**
     * Checks a feature's geometry for validity, if requested in feature request.
     */
    bool checkGeometryValidity( const QgsFeature &feature );
};



/**
 * \class QgsVectorLayerSelectedFeatureSource
 * \ingroup core
 * QgsFeatureSource subclass for the selected features from a QgsVectorLayer.
 * \since QGIS 3.0
 */
class CORE_EXPORT QgsVectorLayerSelectedFeatureSource : public QgsFeatureSource
{
  public:

    /**
     * Constructor for QgsVectorLayerSelectedFeatureSource, for selected features from the specified \a layer.
     * The currently selected feature IDs are stored, so change to the layer selection after constructing
     * the QgsVectorLayerSelectedFeatureSource will not be reflected.
     */
    QgsVectorLayerSelectedFeatureSource( QgsVectorLayer *layer );

    virtual QgsFeatureIterator getFeatures( const QgsFeatureRequest &request = QgsFeatureRequest() ) const override;
    virtual QgsCoordinateReferenceSystem sourceCrs() const override;
    virtual QgsFields fields() const override;
    virtual QgsWkbTypes::Type wkbType() const override;
    virtual long featureCount() const override;

  private:

    // ideally this wouldn't be mutable, but QgsVectorLayerFeatureSource has non-const getFeatures()
    mutable QgsVectorLayerFeatureSource mSource;
    QgsFeatureIds mSelectedFeatureIds;
    QgsWkbTypes::Type mWkbType = QgsWkbTypes::Unknown;

};

#endif // QGSVECTORLAYERFEATUREITERATOR_H
