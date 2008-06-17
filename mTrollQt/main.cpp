#include <QApplication>
#include <qdebug.h>
#include "MainTrollWindow.h"

#if defined(_WINDOWS)
#include "..\winUtil\SEHexception.h"
#endif // _WINDOWS


int main(int argc, char **argv)
{
#if defined(_WINDOWS)
	::_set_se_translator(::trans_func);
#endif // _WINDOWS
	QApplication app(argc, argv);
	MainTrollWindow mainWin;
	mainWin.show();
	return app.exec();
}


#if defined(_WINDOWS)
void 
trans_func(unsigned int /*u*/, 
		   EXCEPTION_POINTERS* /*pExp*/)
{
    throw SEHexception();
}
#endif // _WINDOWS
