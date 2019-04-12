/***************************************************************************
                              qgswmsgetfeatureinfo.cpp
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
#include "qgswmsgetfeatureinfo.h"
#include "qgswmsrenderer.h"
#include "qgswmsserviceexception.h"

namespace QgsWms
{
  void writeGetFeatureInfo( QgsServerInterface *serverIface, const QgsProject *project,
                            const QString &version, const QgsServerRequest &request,
                            QgsServerResponse &response )
  {
    // get wms parameters from query
    QgsWmsParameters parameters( QUrlQuery( request.url() ) );

    // check parameters validity
    QgsWmsGetFeatureInfo::checkParameters( parameters );

    // if FILTER is defined, the CRS parameter is not mandatory and a
    // default one may be used for the rendering
    QgsWmsGetFeatureInfo::updateCrs( parameters );

    // if WIDTH, HEIGHT and INFO_FORMAT are not defined, a default size is
    // still necessary
    QgsWmsGetFeatureInfo::updateSize( parameters );

    // prepare render context
    QgsWmsRenderContext context( project, serverIface );
    context.setFlag( QgsWmsRenderContext::AddQueryLayers );
    context.setFlag( QgsWmsRenderContext::UseFilter );
    context.setFlag( QgsWmsRenderContext::UseScaleDenominator );
    context.setFlag( QgsWmsRenderContext::SetAccessControl );
    context.setParameters( parameters );

    const QString infoFormat = request.parameters().value( QStringLiteral( "INFO_FORMAT" ), QStringLiteral( "text/plain" ) );
    response.setHeader( QStringLiteral( "Content-Type" ), infoFormat + QStringLiteral( "; charset=utf-8" ) );

    QgsRenderer renderer( context );
    response.write( renderer.getFeatureInfo( version ) );
  }

  namespace QgsWmsGetFeatureInfo
  {
    void checkParameters( const QgsWmsParameters &parameters )
    {
      if ( parameters.queryLayersNickname().isEmpty() )
      {
        throw QgsBadRequestException( QgsServiceException::QGIS_MissingParameterValue,
                                      parameters[QgsWmsParameter::QUERY_LAYERS] );
      }

      // The I/J parameters are mandatory if they are not replaced by X/Y or
      // FILTER or FILTER_GEOM
      const bool ij = !parameters.i().isEmpty() && !parameters.j().isEmpty();
      const bool xy = !parameters.x().isEmpty() && !parameters.y().isEmpty();
      const bool filters = !parameters.filters().isEmpty();
      const bool filterGeom = !parameters.filterGeom().isEmpty();

      if ( !ij && !xy && !filters && !filterGeom )
      {
        QgsWmsParameter parameter = parameters[QgsWmsParameter::I];

        if ( parameters.j().isEmpty() )
          parameter = parameters[QgsWmsParameter::J];

        throw QgsBadRequestException( QgsServiceException::QGIS_MissingParameterValue, parameter );
      }

      const QgsWmsParameters::Format infoFormat = parameters.infoFormat();
      if ( infoFormat == QgsWmsParameters::Format::NONE )
      {
        throw QgsBadRequestException( QgsServiceException::OGC_InvalidFormat,
                                      parameters[QgsWmsParameter::INFO_FORMAT] );
      }
    }

    void updateCrs( QgsWmsParameters &parameters )
    {
      const bool ij = !parameters.i().isEmpty() && !parameters.j().isEmpty();
      const bool xy = !parameters.x().isEmpty() && !parameters.y().isEmpty();
      const bool filters = !parameters.filters().isEmpty();

      if ( filters && !ij && !xy && parameters.crs().isEmpty() )
      {
        parameters.set( QgsWmsParameter::CRS, QStringLiteral( "EPSG:4326" ) );
      }
    }

    void updateSize( QgsWmsParameters &parameters )
    {
      const int width = parameters.widthAsInt();
      const int height = parameters.heightAsInt();

      if ( !( width && height ) &&  ! parameters.infoFormatIsImage() )
      {
        parameters.set( QgsWmsParameter::WIDTH, 10 );
        parameters.set( QgsWmsParameter::HEIGHT, 10 );
      }
    }
  }
} // namespace QgsWms
