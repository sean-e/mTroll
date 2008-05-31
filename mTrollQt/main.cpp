#include <QApplication>
#include <qdebug.h>


int main(int argc, char **argv)
{
	QApplication app(argc, argv);
// 	QMap<QString, QSize> customSizeHints = parseCustomSizeHints(argc, argv);
// 	MainWindow mainWin(customSizeHints);
// 	mainWin.resize(800, 600);
// 	mainWin.show();
	app.aboutQt();
	return 1;
//	return app.exec();
}
