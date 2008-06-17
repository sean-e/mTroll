#include <QString.h>

// This file is always rebuilt in release configurations since
// it sets the date of the build when this file is compiled.
// This is required for license expiration to work correctly.


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

	for (int idx = 0; idx < 12; idx++)
		if (name == months[idx].mAbbr)
			return months[idx].mNumber;

#if defined(Q_OS_WIN)
	_ASSERTE(!"no month name match");
#else
	Q_ASSERT_X(0, "MonthNumber", "no month name match");
#endif
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
	sscanf_s(tmp.toStdString().c_str(), "%d %d", &kBuildDay, &kBuildYear);
#else
	sscanf(tmp.toStdString().c_str(), "%d %d", &kBuildDay, &kBuildYear);
#endif

	QString date;
	date.sprintf("%04d.%02d.%02d", kBuildYear, kBuildMon, kBuildDay);
	return date;
}
