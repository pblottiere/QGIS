/***************************************************************************
                         qgsprogressdialog.cpp
    begin                : December 2016
    copyright            : (C) 2016 Paul Blottiere, Oslandia
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
#include <unistd.h>

#include <QProgressDialog>

#include "qgslogger.h"
#include "qgsapplication.h"
#include "qgsprogressdialog.h"

QgsProgressDialog::QgsProgressDialog( const QString &labelText, int minimum,
                                      int maximum )
    : mLabelText( labelText )
    , mMinimum( minimum )
    , mMaximum( maximum )
    , mValue( 0 )
{
}

QgsProgressDialog::~QgsProgressDialog()
{
}

void QgsProgressDialog::setMaximum( int maximum )
{

  mMaximum = maximum;
}

void QgsProgressDialog::setMinimum( int minimum )
{
  mMinimum = minimum;
}

void QgsProgressDialog::setValue( int progress )
{
  mValue = progress;
  printProgress();
}

void QgsProgressDialog::printProgress()
{
  QString header = "[";
  QString footer = "] ";
  QString cursor = ">";
  QString perc = " %";
  QString progress = "=";

  int barSize = 80 - header.size() - footer.size() - cursor.size() - perc.size() - 3;
  int percentage = mValue * 100 / ( mMaximum - mMinimum );
  int progressSize = percentage * barSize / 100;

  std::cout << header.toStdString();

  for ( int i = 0; i < progressSize; i++ )
    std::cout << progress.toStdString();

  std::cout << cursor.toStdString();

  for ( int i = 0; i < ( barSize - progressSize ); i++ )
    std::cout << " ";

  std::cout << footer.toStdString();
  std::cout << " " << percentage << " %";

  if ( percentage >= 100 )
    std::cout << std::endl;
  else
    std::cout << "\r";

  std::cout << std::flush;
}

QgsProgressDialogProxy::QgsProgressDialogProxy( const QString &labelText,
    const QString &cancelButtonText, int minimum, int maximum )
    : mConsoleProgressDialog( nullptr )
    , mGuiProgressDialog( nullptr )
{
  // detect if gui is running or if we are in console
  QWidget *mainWindow = QgsApplication::mainWindow();
  if ( mainWindow )
  {
    mGuiProgressDialog.reset( new QProgressDialog( labelText, cancelButtonText,
                              minimum, maximum, mainWindow ) );
  }
  else
  {
    mConsoleProgressDialog.reset( new QgsProgressDialog( labelText, minimum,
                                  maximum ) );
  }
}

QgsProgressDialogProxy::QgsProgressDialogProxy()
    : QgsProgressDialogProxy( "", "", 0, 100 )
{
}

QgsProgressDialogProxy::~QgsProgressDialogProxy()
{
}

bool QgsProgressDialogProxy::console() const
{
  if ( mConsoleProgressDialog )
    return true;
  else
    return false;
}

void QgsProgressDialogProxy::setMinimum( int minimum )
{
  if ( mConsoleProgressDialog )
    mConsoleProgressDialog->setMinimum( minimum );
  else
    mGuiProgressDialog->setMinimum( minimum );
}

void QgsProgressDialogProxy::setMaximum( int maximum )
{
  if ( mConsoleProgressDialog )
    mConsoleProgressDialog->setMaximum( maximum );
  else
    mGuiProgressDialog->setMaximum( maximum );
}

void QgsProgressDialogProxy::setValue( int progress )
{
  if ( mConsoleProgressDialog )
    mConsoleProgressDialog->setValue( progress );
  else
    mGuiProgressDialog->setValue( progress );
}

void QgsProgressDialogProxy::setWindowTitle( QString windowTitle )
{
  if ( mConsoleProgressDialog )
  {
    // nothing todo
  }
  else
    mGuiProgressDialog->setWindowTitle( windowTitle );
}

void QgsProgressDialogProxy::setWindowModality( Qt::WindowModality windowModality )
{
  if ( mConsoleProgressDialog )
  {
    // nothing todo
  }
  else
    mGuiProgressDialog->setWindowModality( windowModality );
}

bool QgsProgressDialogProxy::wasCanceled() const
{
  if ( mConsoleProgressDialog )
    return false;
  else
    return mGuiProgressDialog->wasCanceled();
}

QProgressDialog* QgsProgressDialogProxy::progressDialog() const
{
  return mGuiProgressDialog.data();
}
