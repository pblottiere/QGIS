/***************************************************************************
    qgslabelingwidget.cpp
    ---------------------
    begin                : September 2015
    copyright            : (C) 2015 by Martin Dobias
    email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QDialogButtonBox>
#include <QDomElement>

#include "qgslabelingwidget.h"

#include "qgslabelengineconfigdialog.h"
#include "qgslabelinggui.h"
#include "qgsreadwritecontext.h"
#include "qgsrulebasedlabelingwidget.h"
#include "qgsvectorlayer.h"
#include "qgsvectorlayerlabeling.h"
#include "qgisapp.h"

QgsLabelingWidget::QgsLabelingWidget( QgsVectorLayer *layer, QgsMapCanvas *canvas, QWidget *parent )
  : QgsMapLayerConfigWidget( layer, canvas, parent )
  , mLayer( layer )
  , mCanvas( canvas )
  , mWidget( nullptr )
{
  setupUi( this );

  connect( mEngineSettingsButton, &QAbstractButton::clicked, this, &QgsLabelingWidget::showEngineConfigDialog );

  mLabelModeComboBox->setCurrentIndex( -1 );

  connect( mLabelModeComboBox, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsLabelingWidget::labelModeChanged );
  setLayer( layer );
}

void QgsLabelingWidget::resetSettings()
{
  if ( mOldSettings )
  {
    mLayer->setLabeling( mOldSettings.release() );
  }
  setLayer( mLayer );
}


void QgsLabelingWidget::setLayer( QgsMapLayer *mapLayer )
{
  if ( !mapLayer || mapLayer->type() != QgsMapLayer::VectorLayer )
  {
    setEnabled( false );
    return;
  }
  else
  {
    setEnabled( true );
  }

  QgsVectorLayer *layer = qobject_cast<QgsVectorLayer *>( mapLayer );
  mLayer = layer;
  if ( mLayer->labeling() )
  {
    mOldSettings.reset( mLayer->labeling()->clone() );
  }
  else
    mOldSettings.reset();

  adaptToLayer();
}

void QgsLabelingWidget::adaptToLayer()
{
  if ( !mLayer )
    return;

  // pick the right mode of the layer
  if ( mLayer->labeling() && mLayer->labeling()->type() == QLatin1String( "rule-based" ) )
  {
    mLabelModeComboBox->setCurrentIndex( 2 );
  }
  else if ( mLayer->labeling() && mLayer->labeling()->type() == QLatin1String( "simple" ) )
  {
    QgsPalLayerSettings lyr = mLayer->labeling()->settings();

    mLabelModeComboBox->setCurrentIndex( lyr.drawLabels ? 1 : 3 );
  }
  else
  {
    mLabelModeComboBox->setCurrentIndex( 0 );
  }

  QgsLabelingGui *lg = qobject_cast<QgsLabelingGui *>( mWidget );
  if ( lg )
  {
    lg->updateUi();
  }
}

void QgsLabelingWidget::writeSettingsToLayer()
{
  int index = mLabelModeComboBox->currentIndex();
  if ( index == 2 )
  {
    const QgsRuleBasedLabeling::Rule *rootRule = qobject_cast<QgsRuleBasedLabelingWidget *>( mWidget )->rootRule();

    mLayer->setLabeling( new QgsRuleBasedLabeling( rootRule->clone() ) );
  }
  else if ( index == 1 || index == 3 )
  {
    mLayer->setLabeling( new QgsVectorLayerSimpleLabeling( qobject_cast<QgsLabelingGui *>( mWidget )->layerSettings() ) );
  }
  else
  {
    mLayer->setLabeling( nullptr );
  }
}

void QgsLabelingWidget::apply()
{
  writeSettingsToLayer();
  QgisApp::instance()->markDirty();
  // trigger refresh
  mLayer->triggerRepaint();
}

void QgsLabelingWidget::labelModeChanged( int index )
{
  if ( mWidget )
    mStackedWidget->removeWidget( mWidget );

  delete mWidget;
  mWidget = nullptr;

  if ( index < 0 )
    return;

  if ( index == 2 )
  {
    QgsRuleBasedLabelingWidget *ruleWidget = new QgsRuleBasedLabelingWidget( mLayer, mCanvas, this );
    ruleWidget->setDockMode( dockMode() );
    connect( ruleWidget, &QgsPanelWidget::showPanel, this, &QgsPanelWidget::openPanel );
    connect( ruleWidget, &QgsRuleBasedLabelingWidget::widgetChanged, this, &QgsLabelingWidget::widgetChanged );
    mWidget = ruleWidget;
    mStackedWidget->addWidget( mWidget );
    mStackedWidget->setCurrentWidget( mWidget );
  }
  else if ( index == 1 || index == 3 )
  {
    if ( mLayer->labeling() && mLayer->labeling()->type() == QLatin1String( "simple" ) )
      mSimpleSettings.reset( new QgsPalLayerSettings( mLayer->labeling()->settings() ) );
    else
      mSimpleSettings.reset( new QgsPalLayerSettings() );

    QgsLabelingGui *simpleWidget = new QgsLabelingGui( mLayer, mCanvas, *mSimpleSettings, this );
    simpleWidget->setDockMode( dockMode() );
    connect( simpleWidget, &QgsTextFormatWidget::widgetChanged, this, &QgsLabelingWidget::widgetChanged );
    connect( simpleWidget, &QgsLabelingGui::autocreated, this, &QgsLabelingWidget::autocreated );

    if ( index == 3 )
      simpleWidget->setLabelMode( QgsLabelingGui::ObstaclesOnly );
    else
      simpleWidget->setLabelMode( static_cast< QgsLabelingGui::LabelMode >( index ) );

    mWidget = simpleWidget;
    mStackedWidget->addWidget( mWidget );
    mStackedWidget->setCurrentWidget( mWidget );
  }
  emit widgetChanged();
}

void QgsLabelingWidget::showEngineConfigDialog()
{
  QgsLabelEngineConfigDialog dlg( this );
  dlg.exec();
  emit widgetChanged();
}
