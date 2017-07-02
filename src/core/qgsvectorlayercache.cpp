/***************************************************************************
  qgsvectorlayercache.cpp
  Cache features of a vector layer
  -------------------
         begin                : January 2013
         copyright            : (C) Matthias Kuhn
         email                : matthias at opengis dot ch

 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsvectorlayercache.h"
#include "qgscacheindex.h"
#include "qgsvectorlayerjoinbuffer.h"
#include "qgsvectorlayereditbuffer.h"
#include "qgscachedfeatureiterator.h"

QgsVectorLayerCache::QgsVectorLayerCache( QgsVectorLayer *layer, int cacheSize, QObject *parent )
  : QObject( parent )
  , mLayer( layer )
  , mFullCache( false )
{
  std::cout << "QgsVectorLayerCache::QgsVectorLayerCache NEW!!!!!" << std::endl;
  mCache.setMaxCost( cacheSize );

  connect( mLayer, &QgsVectorLayer::featureDeleted, this, &QgsVectorLayerCache::featureDeleted );
  connect( mLayer, &QgsVectorLayer::featureAdded, this, &QgsVectorLayerCache::onFeatureAdded );
  connect( mLayer, &QgsVectorLayer::destroyed, this, &QgsVectorLayerCache::layerDeleted );

  setCacheGeometry( true );
  setCacheSubsetOfAttributes( mLayer->attributeList() );
  setCacheAddedAttributes( true );

  connect( mLayer, &QgsVectorLayer::attributeDeleted, this, &QgsVectorLayerCache::attributeDeleted );
  connect( mLayer, &QgsVectorLayer::updatedFields, this, &QgsVectorLayerCache::invalidate );
  connect( mLayer, &QgsVectorLayer::dataChanged, this, &QgsVectorLayerCache::invalidate );
  connect( mLayer, &QgsVectorLayer::attributeValueChanged, this, &QgsVectorLayerCache::onAttributeValueChanged );

  QgsVectorJoinList::const_iterator it = mLayer->vectorJoins().begin();
  for ( ; it != mLayer->vectorJoins().end(); ++it )
  {
    QgsVectorLayer *vl = it->joinLayer();

    if ( vl )
    {
      connect( vl, &QgsVectorLayer::attributeValueChanged, this, &QgsVectorLayerCache::onAttributeValueChanged );
    }
  }
}

QgsVectorLayerCache::~QgsVectorLayerCache()
{
  qDeleteAll( mCacheIndices );
  mCacheIndices.clear();
}

void QgsVectorLayerCache::setCacheSize( int cacheSize )
{
  mCache.setMaxCost( cacheSize );
}

int QgsVectorLayerCache::cacheSize()
{
  return mCache.maxCost();
}

void QgsVectorLayerCache::setCacheGeometry( bool cacheGeometry )
{
  bool shouldCacheGeometry = cacheGeometry && mLayer->hasGeometryType();
  bool mustInvalidate = shouldCacheGeometry && !mCacheGeometry; // going from no geometry -> geometry, so have to clear existing cache entries
  mCacheGeometry = shouldCacheGeometry;
  if ( cacheGeometry )
  {
    connect( mLayer, &QgsVectorLayer::geometryChanged, this, &QgsVectorLayerCache::geometryChanged );
  }
  else
  {
    disconnect( mLayer, &QgsVectorLayer::geometryChanged, this, &QgsVectorLayerCache::geometryChanged );
  }
  if ( mustInvalidate )
  {
    invalidate();
  }
}

void QgsVectorLayerCache::setCacheSubsetOfAttributes( const QgsAttributeList &attributes )
{
  mCachedAttributes = attributes;
}

void QgsVectorLayerCache::setFullCache( bool fullCache )
{
  std::cout << "QgsVectorLayerCache::setFullCache 0" << std::endl;
  mFullCache = fullCache;

  if ( mFullCache )
  {
    // Add a little more than necessary...
    setCacheSize( mLayer->featureCount() + 100 );

    // Initialize the cache...
    std::cout << "QgsVectorLayerCache::setFullCache 1" << std::endl;
    QgsFeatureIterator it( new QgsCachedFeatureWriterIterator( this, QgsFeatureRequest()
                           .setSubsetOfAttributes( mCachedAttributes )
                           .setFlags( mCacheGeometry ? QgsFeatureRequest::NoFlags : QgsFeatureRequest::NoGeometry ) ) );

    int i = 0;

    QTime t;
    t.start();

    QgsFeature f;
    while ( it.nextFeature( f ) )
    {
      ++i;

      if ( t.elapsed() > 1000 )
      {
        bool cancel = false;
        emit progress( i, cancel );
        if ( cancel )
          break;

        t.restart();
      }
    }

    it.close();

    emit finished();
  }
}

void QgsVectorLayerCache::addCacheIndex( QgsAbstractCacheIndex *cacheIndex )
{
  mCacheIndices.append( cacheIndex );
}

void QgsVectorLayerCache::setCacheAddedAttributes( bool cacheAddedAttributes )
{
  if ( cacheAddedAttributes )
  {
    connect( mLayer, &QgsVectorLayer::attributeAdded, this, &QgsVectorLayerCache::attributeAdded );
  }
  else
  {
    disconnect( mLayer, &QgsVectorLayer::attributeAdded, this, &QgsVectorLayerCache::attributeAdded );
  }
}

bool QgsVectorLayerCache::featureAtId( QgsFeatureId featureId, QgsFeature &feature, bool skipCache )
{
  std::cout << "QgsVectorLayerCache::featureAtId 0" << std::endl;
  bool featureFound = false;

  QgsCachedFeature *cachedFeature = nullptr;

  if ( !skipCache )
  {
    std::cout << "QgsVectorLayerCache::featureAtId 1" << std::endl;
    cachedFeature = mCache[ featureId ];
  }

  if ( cachedFeature )
  {
    std::cout << "QgsVectorLayerCache::featureAtId 2" << std::endl;
    feature = QgsFeature( *cachedFeature->feature() );
    // syncFeatureWithJoinedLayers( feature );
    featureFound = true;
  }
  else if ( mLayer->getFeatures( QgsFeatureRequest()
                                 .setFilterFid( featureId )
                                 .setSubsetOfAttributes( mCachedAttributes )
                                 .setFlags( !mCacheGeometry ? QgsFeatureRequest::NoGeometry : QgsFeatureRequest::Flags( nullptr ) ) )
            .nextFeature( feature ) )
  {
    std::cout << "QgsVectorLayerCache::featureAtId 3" << std::endl;

    // syncFeatureWithJoinedLayers( feature );

    cacheFeature( feature );
    featureFound = true;
  }

  return featureFound;
}

void QgsVectorLayerCache::syncFeatureWithJoinedLayers( QgsFeature &feature )
{
  std::cout << "SYNC FEATURE 0!!!!!" << std::endl;
  // update the feature with not committed data of join layers
  QgsVectorJoinList::const_iterator it = mLayer->vectorJoins().begin();
  for ( ; it != mLayer->vectorJoins().end(); ++it )
  {
    std::cout << "SYNC FEATURE 1!!!!!" << std::endl;
    const QgsVectorLayerJoinInfo *info = &( *it );
    QgsVectorLayer *joinLayer = info->joinLayer();

    if ( joinLayer && joinLayer->editBuffer() )
    {
      if ( joinLayer->editBuffer()->isFeatureAttributesChanged( feature.id() ) )
      {
        QgsAttributeMap attrs = joinLayer->editBuffer()->changedAttributeValues( feature.id() );
        QgsAttributeMap::const_iterator ait = attrs.begin();
        for ( ; ait != attrs.end(); ++ait )
        {
          QString fieldName = QString( "%1_%2" ).arg( joinLayer->name(), joinLayer->fields().field( ait.key() ).name() );
          std::cout << "SYNC FEATURE for " << fieldName.toStdString() << std::endl;
          int idxOffset = mLayer->joinBuffer()->joinedFieldsOffset( info, feature.fields() );
          std::cout << "SYNC FEATURE offset field " << ait.key() << std::endl;
          std::cout << "SYNC FEATURE offset index " << idxOffset << std::endl;
          feature.setAttribute( fieldName, ait.value() );
        }
      }
    }
  }
}

bool QgsVectorLayerCache::removeCachedFeature( QgsFeatureId fid )
{
  return mCache.remove( fid );
}

QgsVectorLayer *QgsVectorLayerCache::layer()
{
  return mLayer;
}

void QgsVectorLayerCache::requestCompleted( const QgsFeatureRequest &featureRequest, const QgsFeatureIds &fids )
{
  // If a request is too large for the cache don't notify to prevent from indexing incomplete requests
  if ( fids.count() <= mCache.size() )
  {
    Q_FOREACH ( QgsAbstractCacheIndex *idx, mCacheIndices )
    {
      idx->requestCompleted( featureRequest, fids );
    }
    if ( featureRequest.filterType() == QgsFeatureRequest::FilterNone )
    {
      mFullCache = true;
    }
  }
}

void QgsVectorLayerCache::featureRemoved( QgsFeatureId fid )
{
  Q_FOREACH ( QgsAbstractCacheIndex *idx, mCacheIndices )
  {
    idx->flushFeature( fid );
  }
}

void QgsVectorLayerCache::onAttributeValueChanged( QgsFeatureId fid, int field, const QVariant &value )
{
  QgsCachedFeature *cachedFeat = mCache[ fid ];

  if ( cachedFeat )
  {
    cachedFeat->mFeature->setAttribute( field, value );
  }

  emit attributeValueChanged( fid, field, value );
}

void QgsVectorLayerCache::featureDeleted( QgsFeatureId fid )
{
  mCache.remove( fid );
}

void QgsVectorLayerCache::onFeatureAdded( QgsFeatureId fid )
{
  if ( mFullCache )
  {
    if ( cacheSize() <= mLayer->featureCount() )
    {
      setCacheSize( mLayer->featureCount() + 100 );
    }

    QgsFeature feat;
    featureAtId( fid, feat );
  }
  emit featureAdded( fid );
}

void QgsVectorLayerCache::attributeAdded( int field )
{
  Q_UNUSED( field )
  mCachedAttributes.append( field );
  invalidate();
}

void QgsVectorLayerCache::attributeDeleted( int field )
{
  QgsAttributeList attrs = mCachedAttributes;
  mCachedAttributes.clear();

  Q_FOREACH ( int attr, attrs )
  {
    if ( attr < field )
      mCachedAttributes << attr;
    else if ( attr > field )
      mCachedAttributes << attr - 1;
  }
}

void QgsVectorLayerCache::geometryChanged( QgsFeatureId fid, const QgsGeometry &geom )
{
  QgsCachedFeature *cachedFeat = mCache[ fid ];

  if ( cachedFeat )
  {
    cachedFeat->mFeature->setGeometry( geom );
  }
}

void QgsVectorLayerCache::layerDeleted()
{
  emit cachedLayerDeleted();
  mLayer = nullptr;
}

void QgsVectorLayerCache::invalidate()
{
  mCache.clear();
  mFullCache = false;
  emit invalidated();
}

bool QgsVectorLayerCache::canUseCacheForRequest( const QgsFeatureRequest &featureRequest, QgsFeatureIterator &it )
{
  // check first for available indices
  Q_FOREACH ( QgsAbstractCacheIndex *idx, mCacheIndices )
  {
    if ( idx->getCacheIterator( it, featureRequest ) )
    {
      return true;
    }
  }

  // no indexes available, but maybe we have already cached all required features anyway?
  switch ( featureRequest.filterType() )
  {
    case QgsFeatureRequest::FilterFid:
    {
      if ( mCache.contains( featureRequest.filterFid() ) )
      {
        it = QgsFeatureIterator( new QgsCachedFeatureIterator( this, featureRequest ) );
        return true;
      }
      break;
    }
    case QgsFeatureRequest::FilterFids:
    {
      if ( mCache.keys().toSet().contains( featureRequest.filterFids() ) )
      {
        it = QgsFeatureIterator( new QgsCachedFeatureIterator( this, featureRequest ) );
        return true;
      }
      break;
    }
    case QgsFeatureRequest::FilterNone:
    case QgsFeatureRequest::FilterExpression:
    {
      if ( mFullCache )
      {
        it = QgsFeatureIterator( new QgsCachedFeatureIterator( this, featureRequest ) );
        return true;
      }
      break;
    }

  }
  return false;
}

QgsFeatureIterator QgsVectorLayerCache::getFeatures( const QgsFeatureRequest &featureRequest )
{
  std::cout << "QgsVectorLayerCache::getFeatures 0" << std::endl;
  QgsFeatureIterator it;
  bool requiresWriterIt = true; // If a not yet cached, but cachable request is made, this stays true.

  if ( checkInformationCovered( featureRequest ) )
  {
    std::cout << "QgsVectorLayerCache::getFeatures 1" << std::endl;
    // If we have a full cache available, run on this
    if ( mFullCache )
    {
      std::cout << "QgsVectorLayerCache::getFeatures 2" << std::endl;
      it = QgsFeatureIterator( new QgsCachedFeatureIterator( this, featureRequest ) );
      requiresWriterIt = false;
    }
    else
    {
      std::cout << "QgsVectorLayerCache::getFeatures 3" << std::endl;
      // may still be able to satisfy request using cache
      requiresWriterIt = !canUseCacheForRequest( featureRequest, it );
    }
  }
  else
  {
    std::cout << "QgsVectorLayerCache::getFeatures 4" << std::endl;
    // Let the layer answer the request, so no caching of requests
    // we don't want to cache is done
    requiresWriterIt = false;
    it = mLayer->getFeatures( featureRequest );
  }

  if ( requiresWriterIt && mLayer->dataProvider() )
  {
    std::cout << "QgsVectorLayerCache::getFeatures 5" << std::endl;
    // No index was able to satisfy the request
    QgsFeatureRequest myRequest = QgsFeatureRequest( featureRequest );

    // Make sure if we cache the geometry, it gets fetched
    if ( mCacheGeometry && mLayer->hasGeometryType() )
      myRequest.setFlags( featureRequest.flags() & ~QgsFeatureRequest::NoGeometry );

    // Make sure, all the cached attributes are requested as well
    QSet<int> attrs = featureRequest.subsetOfAttributes().toSet() + mCachedAttributes.toSet();
    myRequest.setSubsetOfAttributes( attrs.toList() );

    it = QgsFeatureIterator( new QgsCachedFeatureWriterIterator( this, myRequest ) );
  }

  return it;
}

bool QgsVectorLayerCache::isFidCached( const QgsFeatureId fid ) const
{
  return mCache.contains( fid );
}

bool QgsVectorLayerCache::checkInformationCovered( const QgsFeatureRequest &featureRequest )
{
  QgsAttributeList requestedAttributes;

  if ( !featureRequest.flags().testFlag( QgsFeatureRequest::SubsetOfAttributes ) )
  {
    requestedAttributes = mLayer->attributeList();
  }
  else
  {
    requestedAttributes = featureRequest.subsetOfAttributes();
  }

  // Check if we even cache the information requested
  Q_FOREACH ( int attr, requestedAttributes )
  {
    if ( !mCachedAttributes.contains( attr ) )
    {
      return false;
    }
  }

  // If the request needs geometry but we don't cache this...
  if ( !featureRequest.flags().testFlag( QgsFeatureRequest::NoGeometry )
       && !mCacheGeometry )
  {
    return false;
  }

  return true;
}
