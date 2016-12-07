/***************************************************************************
                        qgsprogressdialog.h
    begin                : Dec 03, 2016
    copyright            : (C) 2016 by Paul Blottiere
    email                : paul dot blottiere at oslandia dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 **************************************************************************/

#ifndef QGSPROGRESSDIALOG_H
#define QGSPROGRESSDIALOG_H

#include <QProgressDialog>

class CORE_EXPORT QgsProgressDialog
{
  public:
    QgsProgressDialog( int minimum, int maximum );
    ~QgsProgressDialog();

    int maximum() const;

    void setMinimum( int minimum );
    void setMaximum( int maximum );
    void setRange( int minimum, int maximum );
    void setValue( int progress );
    int value() const;

  protected:
    void updateProgressBar();

    int mMinimum;
    int mMaximum;
    int mValue;
};

class CORE_EXPORT QgsProgressDialogProxy
{
  public:
    QgsProgressDialogProxy( const QString labelText, const QString cancelButtonText,
                            int minimum, int maximum );
    QgsProgressDialogProxy();
    ~QgsProgressDialogProxy();

    bool console() const;
    QProgressDialog* progressDialog() const;

    // proxy methods
    int maximum() const;

    void setLabelText( QString text );
    void setMinimum( int minimum );
    void setMaximum( int maximum );
    void setRange( int minimum, int maximum );
    void setValue( int progress );
    void setWindowTitle( QString windowTitle );
    void setWindowModality( Qt::WindowModality windowModality );
    void show();
    int value() const;
    bool wasCanceled() const;

  protected:
    QgsProgressDialog* mConsoleProgressDialog;
    QProgressDialog* mGuiProgressDialog;
};

#endif
