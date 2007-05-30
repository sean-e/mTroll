// mControlUIView.cpp : implementation of the CMControlUIView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "mControlUIView.h"

BOOL CMControlUIView::PreTranslateMessage(MSG* pMsg)
{
	return CWindow::IsDialogMessage(pMsg);
}
