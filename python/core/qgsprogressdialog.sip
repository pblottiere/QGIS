class QgsProgressDialog
{
%TypeHeaderCode
#include <qgsprogressdialog.h>
%End
  public:
    QgsProgressDialog( int minimum, int maximum );
    ~QgsProgressDialog();

    int maximum() const;

    void setMinimum( int minimum );
    void setMaximum( int maximum );
    void setRange( int minimum, int maximum );
    void setValue( int progress );
    int value() const;
};

class QgsProgressDialogProxy
{
%TypeHeaderCode
#include <qgsprogressdialog.h>
%End
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
    void setValue( int progress );
    void setRange( int minimum, int maximum );
    void setWindowTitle( QString windowTitle );
    void setWindowModality( Qt::WindowModality windowModality );
    void show();
    int value() const;
    bool wasCanceled() const;
};
