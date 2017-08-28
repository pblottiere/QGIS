/***************************************************************************
  qgslabelinggui.h
  Smart labeling for vector layers
  -------------------
         begin                : June 2009
         copyright            : (C) Martin Dobias
         email                : wonder dot sk at gmail dot com

 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSLABELINGGUI_H
#define QGSLABELINGGUI_H

#include "qgspallabeling.h"
#include "qgstextformatwidget.h"
#include "qgspropertyoverridebutton.h"
#include "qgis_app.h"

class APP_EXPORT QgsLabelingGui : public QgsTextFormatWidget, private QgsExpressionContextGenerator
{
    Q_OBJECT

  public:
    QgsLabelingGui( QgsVectorLayer *layer, QgsMapCanvas *mapCanvas, const QgsPalLayerSettings &settings, QWidget *parent );

    QgsPalLayerSettings layerSettings();

    enum LabelMode
    {
      NoLabels,
      Labels,
      ObstaclesOnly,
    };

    void setLabelMode( LabelMode mode );

    void setLayer( QgsMapLayer *layer );

  signals:

    void autocreated();

  public slots:

    void updateUi();

    void autocreateProperty();

  protected:
    void blockInitSignals( bool block );
    void syncDefinedCheckboxFrame( QgsPropertyOverrideButton *ddBtn, QCheckBox *chkBx, QFrame *f );

  private:
    QgsVectorLayer *mLayer = nullptr;
    const QgsPalLayerSettings &mSettings;
    QgsPropertyCollection mDataDefinedProperties;
    LabelMode mMode;

    QgsExpressionContext createExpressionContext() const override;

    void populateDataDefinedButtons();
    void registerDataDefinedButton( QgsPropertyOverrideButton *button, QgsPalLayerSettings::Property key );

  private slots:

    void updateProperty();

};

#endif // QGSLABELINGGUI_H


