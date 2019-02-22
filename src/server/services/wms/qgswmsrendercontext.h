/***************************************************************************
                              qgswmsrendercontext.h
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

#ifndef QGSWMSRENDERCONTEXT_H
#define QGSWMSRENDERCONTEXT_H

#include <qgswmsparameters.h>
#include <qgsproject.h>
#include <qgsserverinterface.h>

namespace QgsWms
{
  class QgsWmsRenderContext
  {
    public:
      enum Flag
      {
        UseScaleDenominator    = 0x01,
      };
      Q_DECLARE_FLAGS( Flags, Flag )

      QgsWmsRenderContext() = default;

      QgsWmsRenderContext( const QgsProject *project, QgsServerInterface *interface );

      void setParameters( const QgsWmsParameters &parameters );

      QgsWmsParameters parameters() const;

      const QgsServerSettings &settings() const;

      const QgsProject *project() const;

      // void setInterface( QgsServerInterface *interface );

      // void setFlag( Flag flag, bool on = true );

      // bool testFlag( Flag flag ) const;

#ifdef HAVE_SERVER_PYTHON_PLUGINS
      QgsAccessControl *accessControl();
#endif

    private:
      const QgsProject *mProject;
      QgsServerInterface *mInterface;
      QgsWmsParameters mParameters;
  };
};

#endif
