#ifndef MainTrollWindow_h__
#define MainTrollWindow_h__

#include <QMainWindow>

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
	QMenu		*mFileMenu;
	QMenu		*mHelpMenu;

	QString		mConfigFilename;
	QString		mUiFilename;
};

#endif // MainTrollWindow_h__
