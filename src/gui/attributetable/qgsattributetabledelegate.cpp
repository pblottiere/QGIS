/***************************************************************************
     QgsAttributeTableDelegate.cpp
     --------------------------------------
    Date                 : Feb 2009
    Copyright            : (C) 2009 Vita Cizek
    Email                : weetya (at) gmail.com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QItemDelegate>
#include <QLineEdit>
#include <QComboBox>
#include <QPainter>
#include <QToolButton>

#include "qgsattributetabledelegate.h"
#include "qgsattributetablefiltermodel.h"
#include "qgsattributetablemodel.h"
#include "qgsattributetableview.h"
#include "qgseditorwidgetregistry.h"
#include "qgseditorwidgetwrapper.h"
#include "qgsfeatureselectionmodel.h"
#include "qgslogger.h"
#include "qgsvectordataprovider.h"
#include "qgsactionmanager.h"
#include "qgsvectorlayerjoininfo.h"
#include "qgsvectorlayerjoinbuffer.h"


QgsVectorLayer *QgsAttributeTableDelegate::layer( const QAbstractItemModel *model )
{
  const QgsAttributeTableModel *tm = qobject_cast<const QgsAttributeTableModel *>( model );
  if ( tm )
    return tm->layer();

  const QgsAttributeTableFilterModel *fm = qobject_cast<const QgsAttributeTableFilterModel *>( model );
  if ( fm )
    return fm->layer();

  return nullptr;
}

const QgsAttributeTableModel *QgsAttributeTableDelegate::masterModel( const QAbstractItemModel *model )
{
  const QgsAttributeTableModel *tm = qobject_cast<const QgsAttributeTableModel *>( model );
  if ( tm )
    return tm;

  const QgsAttributeTableFilterModel *fm = qobject_cast<const QgsAttributeTableFilterModel *>( model );
  if ( fm )
    return fm->masterModel();

  return nullptr;
}

QWidget *QgsAttributeTableDelegate::createEditor( QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index ) const
{
  Q_UNUSED( option );
  QgsVectorLayer *vl = layer( index.model() );
  if ( !vl )
    return nullptr;

  int fieldIdx = index.model()->data( index, QgsAttributeTableModel::FieldIndexRole ).toInt();

  QgsAttributeEditorContext context( masterModel( index.model() )->editorContext(), QgsAttributeEditorContext::Popup );
  QgsEditorWidgetWrapper *eww = QgsEditorWidgetRegistry::instance()->create( vl, fieldIdx, nullptr, parent, context );
  QWidget *w = eww->widget();

  w->setAutoFillBackground( true );
  w->setFocusPolicy( Qt::StrongFocus ); // to make sure QMouseEvents are propagated to the editor widget

  int fieldOrigin = vl->fields().fieldOrigin( fieldIdx );
  bool enabled;
  if ( fieldOrigin == QgsFields::OriginJoin )
  {
    int srcFieldIndex;
    const QgsVectorLayerJoinInfo *info = vl->joinBuffer()->joinForFieldIndex( fieldIdx, vl->fields(), srcFieldIndex );

    if ( info && info->editable() )
      enabled = !info->joinLayer()->editFormConfig().readOnly( srcFieldIndex );
  }
  else
    enabled = !vl->editFormConfig().readOnly( fieldIdx );

  std::cout << "QgsAttributeTableDelegate::createEditor " <<  enabled << std::endl;
  eww->setEnabled( enabled );

  return w;
}

void QgsAttributeTableDelegate::setModelData( QWidget *editor, QAbstractItemModel *model, const QModelIndex &index ) const
{
  QgsVectorLayer *vl = layer( model );
  if ( !vl )
    return;

  int fieldIdx = model->data( index, QgsAttributeTableModel::FieldIndexRole ).toInt();
  QgsFeatureId fid = model->data( index, QgsAttributeTableModel::FeatureIdRole ).toLongLong();
  QVariant oldValue = model->data( index, Qt::EditRole );

  QVariant newValue;
  QgsEditorWidgetWrapper *eww = QgsEditorWidgetWrapper::fromWidget( editor );
  if ( !eww )
    return;

  newValue = eww->value();

  if ( ( oldValue != newValue && newValue.isValid() ) || oldValue.isNull() != newValue.isNull() )
  {
    vl->beginEditCommand( tr( "Attribute changed" ) );
    vl->changeAttributeValue( fid, fieldIdx, newValue, oldValue );
    vl->endEditCommand();
  }
}

void QgsAttributeTableDelegate::setEditorData( QWidget *editor, const QModelIndex &index ) const
{
  QgsEditorWidgetWrapper *eww =  QgsEditorWidgetWrapper::fromWidget( editor );
  if ( !eww )
    return;

  eww->setValue( index.model()->data( index, Qt::EditRole ) );
}

void QgsAttributeTableDelegate::setFeatureSelectionModel( QgsFeatureSelectionModel *featureSelectionModel )
{
  mFeatureSelectionModel = featureSelectionModel;
}

void QgsAttributeTableDelegate::paint( QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index ) const
{
  QgsAttributeTableFilterModel::ColumnType columnType = static_cast<QgsAttributeTableFilterModel::ColumnType>( index.model()->data( index, QgsAttributeTableFilterModel::TypeRole ).toInt() );
  /* std::cout << "----------------------------------------" << std::endl;
   std::cout << "QgsAttributeTableDelegate::paint column " << index.column() << std::endl;
   std::cout << "QgsAttributeTableDelegate::paint data.toString " << index.data().toString().toStdString() << std::endl;
   std::cout << "QgsAttributeTableDelegate::paint flags " << index.flags() << std::endl;
   std::cout << "QgsAttributeTableDelegate::paint 0 columnType " << columnType << std::endl;*/

  if ( columnType == QgsAttributeTableFilterModel::ColumnTypeActionButton )
  {
//   std::cout << "QgsAttributeTableDelegate::paint 1" << std::endl;
    emit actionColumnItemPainted( index );
  }
  else
  {
    //  std::cout << "QgsAttributeTableDelegate::paint 2" << std::endl;
    QgsFeatureId fid = index.model()->data( index, QgsAttributeTableModel::FeatureIdRole ).toLongLong();

    QStyleOptionViewItem myOpt = option;

    if ( index.model()->data( index, Qt::EditRole ).isNull() )
    {
      //   std::cout << "QgsAttributeTableDelegate::paint 3" << std::endl;
      myOpt.font.setItalic( true );
      myOpt.palette.setColor( QPalette::Text, QColor( "gray" ) );
    }

    if ( mFeatureSelectionModel && mFeatureSelectionModel->isSelected( fid ) )
    {
      //  std::cout << "QgsAttributeTableDelegate::paint 4" << std::endl;
      myOpt.state |= QStyle::State_Selected;
    }

    QItemDelegate::paint( painter, myOpt, index );

    if ( option.state & QStyle::State_HasFocus )
    {
      //  std::cout << "QgsAttributeTableDelegate::paint 5" << std::endl;
      QRect r = option.rect.adjusted( 1, 1, -1, -1 );
      QPen p( QBrush( QColor( 0, 255, 127 ) ), 2 );
      painter->save();
      painter->setPen( p );
      painter->drawRect( r );
      painter->restore();
    }
  }
// std::cout << "============================================" << std::endl;
}
