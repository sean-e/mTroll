/*
Original code copyright (c) 2007-2008,2018,2020,2025 Sean Echevarria ( http://www.creepingfog.com/sean/ )

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/

#include <QString.h>

// This file is always rebuilt in release configurations since
// it sets the date of the build when this file is compiled.


static int 
MonthNumber(const QString & name)
{
	const struct Month
	{
		const char *	mAbbr;
		int				mNumber;
	}
	months[] = 
	{
		{"jan",	1},
		{"feb",	2},
		{"mar",	3},
		{"apr",	4},
		{"may",	5},
		{"jun",	6},
		{"jul",	7},
		{"aug",	8},
		{"sep",	9},
		{"oct",	10},
		{"nov",	11},
		{"dec",	12},
	};

	for (auto month : months)
		if (name == month.mAbbr)
			return month.mNumber;

	_ASSERTE(!"no month name match");
	return 0;
}

QString 
GetBuildDate()
{
	const char * kBuildDate = __DATE__;
	QString tmp(kBuildDate);
	tmp = tmp.left(3);
	tmp = tmp.toLower();
	const int kBuildMon = MonthNumber(tmp);
	tmp = kBuildDate;
	tmp = tmp.right(tmp.length() - 4);
	int kBuildDay = 0, kBuildYear = 0;
#if defined(Q_OS_WIN)
	sscanf_s(tmp.toUtf8(), "%d %d", &kBuildDay, &kBuildYear);
#else
	sscanf(tmp.toUtf8(), "%d %d", &kBuildDay, &kBuildYear);
#endif

	QString date(QString::asprintf("%04d.%02d.%02d", kBuildYear, kBuildMon, kBuildDay));
	return date;
}
