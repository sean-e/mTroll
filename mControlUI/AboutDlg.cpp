// aboutdlg.cpp : implementation of the CAboutDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "aboutdlg.h"
#include <atlstr.h>


// This file is always rebuilt in release configurations since
// it sets the date of the build when this file is compiled.
// This is required for license expiration to work correctly.


static int 
MonthNumber(const ATL::CString & name)
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

	_ASSERTE(!"no month name match");
	return 0;
}

ATL::CString 
GetBuildDate()
{
	const char * kBuildDate = __DATE__;
	ATL::CString tmp(kBuildDate);
	tmp = tmp.Left(3);
	tmp.MakeLower();
	const int kBuildMon = MonthNumber(tmp);
	tmp = kBuildDate;
	tmp = tmp.Right(tmp.GetLength() - 4);
	int kBuildDay = 0, kBuildYear = 0;
	sscanf_s(tmp, "%d %d", &kBuildDay, &kBuildYear);

	ATL::CString date;
	date.Format("%04d.%02d.%02d", kBuildYear, kBuildMon, kBuildDay);
	return date;
}

LRESULT CAboutDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CenterWindow(GetParent());
	CWindow ed(GetDlgItem(IDC_BUILD_DATE));
	ed.SetWindowText(CString("Built ") + ::GetBuildDate());
	return TRUE;
}

LRESULT CAboutDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}
