#ifndef MainTrollWindow_h__
#define MainTrollWindow_h__

#include <QMainWindow>
#include "ControlUi.h"

class QMenu;


class MainTrollWindow : public QMainWindow
{
	Q_OBJECT;
public:
	MainTrollWindow();
	~MainTrollWindow();

private slots:
	void About();
	void OpenFile();
	void Refresh();

private:
	QMenu	*mFileMenu;
	QMenu	*mHelpMenu;

	QString		mConfigFilename;
	QString		mUiFilename;

	ControlUi	mUi;
};

#endif // MainTrollWindow_h__
