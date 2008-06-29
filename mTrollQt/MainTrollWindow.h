#ifndef MainTrollWindow_h__
#define MainTrollWindow_h__

#include <QMainWindow>

class ControlUi;


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
	void Reconnect();

private:
	ControlUi	* mUi;
	QString		mConfigFilename;
	QString		mUiFilename;
};

#endif // MainTrollWindow_h__
