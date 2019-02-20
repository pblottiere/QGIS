/***************************************************************************
                              qgswmsgetlegendgraphics.cpp
                              -------------------------
  begin                : December 20 , 2016
  copyright            : (C) 2007 by Marco Hugentobler  (original code)
                         (C) 2014 by Alessandro Pasotti (original code)
                         (C) 2016 by David Marteau
  email                : marco dot hugentobler at karto dot baug dot ethz dot ch
                         a dot pasotti at itopen dot it
                         david dot marteau at 3liz dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgswmsutils.h"
#include "qgswmsgetlegendgraphics.h"
#include "qgswmsrenderer.h"
#include "qgslayertree.h"
#include "qgsmaplayerlegend.h"
#include "qgssymbollayerutils.h"
#include "qgslegendrenderer.h"
#include "qgslayertreemodel.h"
#include "qgsvectorlayer.h"
#include "qgsvectorlayerfeaturecounter.h"

#include <QImage>

namespace QgsWms
{

  void writeGetLegendGraphics( QgsServerInterface *serverIface, const QgsProject *project,
                               const QString &version, const QgsServerRequest &request,
                               QgsServerResponse &response )
  {
    Q_UNUSED( version );

    QgsServerRequest::Parameters params = request.parameters();
    QString format = params.value( QStringLiteral( "FORMAT" ), QStringLiteral( "PNG" ) );

    QgsWmsParameters wmsParameters( QUrlQuery( request.url() ) );

    // Get cached image
    /*QgsAccessControl *accessControl = nullptr;
    QgsServerCacheManager *cacheManager = nullptr;
#ifdef HAVE_SERVER_PYTHON_PLUGINS
    accessControl = serverIface->accessControls();
    cacheManager = serverIface->cacheManager();
#endif
    if ( cacheManager )
    {
      ImageOutputFormat outputFormat = parseImageFormat( format );
      QString saveFormat;
      QString contentType;
      switch ( outputFormat )
      {
        case PNG:
        case PNG8:
        case PNG16:
        case PNG1:
          contentType = "image/png";
          saveFormat = "PNG";
          break;
        case JPEG:
          contentType = "image/jpeg";
          saveFormat = "JPEG";
          break;
        default:
          throw QgsServiceException( "InvalidFormat",
                                     QString( "Output format '%1' is not supported in the GetLegendGraphic request" ).arg( format ) );
          break;
      }

      QImage image;
      QByteArray content = cacheManager->getCachedImage( project, request, accessControl );
      if ( !content.isEmpty() && image.loadFromData( content ) )
      {
        response.setHeader( QStringLiteral( "Content-Type" ), contentType );
        image.save( response.io(), qPrintable( saveFormat ) );
        return;
      }
    }*/

    //QgsRenderer renderer( serverIface, project, wmsParameters );

    // check parameters
    if ( wmsParameters.allLayersNickname().isEmpty() )
      throw QgsBadRequestException( QStringLiteral( "LayerNotSpecified" ),
                                    QStringLiteral( "LAYER is mandatory for GetLegendGraphic operation" ) );

    if ( wmsParameters.format() == QgsWmsParameters::Format::NONE )
      throw QgsBadRequestException( QStringLiteral( "FormatNotSpecified" ),
                                    QStringLiteral( "FORMAT is mandatory for GetLegendGraphic operation" ) );

    QgsRenderer renderer( serverIface, project );
    renderer.setParameters( wmsParameters, true );

    QgsLayerTree root;
    std::unique_ptr<QgsLayerTreeModel> model( legendTreeModel( renderer, root ) );

    if ( !wmsParameters.rule().isEmpty() )
    {
      QgsLayerTreeModelLegendNode *node = legendNode( model.get(),
                                                      wmsParameters.rule() );
      if ( node )
        renderer.run( *node );
    } else
    {
      renderer.run( *model );
    }

    root.clear();

    if ( ! renderer.image() )
    {
      throw QgsServiceException( QStringLiteral( "UnknownError" ),
                                 QStringLiteral( "Failed to compute GetLegendGraphics image" ) );
    }
    else
    {
      writeImage( response, *renderer.image(),  format, renderer.getImageQuality() );
    }
  }

  QgsLayerTreeModel *legendTreeModel( const QgsWms::QgsRenderer &renderer, QgsLayerTree &rootGroup )
  {
    // get params
    const QgsWmsParameters parameters = renderer.parameters();
    bool showFeatureCount = parameters.showFeatureCountAsBool();
    bool drawLegendLayerLabel = parameters.layerTitleAsBool();
    bool drawLegendItemLabel = parameters.ruleLabelAsBool();

    double scaleDenominator = -1;
    if ( ! parameters.scale().isEmpty() )
      scaleDenominator = parameters.scaleAsDouble();

    bool ruleDefined = false;
    if ( !parameters.rule().isEmpty() )
      ruleDefined = true;

    bool contentBasedLegend = false;
    QgsRectangle contentBasedLegendExtent;
    if ( ! parameters.bbox().isEmpty() )
    {
      contentBasedLegend = true;
      contentBasedLegendExtent = parameters.bboxAsRectangle();
      if ( contentBasedLegendExtent.isEmpty() )
        throw QgsBadRequestException( QStringLiteral( "InvalidParameterValue" ),
                                      QStringLiteral( "Invalid BBOX parameter" ) );

      if ( !parameters.rule().isEmpty() )
        throw QgsBadRequestException( QStringLiteral( "InvalidParameterValue" ),
                                      QStringLiteral( "BBOX parameter cannot be combined with RULE" ) );
    }

    // build layer tree
    rootGroup.clear();
    QList<QgsVectorLayerFeatureCounter *> counters;
    for ( QgsMapLayer *ml : renderer.mapSettings().layers() )
    {
      QgsLayerTreeLayer *lt = rootGroup.addLayer( ml );
      lt->setCustomProperty( QStringLiteral( "showFeatureCount" ), showFeatureCount );

      if ( !ml->title().isEmpty() )
        lt->setName( ml->title() );

      if ( ml->type() != QgsMapLayer::VectorLayer || !showFeatureCount )
        continue;

      QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( ml );
      QgsVectorLayerFeatureCounter *counter = vl->countSymbolFeatures();
      if ( !counter )
        continue;
      counters.append( counter );
    }

    // build legend model
    QgsLayerTreeModel *legendModel = new QgsLayerTreeModel( &rootGroup );
    if ( scaleDenominator > 0 )
      legendModel->setLegendFilterByScale( scaleDenominator );

    // QgsMapSettings contentBasedMapSettings;
    if ( contentBasedLegend )
    {
      for ( QgsLayerTreeNode *node : rootGroup.children() )
      {
        Q_ASSERT( QgsLayerTree::isLayer( node ) );
        QgsLayerTreeLayer *nodeLayer = QgsLayerTree::toLayer( node );

        QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( nodeLayer->layer() );
        if ( !vl || !vl->renderer() )
          continue;

        QList<int> order = legendNodeOrder( renderer, vl );

        // either remove the whole layer or just filter out some items
        if ( order.isEmpty() )
          rootGroup.removeChildNode( nodeLayer );
        else
        {
          QgsMapLayerLegendUtils::setLegendNodeOrder( nodeLayer, order );
          legendModel->refreshLayerLegend( nodeLayer );
        }
      }
    }

    // if legend is not based on rendering rules
    if ( ! ruleDefined )
    {
      QList<QgsLayerTreeNode *> rootChildren = rootGroup.children();
      for ( QgsLayerTreeNode *node : rootChildren )
      {
        if ( QgsLayerTree::isLayer( node ) )
        {
          QgsLayerTreeLayer *nodeLayer = QgsLayerTree::toLayer( node );

          // layer titles - hidden or not
          QgsLegendRenderer::setNodeLegendStyle( nodeLayer, drawLegendLayerLabel ? QgsLegendStyle::Subgroup : QgsLegendStyle::Hidden );

          // rule item titles
          if ( !drawLegendItemLabel )
          {
            for ( QgsLayerTreeModelLegendNode *legendNode : legendModel->layerLegendNodes( nodeLayer ) )
            {
              legendNode->setUserLabel( QStringLiteral( " " ) ); // empty string = no override, so let's use one space
            }
          }
          else if ( !drawLegendLayerLabel )
          {
            for ( QgsLayerTreeModelLegendNode *legendNode : legendModel->layerLegendNodes( nodeLayer ) )
            {
              if ( legendNode->isEmbeddedInParent() )
                legendNode->setEmbeddedInParent( false );
            }
          }
        }
      }
    }

    for ( QgsVectorLayerFeatureCounter *c : counters )
    {
      c->waitForFinished();
    }

    return legendModel;
  }

  QList<int> legendNodeOrder( const QgsWms::QgsRenderer &renderer, const QgsVectorLayer *layer )
  {
    const QSet<QString> symbols = renderer.symbols(*layer);
    QList<int> order;
    int i = 0;
    for ( const QgsLegendSymbolItem &legendItem : layer->renderer()->legendSymbolItems() )
    {
      QString sProp = QgsSymbolLayerUtils::symbolProperties( legendItem.legacyRuleKey() );
      if ( symbols.contains( sProp ) )
        order.append( i );
      ++i;
    }

    return order;
  }

  QgsLayerTreeModelLegendNode *legendNode( QgsLayerTreeModel *legendModel, const QString &rule )
  {
    for ( QgsLayerTreeLayer *nodeLayer : legendModel->rootGroup()->findLayers() )
    {
      for ( QgsLayerTreeModelLegendNode *legendNode : legendModel->layerLegendNodes( nodeLayer ) )
      {
        if ( legendNode->data( Qt::DisplayRole ).toString() == rule )
          return legendNode;
      }
    }
    return nullptr;
  }
} // namespace QgsWms
