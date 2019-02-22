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

#include "qgswmsparameters.h"
#include "qgsproject.h"
#include "qgsserverinterface.h"

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

      enum Error
      {
        None,
        Exception,
        MapServiceException,
        BadRequestException,
        SecurityException,
        ServerException
      };

      QgsWmsRenderContext() = default;

      QgsWmsRenderContext( const QgsProject *project, QgsServerInterface *interface );

      void setParameters( const QgsWmsParameters &parameters );

      QgsWmsParameters parameters() const;

      const QgsServerSettings &settings() const;

      const QgsProject *project() const;

      void setFlag( Flag flag, bool on = true );

      bool isValid() const;

      Error error() const;

      QString errorMessage() const;

      QString errorType() const;

      QList<QgsMapLayer *> layers() const;

      QList<QgsMapLayer *> layersToRender() const;

      QDomElement sld( const QgsMapLayer &layer ) const;

      QString style( const QgsMapLayer &layer ) const;

      double scaleDenominator() const;

#ifdef HAVE_SERVER_PYTHON_PLUGINS
      QgsAccessControl *accessControl();
#endif

    private:
      QString layerNickname( const QgsMapLayer &layer ) const;

      void initNicknameLayers();
      void initRestrictedLayers();
      void initLayerGroupsRecursive( const QgsLayerTreeGroup *group, const QString &groupName );

      void searchLayersToRender();
      void searchLayersToRenderSld();
      void searchLayersToRenderStyle();
      void removeUnwantedLayers();

      void checkLayerReadPermissions();

      bool layerScaleVisibility( const QString &name ) const;

      const QgsProject *mProject = nullptr;
      QgsServerInterface *mInterface = nullptr;
      QgsWmsParameters mParameters;
      Flags mFlags = nullptr;
      Error mError = None;
      QString mErrorMessage;
      QString mErrorType;

      // nickname of all layers defined within the project
      QMap<QString, QgsMapLayer *> mNicknameLayers;

      // map of layers to use for rendering
      QMap<QString, QgsMapLayer *> mLayersToRender;

      // list of layers which are not usable
      QStringList mRestrictedLayers;

      QMap<QString, QList<QgsMapLayer *> > mLayerGroups;

      QMap<QString, QDomElement> mSlds;
      QMap<QString, QString> mStyles;
  };
};

#endif
