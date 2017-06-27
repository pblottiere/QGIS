#ifndef QGSVECTORLAYERJOININFO_H
#define QGSVECTORLAYERJOININFO_H

#include <QHash>
#include <QString>
#include <QStringList>

#include "qgsfeature.h"

#include "qgsvectorlayerref.h"

/**
 * \ingroup core
 * Defines left outer join from our vector layer to some other vector layer.
 * The join is done based on [our layer].targetField = [join layer].joinField
 *
 * \since QGIS 3.0
 */
class CORE_EXPORT QgsVectorLayerJoinInfo
{
  public:
    QgsVectorLayerJoinInfo()
      : mMemoryCache( false )
      , cacheDirty( true )
      , mEditable( false )
    {}

    //! Sets weak reference to the joined layer
    void setJoinLayer( QgsVectorLayer *layer ) { mJoinLayerRef = QgsVectorLayerRef( layer ); }
    //! Returns joined layer (may be null if the reference was set by layer ID and not resolved yet)
    QgsVectorLayer *joinLayer() const { return mJoinLayerRef.get(); }

    //! Sets ID of the joined layer. It will need to be overwritten by setJoinLayer() to a reference to real layer
    void setJoinLayerId( const QString &layerId ) { mJoinLayerRef = QgsVectorLayerRef( layerId ); }
    //! ID of the joined layer - may be used to resolve reference to the joined layer
    QString joinLayerId() const { return mJoinLayerRef.layerId; }

    //! Sets name of the field of our layer that will be used for join
    void setTargetFieldName( const QString &fieldName ) { mTargetFieldName = fieldName; }
    //! Returns name of the field of our layer that will be used for join
    QString targetFieldName() const { return mTargetFieldName; }

    //! Sets name of the field of joined layer that will be used for join
    void setJoinFieldName( const QString &fieldName ) { mJoinFieldName = fieldName; }
    //! Returns name of the field of joined layer that will be used for join
    QString joinFieldName() const { return mJoinFieldName; }

    //! Sets prefix of fields from the joined layer. If null, joined layer's name will be used.
    void setPrefix( const QString &prefix ) { mPrefix = prefix; }
    //! Returns prefix of fields from the joined layer. If null, joined layer's name will be used.
    QString prefix() const { return mPrefix; }

    //! Sets whether values from the joined layer should be cached in memory to speed up lookups
    void setUsingMemoryCache( bool enabled ) { mMemoryCache = enabled; }
    //! Returns whether values from the joined layer should be cached in memory to speed up lookups
    bool isUsingMemoryCache() const { return mMemoryCache; }

    /** Returns the prefixed name of the field as known by the target layer.
     * \param field the field
     * \returns the prefixed name of the field
     * \since QGIS 3.0
     */
    QString prefixedNameField( const QgsField &field ) const;

    /** Sets whether values from the joined layer may be editing from the
     *  target layer.
     * \since QGIS 3.0
     */
    void setEditable( bool editable ) { mEditable = editable; }

    /** Returns whether values from the joined layer may be editing from the
     *  target layer.
     * \since QGIS 3.0
     */
    bool isEditable() const { return mEditable; }

    bool operator==( const QgsVectorLayerJoinInfo &other ) const
    {
      return mTargetFieldName == other.mTargetFieldName &&
             mJoinLayerRef.layerId == other.mJoinLayerRef.layerId &&
             mJoinFieldName == other.mJoinFieldName &&
             mJoinFieldsSubset == other.mJoinFieldsSubset &&
             mMemoryCache == other.mMemoryCache &&
             mPrefix == other.mPrefix;
    }

    /** Set subset of fields to be used from joined layer. Takes ownership of the passed pointer. Null pointer tells to use all fields.
      \since QGIS 2.6 */
    void setJoinFieldNamesSubset( QStringList *fieldNamesSubset SIP_TRANSFER ) { mJoinFieldsSubset = std::shared_ptr<QStringList>( fieldNamesSubset ); }

    /** Get subset of fields to be used from joined layer. All fields will be used if null is returned.
      \since QGIS 2.6 */
    QStringList *joinFieldNamesSubset() const { return mJoinFieldsSubset.get(); }

  protected:
    //! Join field in the target layer
    QString mTargetFieldName;
    //! Weak reference to the joined layer
    QgsVectorLayerRef mJoinLayerRef;
    //! Join field in the source layer
    QString mJoinFieldName;

    /** An optional prefix. If it is a Null string "{layername}_" will be used
     * \since QGIS 2.8
     */
    QString mPrefix;

    //! True if the join is cached in virtual memory
    bool mMemoryCache;

    //! Subset of fields to use from joined layer. null = use all fields
    std::shared_ptr<QStringList> mJoinFieldsSubset;

    // caching support

    friend class QgsVectorLayerJoinBuffer;
    friend class QgsVectorLayerFeatureIterator;

    //! True if the cached join attributes need to be updated
    bool cacheDirty;

    bool mEditable;

    //! Cache for joined attributes to provide fast lookup (size is 0 if no memory caching)
    QHash< QString, QgsAttributes> cachedAttributes;

};


#endif // QGSVECTORLAYERJOININFO_H
