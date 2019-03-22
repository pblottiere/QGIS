/***************************************************************************
                              qgswmsgetmap.cpp
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
#include "qgsserverprojectutils.h"
#include "qgswmsutils.h"
#include "qgswmsgetmap.h"
#include "qgswmsrenderer.h"

#include <QImage>

namespace QgsWms
{
  void writeGetMap( QgsServerInterface *serverIface, const QgsProject *project,
                    const QString &, const QgsServerRequest &request,
                    QgsServerResponse &response )
  {
    // get wms parameters from query
    const QgsWmsParameters parameters( QUrlQuery( request.url() ) );

    // prepare render context
    QgsWmsRenderContext context( project, serverIface );
    context.setFlag( QgsWmsRenderContext::UpdateExtent );
    context.setFlag( QgsWmsRenderContext::UseOpacity );
    context.setFlag( QgsWmsRenderContext::UseFilter );
    context.setFlag( QgsWmsRenderContext::UseSelection );
    context.setFlag( QgsWmsRenderContext::UseOpacity );
    context.setFlag( QgsWmsRenderContext::AddHighlightLayers );
    context.setFlag( QgsWmsRenderContext::AddExternalLayers );
    context.setFlag( QgsWmsRenderContext::SetAccessControl );
    context.setParameters( parameters );

    // check parameters
    if ( !checkMaximumWidthHeight( context ) )
    {
      throw QgsBadRequestException( QStringLiteral( "Size error" ),
                                    QStringLiteral( "The requested map size is too large" ) );
    }

    // rendering
    QgsRenderer renderer( context );
    std::unique_ptr<QImage> result( renderer.getMap() );

    // return result
    if ( result )
    {
      const QString format = request.parameters().value( QStringLiteral( "FORMAT" ), QStringLiteral( "PNG" ) );
      writeImage( response, *result, format, renderer.imageQuality() );
    }
    else
    {
      throw QgsServiceException( QStringLiteral( "UnknownError" ),
                                 QStringLiteral( "Failed to compute GetMap image" ) );
    }
  }

  bool checkMaximumWidthHeight( const QgsWmsRenderContext &context )
  {
    const QgsProject &project = *context.project();

    //test if maxWidth / maxHeight set and WIDTH / HEIGHT parameter is in the range
    int wmsMaxWidth = QgsServerProjectUtils::wmsMaxWidth( project );
    int width = context.parameters().widthAsInt();

    if ( wmsMaxWidth != -1 && width > wmsMaxWidth )
    {
      return false;
    }

    int wmsMaxHeight = QgsServerProjectUtils::wmsMaxHeight( project );
    int height = context.parameters().heightAsInt();
    if ( wmsMaxHeight != -1 && height > wmsMaxHeight )
    {
      return false;
    }

    // Sanity check from internal QImage checks (see qimage.cpp)
    // this is to report a meaningful error message in case of
    // image creation failure and to differentiate it from out
    // of memory conditions.

    // depth for now it cannot be anything other than 32, but I don't like
    // to hardcode it: I hope we will support other depths in the future.
    uint depth = 32;
    switch ( context.parameters().format() )
    {
      case QgsWmsParameters::Format::JPG:
      case QgsWmsParameters::Format::PNG:
      default:
        depth = 32;
    }

    const int bytes_per_line = ( ( width * depth + 31 ) >> 5 ) << 2; // bytes per scanline (must be multiple of 4)

    if ( std::numeric_limits<int>::max() / depth < static_cast<uint>( width )
         || bytes_per_line <= 0
         || height <= 0
         || std::numeric_limits<int>::max() / static_cast<uint>( bytes_per_line ) < static_cast<uint>( height )
         || std::numeric_limits<int>::max() / sizeof( uchar * ) < static_cast<uint>( height ) )
      return false;

    return true;
  }
} // namespace QgsWms
