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

namespace QgsWms
{

  void writeGetFeatureInfo( QgsServerInterface *serverIface,
                            const QgsProject *project, const QString &version,
                            const QgsServerRequest &request,
                            QgsServerResponse &response )
  {
    QgsServerRequest::Parameters params = request.parameters();

    // check mandatory parameters
    const QgsWmsParameters wmsParameters( QUrlQuery( request.url() ) );
    checkMandatoryParameters( wmsParameters );

    // init context
    QgsWmsRenderContext context( project, serverIface );
    context.setFlag( QgsWmsRenderContext::UseFilter );
    context.setFlag( QgsWmsRenderContext::SetAccessControl );
    context.setFlag( QgsWmsRenderContext::AddQueryLayers );
    context.setParameters( wmsParameters );

    // build document
    QgsRenderer renderer( context );

    QString infoFormat = params.value( QStringLiteral( "INFO_FORMAT" ), QStringLiteral( "text/plain" ) );

    response.setHeader( QStringLiteral( "Content-Type" ), infoFormat + QStringLiteral( "; charset=utf-8" ) );
    response.write( renderer.getFeatureInfo( version ) );
  }

  void checkMandatoryParameters( const QgsWmsParameters &parameters )
  {
    const QStringList queryLayers = parameters.queryLayersNickname();
    if ( queryLayers.isEmpty() )
    {
      const QString msg = QObject::tr( "QUERY_LAYERS parameter is required" );
      throw QgsBadRequestException( QStringLiteral( "LayerNotQueryable" ), msg );
    }

    const bool ijDefined = !parameters.i().isEmpty() && !parameters.j().isEmpty();
    const bool xyDefined = !parameters.x().isEmpty() && !parameters.y().isEmpty();
    const bool filtersDefined = !parameters.filters().isEmpty();
    const bool filterGeomDefined = !parameters.filterGeom().isEmpty();

    if ( !ijDefined && !xyDefined && !filtersDefined && !filterGeomDefined )
    {
      throw QgsBadRequestException( QStringLiteral( "ParameterMissing" ),
                                    QStringLiteral( "I/J parameters are required" ) );
    }

    const QgsWmsParameters::Format infoFormat = parameters.infoFormat();
    if ( infoFormat == QgsWmsParameters::Format::NONE )
    {
      throw QgsBadRequestException( QStringLiteral( "InvalidFormat" ),
                                    QStringLiteral( "Invalid INFO_FORMAT parameter" ) );
    }
  }
} // namespace QgsWms




