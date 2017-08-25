/************************************************************************
 * This file has been generated automatically from                      *
 *                                                                      *
 * src/core/qgsauxiliarystorage.h                                       *
 *                                                                      *
 * Do not edit manually ! Edit header and run scripts/sipify.pl again   *
 ************************************************************************/





class QgsAuxiliaryField : QgsField
{
%Docstring
*************************************************************************
                                                                         *
   This program is free software; you can redistribute it and/or modify  *
   it under the terms of the GNU General Public License as published by  *
   the Free Software Foundation; either version 2 of the License, or     *
   (at your option) any later version.                                   *
                                                                         *
**************************************************************************
%End

%TypeHeaderCode
#include "qgsauxiliarystorage.h"
%End
  public:
    QgsAuxiliaryField( const QgsPropertyDefinition &def );

    QgsPropertyDefinition propertyDefinition() const;
%Docstring
 :rtype: QgsPropertyDefinition
%End


    static QString name( const QgsPropertyDefinition &def, bool joined = false );
%Docstring
 :rtype: str
%End

};

typedef QList<QgsAuxiliaryField> QgsAuxiliaryFields;

class QgsAuxiliaryLayer : QgsVectorLayer
{

%TypeHeaderCode
#include "qgsauxiliarystorage.h"
%End
  public:
    QgsAuxiliaryLayer( const QString &pkField, const QString &filename, const QString &table, const QgsVectorLayer *vlayer );

    virtual ~QgsAuxiliaryLayer();



    QgsVectorLayer *toSpatialLayer() const;
%Docstring
QgsAuxiliaryStorage cannot be copied.
 :rtype: QgsVectorLayer
%End

    bool clear();
%Docstring
 :rtype: bool
%End

    QgsVectorLayerJoinInfo joinInfo() const;
%Docstring
 :rtype: QgsVectorLayerJoinInfo
%End

    bool exists( const QgsPropertyDefinition &definition ) const;
%Docstring
 :rtype: bool
%End

    bool addAuxiliaryField( const QgsPropertyDefinition &definition );
%Docstring
 :rtype: bool
%End

    QgsAuxiliaryFields auxiliaryFields() const;
%Docstring
 :rtype: QgsAuxiliaryFields
%End

    virtual bool deleteAttribute( int attr );

};

class QgsAuxiliaryStorage
{

%TypeHeaderCode
#include "qgsauxiliarystorage.h"
%End
  public:

    QgsAuxiliaryStorage( const QgsProject &project, bool copy = true );

    QgsAuxiliaryStorage( const QString &filename = QString(), bool copy = true );

    virtual ~QgsAuxiliaryStorage();

    bool isValid() const;
%Docstring
 :rtype: bool
%End

    QString fileName() const;
%Docstring
 :rtype: str
%End

    QString currentFileName() const;
%Docstring
 :rtype: str
%End

    bool saveAs( const QString &filename ) const;
%Docstring
 :rtype: bool
%End

    bool saveAs( const QgsProject &project ) const;
%Docstring
 :rtype: bool
%End

    bool save() const;
%Docstring
 :rtype: bool
%End

    QgsAuxiliaryLayer *createAuxiliaryLayer( const QgsVectorLayer *layer );
%Docstring
 :rtype: QgsAuxiliaryLayer
%End

    QgsAuxiliaryLayer *createAuxiliaryLayer( const QgsField &field, const QgsVectorLayer *layer );
%Docstring
 :rtype: QgsAuxiliaryLayer
%End

    static bool deleteTable( const QgsDataSourceUri &uri );
%Docstring
 :rtype: bool
%End

    static QString extension();
%Docstring
 :rtype: str
%End

};

/************************************************************************
 * This file has been generated automatically from                      *
 *                                                                      *
 * src/core/qgsauxiliarystorage.h                                       *
 *                                                                      *
 * Do not edit manually ! Edit header and run scripts/sipify.pl again   *
 ************************************************************************/
