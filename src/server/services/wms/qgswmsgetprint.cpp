/***************************************************************************
                              qgswmsgetprint.h
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
#include "qgswmsgetprint.h"
#include "qgswmsrenderer.h"

namespace QgsWms
{
  void writeGetPrint( QgsServerInterface *serverIface,
                      const QgsProject *project, const QString &,
                      const QgsServerRequest &request,
                      QgsServerResponse &response )
  {
    QgsServerRequest::Parameters params = request.parameters();

    // init context
    const QgsWmsParameters wmsParameters( QUrlQuery( request.url() ) );

    QgsWmsRenderContext context( project, serverIface );
    context.setFlag( QgsWmsRenderContext::UpdateExtent );
    context.setFlag( QgsWmsRenderContext::UseOpacity );
    context.setFlag( QgsWmsRenderContext::UseFilter );
    context.setFlag( QgsWmsRenderContext::UseSelection );
    context.setFlag( QgsWmsRenderContext::SetAccessControl );
    context.setFlag( QgsWmsRenderContext::AddHighlightLayers );
    context.setParameters( wmsParameters );

    // check mandatory parameters
    if ( wmsParameters.composerTemplate().isEmpty() )
    {
      throw QgsBadRequestException( QStringLiteral( "ParameterMissing" ),
                                    QStringLiteral( "The TEMPLATE parameter is required" ) );
    }

    // rendering
    QgsRenderer renderer( context );

    const QgsWmsParameters::Format format = wmsParameters.format();
    QString contentType;

    // GetPrint supports svg/png/pdf
    if ( format == QgsWmsParameters::PNG )
    {
      contentType = "image/png";
    }
    else if ( format == QgsWmsParameters::SVG )
    {
      contentType = "image/svg+xml";
    }
    else if ( format == QgsWmsParameters::PDF )
    {
      contentType = "application/pdf";
    }
    else
    {
      throw QgsServiceException( QStringLiteral( "InvalidFormat" ),
                                 QString( "Output format %1 is not supported by the GetPrint request" ).arg( wmsParameters.formatAsString() ) );
    }

    response.setHeader( QStringLiteral( "Content-Type" ), contentType );
    response.write( renderer.getPrint() );
  }

} // namespace QgsWms
