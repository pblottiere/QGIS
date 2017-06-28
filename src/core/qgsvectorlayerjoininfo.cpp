/***************************************************************************
                          qgsvectorlayerjoininfo.cpp
                          --------------------------
    begin                : Jun 26, 2017
    copyright            : (C) 2017 by Paul Blottiere
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

#include "qgsvectorlayerjoininfo.h"

void QgsVectorLayerJoinInfo::setEditable( bool editable )
{
  mEditable = editable;

  if ( !mEditable )
  {
    mUpsertOnEdit = false;
    mDeleteCascade = false;
  }
}

QString QgsVectorLayerJoinInfo::prefixedNameField( const QgsField &f ) const
{
  return prefixedNameField( f.name() );
}

QString QgsVectorLayerJoinInfo::prefixedNameField( const QString &fieldName ) const
{
  QString name;

  if ( joinLayer() )
  {
    if ( prefix().isNull() )
      name = joinLayer()->name() + '_';
    else
      name = prefix();

    name += fieldName;
  }

  return name;
}
