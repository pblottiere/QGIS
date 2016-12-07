class QgsProgressDialog : QObject
{
%TypeHeaderCode
#include <qgsprogressdialog.h>
%End
  public:
    QgsProgressDialog( int minimum, int maximum );
    ~QgsProgressDialog();

    int maximum() const;
    int value() const;

  public slots:
    void setMinimum( int minimum );
    void setMaximum( int maximum );
    void setRange( int minimum, int maximum );
    void setValue( int progress );
};

class QgsProgressDialogProxy : QObject
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

    void show();
    void setWindowTitle( QString windowTitle );
    void setWindowModality( Qt::WindowModality windowModality );
    int value() const;
    bool wasCanceled() const;

  public slots:
    void setLabelText( QString text );
    void setMinimum( int minimum );
    void setMaximum( int maximum );
    void setValue( int progress );
    void setRange( int minimum, int maximum );
};
