/***************************************************************************
                              qgsmaprendererjobproxy.cpp
                              --------------------------
    begin                : January 2017
    copyright            : (C) 2017 by Paul Blottiere
    email                : paul dot blottiere at oslandia dot com
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmaprendererjobproxy.h"

#include "qgsmessagelog.h"
#include "qgsmaprendererparalleljob.h"
#include "qgsmaprenderercustompainterjob.h"

namespace QgsWms
{

  QgsMapRendererJobProxy::QgsMapRendererJobProxy(
    bool parallelRendering
    , int maxThreads
#ifdef HAVE_SERVER_PYTHON_PLUGINS
    , QgsAccessControl* accessControl
#endif
  )
      :
      mParallelRendering( parallelRendering )
#ifdef HAVE_SERVER_PYTHON_PLUGINS
      , mAccessControl( accessControl )
#endif
  {
    if ( mParallelRendering )
    {
      QgsApplication::setMaxThreads( maxThreads );
      QgsMessageLog::logMessage( QStringLiteral( "Parallel rendering activated with %1 threads" ).arg( maxThreads ), QStringLiteral( "server" ), QgsMessageLog::INFO );
    }
    else
    {
      QgsMessageLog::logMessage( QStringLiteral( "Parallel rendering deactivated" ), QStringLiteral( "server" ), QgsMessageLog::INFO );
    }
  }

  void QgsMapRendererJobProxy::render( const QgsMapSettings& mapSettings, QImage* image )
  {
    if ( mParallelRendering )
    {
      QgsMapRendererParallelJob renderJob( mapSettings );
#ifdef HAVE_SERVER_PYTHON_PLUGINS
      renderJob.setFeatureFilterProvider( mAccessControl );
#endif
      renderJob.start();
      renderJob.waitForFinished();
      *image = renderJob.renderedImage();
      mPainter.reset( new QPainter( image ) );
    }
    else
    {
      mPainter.reset( new QPainter( image ) );
      QgsMapRendererCustomPainterJob renderJob( mapSettings, mPainter.data() );
#ifdef HAVE_SERVER_PYTHON_PLUGINS
      renderJob.setFeatureFilterProvider( mAccessControl );
#endif
      renderJob.renderSynchronously();
    }
  }

  QPainter* QgsMapRendererJobProxy::takePainter()
  {
    return mPainter.take();
  }

} // namespace qgsws
