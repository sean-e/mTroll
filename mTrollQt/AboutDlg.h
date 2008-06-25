#ifndef AboutDlg_h__
#define AboutDlg_h__

#include <qdialog>

class QLabel;
class QBoxLayout;
class QPushButton;
class QGroupBox;


class AboutDlg : public QDialog
{
public:
	AboutDlg();
	~AboutDlg();

private:
	QLabel		* mLabel;
	QBoxLayout	* mLayout;
	QPushButton	* mExitButton;
	QGroupBox	* mLabelGroupBox;
};

#endif // AboutDlg_h__
