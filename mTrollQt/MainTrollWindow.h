#ifndef MainTrollWindow_h__
#define MainTrollWindow_h__

#include <QMainWindow>


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
	QString		mConfigFilename;
	QString		mUiFilename;
};

#endif // MainTrollWindow_h__
