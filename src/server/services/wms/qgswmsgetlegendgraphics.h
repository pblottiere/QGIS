/***************************************************************************
                              qgswmsgetlegendgraphics.h
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

#include "qgslayertreemodellegendnode.h"

class QgsLayerTreeModel;
class QgsLayerTree;
class QgsRenderContext;

namespace QgsWms
{
  class QgsRenderer;

  typedef QSet<QString> SymbolSet;
  typedef QHash<const QgsVectorLayer *, SymbolSet> HitTest;

  /**
   * Output GetLegendGRaphics response
   */
  void writeGetLegendGraphics( QgsServerInterface *serverIface, const QgsProject *project,
                               const QString &version, const QgsServerRequest &request,
                               QgsServerResponse &response );

  QgsLayerTreeModel *legendTreeModel( const QgsWms::QgsRenderer &renderer, QgsLayerTree &rootGroup );

  QList<int> legendNodeOrder( const QgsWms::QgsRenderer &renderer, const QgsVectorLayer *layer );

  QgsLayerTreeModelLegendNode *legendNode( QgsLayerTreeModel *legendModel, const QString &rule );
} // namespace QgsWms




