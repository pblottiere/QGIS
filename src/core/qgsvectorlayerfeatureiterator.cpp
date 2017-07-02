/***************************************************************************
    qgsvectorlayerfeatureiterator.cpp
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
#include "qgsvectorlayerfeatureiterator.h"

#include "qgsexpressionfieldbuffer.h"
#include "qgsgeometrysimplifier.h"
#include "qgssimplifymethod.h"
#include "qgsvectordataprovider.h"
#include "qgsvectorlayereditbuffer.h"
#include "qgsvectorlayer.h"
#include "qgsvectorlayerjoinbuffer.h"
#include "qgsexpressioncontext.h"
#include "qgsdistancearea.h"
#include "qgsproject.h"

QgsVectorLayerFeatureSource::QgsVectorLayerFeatureSource( const QgsVectorLayer *layer )
  : mCrsId( 0 )
{
  QMutexLocker locker( &layer->mFeatureSourceConstructorMutex );
  mProviderFeatureSource = layer->dataProvider()->featureSource();
  mFields = layer->fields();

  // update layer's join caches if necessary
  if ( layer->mJoinBuffer->containsJoins() )
    layer->mJoinBuffer->createJoinCaches();

  mJoinBuffer = layer->mJoinBuffer->clone();

  mExpressionFieldBuffer = new QgsExpressionFieldBuffer( *layer->mExpressionFieldBuffer );
  mCrsId = layer->crs().srsid();

  mHasEditBuffer = layer->editBuffer();
  if ( mHasEditBuffer )
  {
#if 0
    // TODO[MD]: after merge
    if ( request.filterType() == QgsFeatureRequest::FilterFid )
    {

      // only copy relevant parts
      if ( L->editBuffer()->addedFeatures().contains( request.filterFid() ) )
        mAddedFeatures.insert( request.filterFid(), L->editBuffer()->addedFeatures()[ request.filterFid()] );

      if ( L->editBuffer()->changedGeometries().contains( request.filterFid() ) )
        mChangedGeometries.insert( request.filterFid(), L->editBuffer()->changedGeometries()[ request.filterFid()] );

      if ( L->editBuffer()->deletedFeatureIds().contains( request.filterFid() ) )
        mDeletedFeatureIds.insert( request.filterFid() );

      if ( L->editBuffer()->changedAttributeValues().contains( request.filterFid() ) )
        mChangedAttributeValues.insert( request.filterFid(), L->editBuffer()->changedAttributeValues()[ request.filterFid()] );

      if ( L->editBuffer()->changedAttributeValues().contains( request.filterFid() ) )
        mChangedFeaturesRequest.setFilterFids( QgsFeatureIds() << request.filterFid() );
    }
    else
    {
#endif
      mAddedFeatures = QgsFeatureMap( layer->editBuffer()->addedFeatures() );
      mChangedGeometries = QgsGeometryMap( layer->editBuffer()->changedGeometries() );
      mDeletedFeatureIds = QgsFeatureIds( layer->editBuffer()->deletedFeatureIds() );
      mChangedAttributeValues = QgsChangedAttributesMap( layer->editBuffer()->changedAttributeValues() );
      mAddedAttributes = QList<QgsField>( layer->editBuffer()->addedAttributes() );
      mDeletedAttributeIds = QgsAttributeList( layer->editBuffer()->deletedAttributeIds() );
#if 0
    }
#endif
  }
}

QgsVectorLayerFeatureSource::~QgsVectorLayerFeatureSource()
{
  delete mJoinBuffer;
  delete mExpressionFieldBuffer;
  delete mProviderFeatureSource;
}

QgsFeatureIterator QgsVectorLayerFeatureSource::getFeatures( const QgsFeatureRequest &request )
{
  // return feature iterator that does not own this source
  return QgsFeatureIterator( new QgsVectorLayerFeatureIterator( this, false, request ) );
}


QgsVectorLayerFeatureIterator::QgsVectorLayerFeatureIterator( QgsVectorLayerFeatureSource *source, bool ownSource, const QgsFeatureRequest &request )
  : QgsAbstractFeatureIteratorFromSource<QgsVectorLayerFeatureSource>( source, ownSource, request )
  , mFetchedFid( false )
  , mInterruptionChecker( nullptr )
{
  if ( mRequest.filterType() == QgsFeatureRequest::FilterExpression )
  {
    mRequest.expressionContext()->setFields( mSource->mFields );
    mRequest.filterExpression()->prepare( mRequest.expressionContext() );

    if ( mRequest.flags() & QgsFeatureRequest::SubsetOfAttributes )
    {
      //ensure that all fields required for filter expressions are prepared
      QSet<int> attributeIndexes = mRequest.filterExpression()->referencedAttributeIndexes( mSource->mFields );
      attributeIndexes += mRequest.subsetOfAttributes().toSet();
      mRequest.setSubsetOfAttributes( attributeIndexes.toList() );
    }
  }

  prepareFields();

  mHasVirtualAttributes = !mFetchJoinInfo.isEmpty() || !mExpressionFieldInfo.isEmpty();

  // by default provider's request is the same
  mProviderRequest = mRequest;

  if ( mProviderRequest.flags() & QgsFeatureRequest::SubsetOfAttributes )
  {
    // prepare list of attributes to match provider fields
    QSet<int> providerSubset;
    QgsAttributeList subset = mProviderRequest.subsetOfAttributes();
    int nPendingFields = mSource->mFields.count();
    Q_FOREACH ( int attrIndex, subset )
    {
      if ( attrIndex < 0 || attrIndex >= nPendingFields )
        continue;
      if ( mSource->mFields.fieldOrigin( attrIndex ) == QgsFields::OriginProvider )
        providerSubset << mSource->mFields.fieldOriginIndex( attrIndex );
    }

    // This is done in order to be prepared to do fallback order bys
    // and be sure we have the required columns.
    // TODO:
    // It would be nicer to first check if we can compile the order by
    // and only modify the subset if we cannot.
    if ( !mProviderRequest.orderBy().isEmpty() )
    {
      Q_FOREACH ( const QString &attr, mProviderRequest.orderBy().usedAttributes() )
      {
        providerSubset << mSource->mFields.lookupField( attr );
      }
    }

    mProviderRequest.setSubsetOfAttributes( providerSubset.toList() );
  }

  if ( mProviderRequest.filterType() == QgsFeatureRequest::FilterExpression )
  {
    Q_FOREACH ( const QString &field, mProviderRequest.filterExpression()->referencedColumns() )
    {
      int idx = source->mFields.lookupField( field );

      // If there are fields in the expression which are not of origin provider, the provider will not be able to filter based on them.
      // In this case we disable the expression filter.
      if ( source->mFields.fieldOrigin( idx ) != QgsFields::OriginProvider )
      {
        mProviderRequest.disableFilter();
        // can't limit at provider side
        mProviderRequest.setLimit( -1 );
      }
    }
  }

  if ( mSource->mHasEditBuffer )
  {
    mChangedFeaturesRequest = mProviderRequest;
    QgsFeatureIds changedIds;
    QgsChangedAttributesMap::const_iterator attIt = mSource->mChangedAttributeValues.constBegin();
    for ( ; attIt != mSource->mChangedAttributeValues.constEnd(); ++attIt )
    {
      changedIds << attIt.key();
    }
    mChangedFeaturesRequest.setFilterFids( changedIds );

    if ( mChangedFeaturesRequest.limit() > 0 )
    {
      int providerLimit = mProviderRequest.limit();

      // features may be deleted in buffer, so increase limit sent to provider
      providerLimit += mSource->mDeletedFeatureIds.size();

      if ( mProviderRequest.filterType() == QgsFeatureRequest::FilterExpression )
      {
        // attribute changes may mean some features no longer match expression, so increase limit sent to provider
        providerLimit += mSource->mChangedAttributeValues.size();
      }

      if ( mProviderRequest.filterType() == QgsFeatureRequest::FilterExpression || !mProviderRequest.filterRect().isNull() )
      {
        // geometry changes may mean some features no longer match expression or rect, so increase limit sent to provider
        providerLimit += mSource->mChangedGeometries.size();
      }

      mProviderRequest.setLimit( providerLimit );
    }
  }

  if ( request.filterType() == QgsFeatureRequest::FilterFid )
  {
    mFetchedFid = false;
  }
  else // no filter or filter by rect
  {
    if ( mSource->mHasEditBuffer )
    {
      mChangedFeaturesIterator = mSource->mProviderFeatureSource->getFeatures( mChangedFeaturesRequest );
    }
    else
    {
      mProviderIterator = mSource->mProviderFeatureSource->getFeatures( mProviderRequest );
    }

    rewindEditBuffer();
  }
}


QgsVectorLayerFeatureIterator::~QgsVectorLayerFeatureIterator()
{
  qDeleteAll( mExpressionFieldInfo );

  close();
}



bool QgsVectorLayerFeatureIterator::fetchFeature( QgsFeature &f )
{
  std::cout << "QgsVectorLayerFeatureIterator::fetchFeature 0" << std::endl;
  f.setValid( false );

  if ( mClosed )
    return false;

  if ( mRequest.filterType() == QgsFeatureRequest::FilterFid )
  {
    if ( mFetchedFid )
      return false;
    std::cout << "QgsVectorLayerFeatureIterator::fetchFeature 01" << std::endl;
    bool res = nextFeatureFid( f );
    std::cout << "QgsVectorLayerFeatureIterator::fetchFeature 1 " << f.attribute( 1 ).toString().toStdString() << std::endl;
    mFetchedFid = true;
    return res;
  }

  if ( !mRequest.filterRect().isNull() )
  {
    if ( fetchNextChangedGeomFeature( f ) )
      return true;

    // no more changed geometries
  }

  if ( mRequest.filterType() == QgsFeatureRequest::FilterExpression )
  {
    if ( fetchNextChangedAttributeFeature( f ) )
    {
      std::cout << "QgsVectorLayerFeatureIterator::fetchFeature 2 " << f.attribute( 1 ).toString().toStdString() << std::endl;
      return true;
    }

    // no more changed features
  }

  while ( fetchNextAddedFeature( f ) )
  {
    return true;
  }
  // no more added features

  if ( mProviderIterator.isClosed() )
  {
    mChangedFeaturesIterator.close();
    mProviderIterator = mSource->mProviderFeatureSource->getFeatures( mProviderRequest );
    mProviderIterator.setInterruptionChecker( mInterruptionChecker );
  }

  while ( mProviderIterator.nextFeature( f ) )
  {
    if ( mFetchConsidered.contains( f.id() ) )
      continue;

    // TODO[MD]: just one resize of attributes
    f.setFields( mSource->mFields );

    // update attributes
    if ( mSource->mHasEditBuffer )
      updateChangedAttributes( f );

    if ( mHasVirtualAttributes )
      addVirtualAttributes( f );

    if ( mRequest.filterType() == QgsFeatureRequest::FilterExpression && mProviderRequest.filterType() != QgsFeatureRequest::FilterExpression )
    {
      //filtering by expression, and couldn't do it on the provider side
      mRequest.expressionContext()->setFeature( f );
      if ( !mRequest.filterExpression()->evaluate( mRequest.expressionContext() ).toBool() )
      {
        //feature did not match filter
        continue;
      }
    }

    // update geometry
    // TODO[MK]: FilterRect check after updating the geometry
    if ( !( mRequest.flags() & QgsFeatureRequest::NoGeometry ) )
      updateFeatureGeometry( f );

    return true;
  }
  // no more provider features

  close();
  return false;
}



bool QgsVectorLayerFeatureIterator::rewind()
{
  if ( mClosed )
    return false;

  if ( mRequest.filterType() == QgsFeatureRequest::FilterFid )
  {
    mFetchedFid = false;
  }
  else
  {
    mProviderIterator.rewind();
    rewindEditBuffer();
  }

  return true;
}

bool QgsVectorLayerFeatureIterator::close()
{
  if ( mClosed )
    return false;

  mProviderIterator.close();

  iteratorClosed();

  mClosed = true;
  return true;
}

void QgsVectorLayerFeatureIterator::setInterruptionChecker( QgsInterruptionChecker *interruptionChecker )
{
  mProviderIterator.setInterruptionChecker( interruptionChecker );
  mInterruptionChecker = interruptionChecker;
}

bool QgsVectorLayerFeatureIterator::fetchNextAddedFeature( QgsFeature &f )
{
  while ( mFetchAddedFeaturesIt-- != mSource->mAddedFeatures.constBegin() )
  {
    QgsFeatureId fid = mFetchAddedFeaturesIt->id();

    if ( mFetchConsidered.contains( fid ) )
      // must have changed geometry outside rectangle
      continue;

    if ( !mRequest.acceptFeature( *mFetchAddedFeaturesIt ) )
      // skip features which are not accepted by the filter
      continue;

    useAddedFeature( *mFetchAddedFeaturesIt, f );

    return true;
  }

  mFetchAddedFeaturesIt = mSource->mAddedFeatures.constBegin();
  return false; // no more added features
}


void QgsVectorLayerFeatureIterator::useAddedFeature( const QgsFeature &src, QgsFeature &f )
{
  f.setId( src.id() );
  f.setValid( true );
  f.setFields( mSource->mFields );

  if ( src.hasGeometry() && !( mRequest.flags() & QgsFeatureRequest::NoGeometry ) )
  {
    f.setGeometry( src.geometry() );
  }

  // TODO[MD]: if subset set just some attributes

  f.setAttributes( src.attributes() );

  if ( mHasVirtualAttributes )
    addVirtualAttributes( f );
}



bool QgsVectorLayerFeatureIterator::fetchNextChangedGeomFeature( QgsFeature &f )
{
  // check if changed geometries are in rectangle
  for ( ; mFetchChangedGeomIt != mSource->mChangedGeometries.constEnd(); mFetchChangedGeomIt++ )
  {
    QgsFeatureId fid = mFetchChangedGeomIt.key();

    if ( mFetchConsidered.contains( fid ) )
      // skip deleted features
      continue;

    mFetchConsidered << fid;

    if ( !mFetchChangedGeomIt->intersects( mRequest.filterRect() ) )
      // skip changed geometries not in rectangle and don't check again
      continue;

    useChangedAttributeFeature( fid, *mFetchChangedGeomIt, f );

    // return complete feature
    mFetchChangedGeomIt++;
    return true;
  }

  return false; // no more changed geometries
}

bool QgsVectorLayerFeatureIterator::fetchNextChangedAttributeFeature( QgsFeature &f )
{
  while ( mChangedFeaturesIterator.nextFeature( f ) )
  {
    if ( mFetchConsidered.contains( f.id() ) )
      // skip deleted features and those already handled by the geometry
      continue;

    mFetchConsidered << f.id();

    updateChangedAttributes( f );

    if ( mHasVirtualAttributes )
      addVirtualAttributes( f );

    mRequest.expressionContext()->setFeature( f );
    if ( mRequest.filterExpression()->evaluate( mRequest.expressionContext() ).toBool() )
    {
      return true;
    }
  }

  return false;
}


void QgsVectorLayerFeatureIterator::useChangedAttributeFeature( QgsFeatureId fid, const QgsGeometry &geom, QgsFeature &f )
{
  f.setId( fid );
  f.setValid( true );
  f.setFields( mSource->mFields );

  if ( !( mRequest.flags() & QgsFeatureRequest::NoGeometry ) )
  {
    f.setGeometry( geom );
  }

  bool subsetAttrs = ( mRequest.flags() & QgsFeatureRequest::SubsetOfAttributes );
  if ( !subsetAttrs || !mRequest.subsetOfAttributes().isEmpty() )
  {
    // retrieve attributes from provider
    QgsFeature tmp;
    //mDataProvider->featureAtId( fid, tmp, false, mFetchProvAttributes );
    QgsFeatureRequest request;
    request.setFilterFid( fid ).setFlags( QgsFeatureRequest::NoGeometry );
    if ( subsetAttrs )
    {
      request.setSubsetOfAttributes( mProviderRequest.subsetOfAttributes() );
    }
    QgsFeatureIterator fi = mSource->mProviderFeatureSource->getFeatures( request );
    if ( fi.nextFeature( tmp ) )
    {
      if ( mHasVirtualAttributes || mSource->mHasEditBuffer )
        updateChangedAttributes( tmp );
      f.setAttributes( tmp.attributes() );
    }
  }

  addVirtualAttributes( f );
}



void QgsVectorLayerFeatureIterator::rewindEditBuffer()
{
  mFetchConsidered = mSource->mDeletedFeatureIds;

  mFetchAddedFeaturesIt = mSource->mAddedFeatures.constEnd();
  mFetchChangedGeomIt = mSource->mChangedGeometries.constBegin();
}

void QgsVectorLayerFeatureIterator::prepareJoin( int fieldIdx )
{
  if ( !mSource->mFields.exists( fieldIdx ) )
    return;

  if ( mSource->mFields.fieldOrigin( fieldIdx ) != QgsFields::OriginJoin )
    return;

  int sourceLayerIndex;
  const QgsVectorLayerJoinInfo *joinInfo = mSource->mJoinBuffer->joinForFieldIndex( fieldIdx, mSource->mFields, sourceLayerIndex );
  Q_ASSERT( joinInfo );

  QgsVectorLayer *joinLayer = joinInfo->joinLayer();
  if ( !joinLayer )
    return;  // invalid join (unresolved reference to layer)

  if ( !mFetchJoinInfo.contains( joinInfo ) )
  {
    FetchJoinInfo info;
    info.joinInfo = joinInfo;
    info.joinLayer = joinLayer;
    info.indexOffset = mSource->mJoinBuffer->joinedFieldsOffset( joinInfo, mSource->mFields );
    info.targetField = mSource->mFields.indexFromName( joinInfo->targetFieldName() );
    info.joinField = joinLayer->fields().indexFromName( joinInfo->joinFieldName() );

    // for joined fields, we always need to request the targetField from the provider too
    if ( !mPreparedFields.contains( info.targetField ) && !mFieldsToPrepare.contains( info.targetField ) )
      mFieldsToPrepare << info.targetField;

    if ( mRequest.flags() & QgsFeatureRequest::SubsetOfAttributes && !mRequest.subsetOfAttributes().contains( info.targetField ) )
      mRequest.setSubsetOfAttributes( mRequest.subsetOfAttributes() << info.targetField );

    mFetchJoinInfo.insert( joinInfo, info );
  }

  // store field source index - we'll need it when fetching from provider
  mFetchJoinInfo[ joinInfo ].attributes.push_back( sourceLayerIndex );
}

void QgsVectorLayerFeatureIterator::prepareExpression( int fieldIdx )
{
  const QList<QgsExpressionFieldBuffer::ExpressionField> &exps = mSource->mExpressionFieldBuffer->expressions();

  int oi = mSource->mFields.fieldOriginIndex( fieldIdx );
  QgsExpression *exp = new QgsExpression( exps[oi].cachedExpression );

  QgsDistanceArea da;
  da.setSourceCrs( mSource->mCrsId );
  da.setEllipsoidalMode( true );
  da.setEllipsoid( QgsProject::instance()->ellipsoid() );
  exp->setGeomCalculator( &da );
  exp->setDistanceUnits( QgsProject::instance()->distanceUnits() );
  exp->setAreaUnits( QgsProject::instance()->areaUnits() );

  exp->prepare( mExpressionContext.get() );
  mExpressionFieldInfo.insert( fieldIdx, exp );

  Q_FOREACH ( const QString &col, exp->referencedColumns() )
  {
    int dependentFieldIdx = mSource->mFields.lookupField( col );
    if ( mRequest.flags() & QgsFeatureRequest::SubsetOfAttributes )
    {
      mRequest.setSubsetOfAttributes( mRequest.subsetOfAttributes() << dependentFieldIdx );
    }
    // also need to fetch this dependent field
    if ( !mPreparedFields.contains( dependentFieldIdx ) && !mFieldsToPrepare.contains( dependentFieldIdx ) )
      mFieldsToPrepare << dependentFieldIdx;
  }

  if ( exp->needsGeometry() )
  {
    mRequest.setFlags( mRequest.flags() & ~QgsFeatureRequest::NoGeometry );
  }
}

void QgsVectorLayerFeatureIterator::prepareFields()
{
  mPreparedFields.clear();
  mFieldsToPrepare.clear();
  mFetchJoinInfo.clear();
  mOrderedJoinInfoList.clear();

  mExpressionContext.reset( new QgsExpressionContext() );
  mExpressionContext->appendScope( QgsExpressionContextUtils::globalScope() );
  mExpressionContext->appendScope( QgsExpressionContextUtils::projectScope( QgsProject::instance() ) );
  mExpressionContext->setFields( mSource->mFields );

  mFieldsToPrepare = ( mRequest.flags() & QgsFeatureRequest::SubsetOfAttributes ) ? mRequest.subsetOfAttributes() : mSource->mFields.allAttributesList();

  while ( !mFieldsToPrepare.isEmpty() )
  {
    int fieldIdx = mFieldsToPrepare.takeFirst();
    if ( mPreparedFields.contains( fieldIdx ) )
      continue;

    mPreparedFields << fieldIdx;
    prepareField( fieldIdx );
  }

  //sort joins by dependency
  if ( mFetchJoinInfo.size() > 0 )
  {
    createOrderedJoinList();
  }
}

void QgsVectorLayerFeatureIterator::createOrderedJoinList()
{
  mOrderedJoinInfoList = mFetchJoinInfo.values();
  if ( mOrderedJoinInfoList.size() < 2 )
  {
    return;
  }

  QSet<int> resolvedFields; //todo: get provider / virtual fields without joins

  //add all provider fields without joins as resolved fields
  QList< int >::const_iterator prepFieldIt = mPreparedFields.constBegin();
  for ( ; prepFieldIt != mPreparedFields.constEnd(); ++prepFieldIt )
  {
    if ( mSource->mFields.fieldOrigin( *prepFieldIt ) != QgsFields::OriginJoin )
    {
      resolvedFields.insert( *prepFieldIt );
    }
  }

  //iterate through the joins. If target field is not yet covered, move the entry to the end of the list

  //some join combinations might not have a resolution at all
  int maxIterations = ( mOrderedJoinInfoList.size() + 1 ) * mOrderedJoinInfoList.size() / 2.0;
  int currentIteration = 0;

  for ( int i = 0; i < mOrderedJoinInfoList.size() - 1; ++i )
  {
    if ( !resolvedFields.contains( mOrderedJoinInfoList.at( i ).targetField ) )
    {
      mOrderedJoinInfoList.append( mOrderedJoinInfoList.at( i ) );
      mOrderedJoinInfoList.removeAt( i );
      --i;
    }
    else
    {
      int offset = mOrderedJoinInfoList.at( i ).indexOffset;
      int joinField = mOrderedJoinInfoList.at( i ).joinField;

      QgsAttributeList attributes = mOrderedJoinInfoList.at( i ).attributes;
      QgsAttributeList::const_iterator attIt = attributes.constBegin();
      for ( ; attIt != attributes.constEnd(); ++attIt )
      {
        if ( *attIt != joinField )
        {
          resolvedFields.insert( joinField < *attIt ? *attIt + offset - 1 : *attIt + offset );
        }
      }
    }

    ++currentIteration;
    if ( currentIteration >= maxIterations )
    {
      break;
    }
  }
}

void QgsVectorLayerFeatureIterator::prepareField( int fieldIdx )
{
  switch ( mSource->mFields.fieldOrigin( fieldIdx ) )
  {
    case QgsFields::OriginExpression:
      prepareExpression( fieldIdx );
      break;

    case QgsFields::OriginJoin:
      if ( mSource->mJoinBuffer->containsJoins() )
      {
        prepareJoin( fieldIdx );
      }
      break;

    case QgsFields::OriginUnknown:
    case QgsFields::OriginProvider:
    case QgsFields::OriginEdit:
      break;
  }
}

void QgsVectorLayerFeatureIterator::addJoinedAttributes( QgsFeature &f )
{
  std::cout << "QgsVectorLayerFeatureIterator::addJoinedAttributes 0" << std::endl;
  QList< FetchJoinInfo >::const_iterator joinIt = mOrderedJoinInfoList.constBegin();
  for ( ; joinIt != mOrderedJoinInfoList.constEnd(); ++joinIt )
  {
    std::cout << "QgsVectorLayerFeatureIterator::addJoinedAttributes 1 " << joinIt->joinInfo->joinLayer()->name().toStdString() << std::endl;
    QVariant targetFieldValue = f.attribute( joinIt->targetField );
    if ( joinIt->joinInfo->useTargetFeatureId() )
    {
      std::cout << "QgsVectorLayerFeatureIterator::addJoinedAttributes 2 " << joinIt->joinInfo->joinLayer()->name().toStdString() << std::endl;
      targetFieldValue = f.id();
    }

    std::cout << "QgsVectorLayerFeatureIterator::addJoinedAttributes 3 " << targetFieldValue.toString().toStdString() << std::endl;

    if ( !targetFieldValue.isValid() )
      continue;

    std::cout << "QgsVectorLayerFeatureIterator::addJoinedAttributes 4 " << std::endl;
    const QHash< QString, QgsAttributes> &memoryCache = joinIt->joinInfo->cachedAttributes;
    if ( memoryCache.isEmpty() )
    {
      std::cout << "QgsVectorLayerFeatureIterator::addJoinedAttributes 5 " << std::endl;
      joinIt->addJoinedAttributesDirect( f, targetFieldValue );
    }
    else
    {
      std::cout << "QgsVectorLayerFeatureIterator::addJoinedAttributes 6 " << std::endl;
      joinIt->addJoinedAttributesCached( f, targetFieldValue );
    }
  }
}

void QgsVectorLayerFeatureIterator::addVirtualAttributes( QgsFeature &f )
{
  std::cout << "QgsVectorLayerFeatureIterator::addVirtualAttributes 0" << std::endl;
  // make sure we have space for newly added attributes
  QgsAttributes attr = f.attributes();
  attr.resize( mSource->mFields.count() );  // Provider attrs count + joined attrs count + expression attrs count
  f.setAttributes( attr );

  // possible TODO - handle combinations of expression -> join -> expression -> join?
  // but for now, write that off as too complex and an unlikely rare, unsupported use case

  QList< int > fetchedVirtualAttributes;
  //first, check through joins for any virtual fields we need
  QMap<const QgsVectorLayerJoinInfo *, FetchJoinInfo>::const_iterator joinIt = mFetchJoinInfo.constBegin();
  for ( ; joinIt != mFetchJoinInfo.constEnd(); ++joinIt )
  {
    if ( mExpressionFieldInfo.contains( joinIt->targetField ) )
    {
      // have to calculate expression field before we can handle this join
      addExpressionAttribute( f, joinIt->targetField );
      fetchedVirtualAttributes << joinIt->targetField;
    }
  }

  if ( !mFetchJoinInfo.isEmpty() )
  {
    std::cout << "QgsVectorLayerFeatureIterator::addVirtualAttributes 1" << std::endl;
    addJoinedAttributes( f );
  }

  // add remaining expression fields
  if ( !mExpressionFieldInfo.isEmpty() )
  {
    QMap<int, QgsExpression *>::ConstIterator it = mExpressionFieldInfo.constBegin();
    for ( ; it != mExpressionFieldInfo.constEnd(); ++it )
    {
      if ( fetchedVirtualAttributes.contains( it.key() ) )
        continue;

      addExpressionAttribute( f, it.key() );
    }
  }
}

void QgsVectorLayerFeatureIterator::addExpressionAttribute( QgsFeature &f, int attrIndex )
{
  QgsExpression *exp = mExpressionFieldInfo.value( attrIndex );
  if ( exp )
  {
    mExpressionContext->setFeature( f );
    QVariant val = exp->evaluate( mExpressionContext.get() );
    mSource->mFields.at( attrIndex ).convertCompatible( val );
    f.setAttribute( attrIndex, val );
  }
  else
  {
    f.setAttribute( attrIndex, QVariant() );
  }
}

bool QgsVectorLayerFeatureIterator::prepareSimplification( const QgsSimplifyMethod &simplifyMethod )
{
  Q_UNUSED( simplifyMethod );
  return false;
}

bool QgsVectorLayerFeatureIterator::providerCanSimplify( QgsSimplifyMethod::MethodType methodType ) const
{
  Q_UNUSED( methodType );
  return false;
}


void QgsVectorLayerFeatureIterator::FetchJoinInfo::addJoinedAttributesCached( QgsFeature &f, const QVariant &joinValue ) const
{
  std::cout << "QgsVectorLayerFeatureIterator::addJoinedAttributesCached 0" << std::endl;
  const QHash<QString, QgsAttributes> &memoryCache = joinInfo->cachedAttributes;
  QHash<QString, QgsAttributes>::const_iterator it = memoryCache.find( joinValue.toString() );
  if ( it == memoryCache.constEnd() )
    return; // joined value not found -> leaving the attributes empty (null)

  std::cout << "QgsVectorLayerFeatureIterator::addJoinedAttributesCached 1" << std::endl;
  int index = indexOffset;

  const QgsAttributes &featureAttributes = it.value();
  for ( int i = 0; i < featureAttributes.count(); ++i )
  {
    std::cout << "QgsVectorLayerFeatureIterator::addJoinedAttributesCached 2 for " << i << " is " << featureAttributes.at( i ).toString().toStdString() << std::endl;
    f.setAttribute( index++, featureAttributes.at( i ) );
  }
}



void QgsVectorLayerFeatureIterator::FetchJoinInfo::addJoinedAttributesDirect( QgsFeature &f, const QVariant &joinValue ) const
{
  std::cout << "QgsVectorLayerFeatureIterator::addJoinedAttributesDirect 0" << std::endl;
  // no memory cache, query the joined values by setting substring
  QString subsetString;

  QString joinFieldName = joinInfo->joinFieldName();

  subsetString.append( QStringLiteral( "\"%1\"" ).arg( joinFieldName ) );

  if ( joinValue.isNull() )
  {
    subsetString += QLatin1String( " IS NULL" );
  }
  else
  {
    QString v = joinValue.toString();
    switch ( joinValue.type() )
    {
      case QVariant::Int:
      case QVariant::LongLong:
      case QVariant::Double:
        break;

      default:
      case QVariant::String:
        v.replace( '\'', QLatin1String( "''" ) );
        v.prepend( '\'' ).append( '\'' );
        break;
    }
    subsetString += '=' + v;
  }

  // maybe user requested just a subset of layer's attributes
  // so we do not have to cache everything
  bool hasSubset = joinInfo->joinFieldNamesSubset();
  QVector<int> subsetIndices;
  if ( hasSubset )
    subsetIndices = QgsVectorLayerJoinBuffer::joinSubsetIndices( joinLayer, *joinInfo->joinFieldNamesSubset() );

  // select (no geometry)
  QgsFeatureRequest request;
  request.setFlags( QgsFeatureRequest::NoGeometry );
  request.setSubsetOfAttributes( attributes );
  request.setFilterExpression( subsetString );
  request.setLimit( 1 );
  QgsFeatureIterator fi = joinLayer->getFeatures( request );

  // get first feature
  QgsFeature fet;
  std::cout << "QgsVectorLayerFeatureIterator::addJoinedAttributesDirect 1" << std::endl;
  if ( fi.nextFeature( fet ) )
  {
    std::cout << "QgsVectorLayerFeatureIterator::addJoinedAttributesDirect 2" << std::endl;
    int index = indexOffset;
    QgsAttributes attr = fet.attributes();
    if ( hasSubset )
    {
      std::cout << "QgsVectorLayerFeatureIterator::addJoinedAttributesDirect 3" << std::endl;
      for ( int i = 0; i < subsetIndices.count(); ++i )
        f.setAttribute( index++, attr.at( subsetIndices.at( i ) ) );
    }
    else
    {
      std::cout << "QgsVectorLayerFeatureIterator::addJoinedAttributesDirect 4" << std::endl;
      // use all fields except for the one used for join (has same value as exiting field in target layer)
      for ( int i = 0; i < attr.count(); ++i )
      {
        if ( i == joinField )
          continue;

        std::cout << "QgsVectorLayerFeatureIterator::addJoinedAttributesDirect 5 at " << i << " is " << attr.at( i ).toString().toStdString() << std::endl;
        f.setAttribute( index++, attr.at( i ) );
      }
      std::cout << "QgsVectorLayerFeatureIterator::addJoinedAttributesDirect 6 " << f.attribute( 1 ).toString().toStdString() << std::endl;
    }
  }
  else
  {
    // no suitable join feature found, keeping empty (null) attributes
  }
}




bool QgsVectorLayerFeatureIterator::nextFeatureFid( QgsFeature &f )
{
  std::cout << "QgsVectorLayerFeatureIterator::nextFeatureFid 0" << std::endl;
  QgsFeatureId featureId = mRequest.filterFid();

  // deleted already?
  if ( mSource->mDeletedFeatureIds.contains( featureId ) )
    return false;

  std::cout << "QgsVectorLayerFeatureIterator::nextFeatureFid 1" << std::endl;
  // has changed geometry?
  if ( !( mRequest.flags() & QgsFeatureRequest::NoGeometry ) && mSource->mChangedGeometries.contains( featureId ) )
  {
    std::cout << "QgsVectorLayerFeatureIterator::nextFeatureFid 2" << std::endl;
    useChangedAttributeFeature( featureId, mSource->mChangedGeometries[featureId], f );
    return true;
  }

  // added features
  std::cout << "QgsVectorLayerFeatureIterator::nextFeatureFid 3" << std::endl;
  for ( QgsFeatureMap::ConstIterator iter = mSource->mAddedFeatures.constBegin(); iter != mSource->mAddedFeatures.constEnd(); ++iter )
  {
    if ( iter->id() == featureId )
    {
      std::cout << "QgsVectorLayerFeatureIterator::nextFeatureFid 4" << std::endl;
      useAddedFeature( *iter, f );
      return true;
    }
  }

  // regular features
  std::cout << "QgsVectorLayerFeatureIterator::nextFeatureFid 5" << std::endl;
  QgsFeatureIterator fi = mSource->mProviderFeatureSource->getFeatures( mProviderRequest );
  if ( fi.nextFeature( f ) )
  {
    std::cout << "QgsVectorLayerFeatureIterator::nextFeatureFid 6" << std::endl;
    f.setFields( mSource->mFields );

    if ( mSource->mHasEditBuffer )
    {
      std::cout << "QgsVectorLayerFeatureIterator::nextFeatureFid 7" << std::endl;
      updateChangedAttributes( f );
    }

    if ( mHasVirtualAttributes )
    {
      std::cout << "QgsVectorLayerFeatureIterator::nextFeatureFid 8" << std::endl;
      addVirtualAttributes( f );
    }

    return true;
  }

  std::cout << "QgsVectorLayerFeatureIterator::nextFeatureFid 7" << std::endl;
  return false;
}

void QgsVectorLayerFeatureIterator::updateChangedAttributes( QgsFeature &f )
{
  QgsAttributes attrs = f.attributes();

  // remove all attributes that will disappear - from higher indices to lower
  for ( int idx = mSource->mDeletedAttributeIds.count() - 1; idx >= 0; --idx )
  {
    attrs.remove( mSource->mDeletedAttributeIds[idx] );
  }

  // adjust size to accommodate added attributes
  attrs.resize( attrs.count() + mSource->mAddedAttributes.count() );

  // update changed attributes
  if ( mSource->mChangedAttributeValues.contains( f.id() ) )
  {
    const QgsAttributeMap &map = mSource->mChangedAttributeValues[f.id()];
    for ( QgsAttributeMap::const_iterator it = map.begin(); it != map.end(); ++it )
      attrs[it.key()] = it.value();
  }
  f.setAttributes( attrs );
}

void QgsVectorLayerFeatureIterator::updateFeatureGeometry( QgsFeature &f )
{
  if ( mSource->mChangedGeometries.contains( f.id() ) )
    f.setGeometry( mSource->mChangedGeometries[f.id()] );
}

bool QgsVectorLayerFeatureIterator::prepareOrderBy( const QList<QgsFeatureRequest::OrderByClause> &orderBys )
{
  Q_UNUSED( orderBys );
  return true;
}

