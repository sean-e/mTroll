/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2008,2018,2020,2025 Sean Echevarria
 *
 * This file is part of mTroll.
 *
 * mTroll is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mTroll is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Let me know if you modify, extend or use mTroll.
 * Original project site: http://www.creepingfog.com/mTroll/
 * Contact Sean: "fester" at the domain of the original project site
 */

#include <QApplication>
#include "MainTrollWindow.h"

#if defined(_WINDOWS)
#include "../winUtil/SEHexception.h"
#endif // _WINDOWS


int main(int argc, char **argv)
{
#if 0
	extern void TestPedals();
	TestPedals();
	return 0;
#endif

#if defined(_WINDOWS)
	::_set_se_translator(::trans_func);
#endif // _WINDOWS

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	// High DPI support
	//   http://blog.qt.io/blog/2016/01/26/high-dpi-support-in-qt-5-6/
	//   http://doc.qt.io/qt-5/highdpi.html
	//   https://stackoverflow.com/questions/32313658/qt-high-dpi-support-on-windows
	QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

	QApplication app(argc, argv);
	MainTrollWindow mainWin;
	mainWin.show();
	return app.exec();
}
