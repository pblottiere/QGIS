/***************************************************************************
                          qgsvectorlayerjoinbuffer.h
                          ---------------------------
    begin                : Feb 09, 2011
    copyright            : (C) 2011 by Marco Hugentobler
    email                : marco dot hugentobler at sourcepole dot ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSVECTORLAYERJOINBUFFER_H
#define QGSVECTORLAYERJOINBUFFER_H

#include "qgis_core.h"
#include "qgis.h"
#include "qgsvectorlayerjoininfo.h"

#include <QHash>
#include <QString>


typedef QList< QgsVectorLayerJoinInfo > QgsVectorJoinList;


/** \ingroup core
 * Manages joined fields for a vector layer*/
class CORE_EXPORT QgsVectorLayerJoinBuffer : public QObject
{
    Q_OBJECT
  public:
    QgsVectorLayerJoinBuffer( QgsVectorLayer *layer = nullptr );

    /** Joins another vector layer to this layer
      \param joinInfo join object containing join layer id, target and source field
      \returns (since 2.6) whether the join was successfully added */
    bool addJoin( const QgsVectorLayerJoinInfo &joinInfo );

    /** Removes a vector layer join
      \returns true if join was found and successfully removed */
    bool removeJoin( const QString &joinLayerId );

    /** Updates field map with joined attributes
      \param fields map to append joined attributes
     */
    void updateFields( QgsFields &fields );

    //! Calls cacheJoinLayer() for all vector joins
    void createJoinCaches();

    //! Saves mVectorJoins to xml under the layer node
    void writeXml( QDomNode &layer_node, QDomDocument &document ) const;

    //! Reads joins from project file.
    //! Does not resolve layer IDs to layers - call resolveReferences() afterwards
    void readXml( const QDomNode &layer_node );

    //! Resolves layer IDs of joined layers using given project's available layers
    //! \since QGIS 3.0
    void resolveReferences( QgsProject *project );

    //! Quick way to test if there is any join at all
    bool containsJoins() const { return !mVectorJoins.isEmpty(); }

    const QgsVectorJoinList &vectorJoins() const { return mVectorJoins; }

    /** Finds the vector join for a layer field index.
      \param index this layers attribute index
      \param fields fields of the vector layer (including joined fields)
      \param sourceFieldIndex Output: field's index in source layer */
    const QgsVectorLayerJoinInfo *joinForFieldIndex( int index, const QgsFields &fields, int &sourceFieldIndex SIP_OUT ) const;

    //! Find out what is the first index of the join within fields. Returns -1 if join is not present
    //! \since QGIS 2.6
    int joinedFieldsOffset( const QgsVectorLayerJoinInfo *info, const QgsFields &fields );

    //! Return a vector of indices for use in join based on field names from the layer
    //! \since QGIS 2.6
    static QVector<int> joinSubsetIndices( QgsVectorLayer *joinLayer, const QStringList &joinFieldsSubset );

    /** Gets the joined feature corresponding to the feature id given in
     *  parameter.
     * \param info the join information
     * \param fid the feature id
     * \param joinFeature the joined feature
     * \returns true if a valid feature is found in the joined layer, false otherwise
     * \since QGIS 3.0
     */
    bool joinFeature( const QgsVectorLayerJoinInfo &info, QgsFeatureId fid, QgsFeature &joinFeature ) const;

    /** Gets feature corresponding to the joined feature id given in parameter.
     * \param info the join information
     * \param joinFid the joined feature id
     * \param feature the feature
     * \returns true if a valid feature is found, false otherwise
     * \since QGIS 3.0
     */
    bool feature( const QgsVectorLayerJoinInfo &info, QgsFeatureId joinFid, QgsFeature &feature ) const;

    //! Create a copy of the join buffer
    //! \since QGIS 2.6
    QgsVectorLayerJoinBuffer *clone() const SIP_FACTORY;

  signals:
    //! Emitted whenever the list of joined fields changes (e.g. added join or joined layer's fields change)
    //! \since QGIS 2.6
    void joinedFieldsChanged();

  private slots:
    void joinedLayerUpdatedFields();

    void joinedLayerModified();

    void joinedLayerWillBeDeleted();

  private:
    void connectJoinedLayer( QgsVectorLayer *vl );

  private:

    QgsVectorLayer *mLayer = nullptr;

    //! Joined vector layers
    QgsVectorJoinList mVectorJoins;

    //! Caches attributes of join layer in memory if QgsVectorJoinInfo.memoryCache is true (and the cache is not already there)
    void cacheJoinLayer( QgsVectorLayerJoinInfo &joinInfo );

    //! Main mutex to protect most data members that can be modified concurrently
    QMutex mMutex;
};

#endif // QGSVECTORLAYERJOINBUFFER_H
