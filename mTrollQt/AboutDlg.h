#ifndef AboutDlg_h__
#define AboutDlg_h__

#include <qdialog>
#include <QLabel>
#include <QLayout>
#include <QPushButton>


class AboutDlg : public QDialog
{
public:
	AboutDlg();
	~AboutDlg();

private:
	QLabel		* mLabel;
	QBoxLayout	* mLayout;
	QPushButton	* mExitButton;
};

#endif // AboutDlg_h__
