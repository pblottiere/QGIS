/***************************************************************************
                              qgswmsrendercontext.cpp
                              ---------------------
  begin                : February 22, 2019
  copyright            : (C) 2019 by Paul Blottiere
  email                : paul.blottiere@oslandia.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qgswmsrendercontext.h>

using namespace QgsWms;

QgsWmsRenderContext::QgsWmsRenderContext( const QgsProject *project, QgsServerInterface *interface )
  : mProject( project )
  , mInterface( interface )
{
}

void QgsWmsRenderContext::setParameters( const QgsWmsParameters &parameters )
{
  mParameters = parameters;
}

QgsWmsParameters QgsWmsRenderContext::parameters() const
{
  return mParameters;
}

const QgsServerSettings &QgsWmsRenderContext::settings() const
{
  return *mInterface->serverSettings();
}

const QgsProject *QgsWmsRenderContext::project() const
{
  return mProject;
}

#ifdef HAVE_SERVER_PYTHON_PLUGINS
QgsAccessControl *QgsWmsRenderContext::accessControl()
{
  return mInterface->accessControls();
}
#endif

