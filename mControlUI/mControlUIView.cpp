// mControlUIView.cpp : implementation of the CMControlUIView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"
#include <atlstr.h>

#include "mControlUIView.h"
#include "..\Engine\EngineLoader.h"
#include "..\Engine\MidiControlEngine.h"


CMControlUIView::CMControlUIView()
{
}

CMControlUIView::~CMControlUIView()
{
	delete mEngine;
}

HWND
CMControlUIView::Create(HWND hWndParent, LPARAM dwInitParam /*= NULL*/)
{
	HWND wnd = CDialogImpl<CMControlUIView>::Create(hWndParent, dwInitParam);
	if (wnd)
	{
		mMainDisplay = (CEdit) GetDlgItem(IDC_BANKTEXT);

		mLeds[0] = (CProgressBarCtrl) GetDlgItem(IDC_LED1);
		mLeds[1] = (CProgressBarCtrl) GetDlgItem(IDC_LED2);
		mLeds[2] = (CProgressBarCtrl) GetDlgItem(IDC_LED3);
		mLeds[3] = (CProgressBarCtrl) GetDlgItem(IDC_LED4);
		mLeds[4] = (CProgressBarCtrl) GetDlgItem(IDC_LED5);
		mLeds[5] = (CProgressBarCtrl) GetDlgItem(IDC_LED6);
		mLeds[6] = (CProgressBarCtrl) GetDlgItem(IDC_LED7);
		mLeds[7] = (CProgressBarCtrl) GetDlgItem(IDC_LED8);
		mLeds[8] = (CProgressBarCtrl) GetDlgItem(IDC_LED9);
		mLeds[9] = (CProgressBarCtrl) GetDlgItem(IDC_LED10);
		mLeds[10] = (CProgressBarCtrl) GetDlgItem(IDC_LED11);
		mLeds[11] = (CProgressBarCtrl) GetDlgItem(IDC_LED12);
		mLeds[12] = (CProgressBarCtrl) GetDlgItem(IDC_LED13);
		mLeds[13] = (CProgressBarCtrl) GetDlgItem(IDC_LED14);

		mSwitchTextDisplays[0] = (CEdit) GetDlgItem(IDC_SWITCH_TEXT1);
		mSwitchTextDisplays[1] = (CEdit) GetDlgItem(IDC_SWITCH_TEXT2);
		mSwitchTextDisplays[2] = (CEdit) GetDlgItem(IDC_SWITCH_TEXT3);
		mSwitchTextDisplays[3] = (CEdit) GetDlgItem(IDC_SWITCH_TEXT4);
		mSwitchTextDisplays[4] = (CEdit) GetDlgItem(IDC_SWITCH_TEXT5);
		mSwitchTextDisplays[5] = (CEdit) GetDlgItem(IDC_SWITCH_TEXT6);
		mSwitchTextDisplays[6] = (CEdit) GetDlgItem(IDC_SWITCH_TEXT7);
		mSwitchTextDisplays[7] = (CEdit) GetDlgItem(IDC_SWITCH_TEXT8);
		mSwitchTextDisplays[8] = (CEdit) GetDlgItem(IDC_SWITCH_TEXT9);
		mSwitchTextDisplays[9] = (CEdit) GetDlgItem(IDC_SWITCH_TEXT10);
		mSwitchTextDisplays[10] = (CEdit) GetDlgItem(IDC_SWITCH_TEXT11);
		mSwitchTextDisplays[11] = (CEdit) GetDlgItem(IDC_SWITCH_TEXT12);
		mSwitchTextDisplays[12] = (CEdit) GetDlgItem(IDC_SWITCH_TEXT13);
		mSwitchTextDisplays[13] = (CEdit) GetDlgItem(IDC_SWITCH_TEXT14);

		mSwitches[0] = (CButton) GetDlgItem(IDC_SWITCH1);
		mSwitches[1] = (CButton) GetDlgItem(IDC_SWITCH2);
		mSwitches[2] = (CButton) GetDlgItem(IDC_SWITCH3);
		mSwitches[3] = (CButton) GetDlgItem(IDC_SWITCH4);
		mSwitches[4] = (CButton) GetDlgItem(IDC_SWITCH5);
		mSwitches[5] = (CButton) GetDlgItem(IDC_SWITCH6);
		mSwitches[6] = (CButton) GetDlgItem(IDC_SWITCH7);
		mSwitches[7] = (CButton) GetDlgItem(IDC_SWITCH8);
		mSwitches[8] = (CButton) GetDlgItem(IDC_SWITCH9);
		mSwitches[9] = (CButton) GetDlgItem(IDC_SWITCH10);
		mSwitches[10] = (CButton) GetDlgItem(IDC_SWITCH11);
		mSwitches[11] = (CButton) GetDlgItem(IDC_SWITCH12);
		mSwitches[12] = (CButton) GetDlgItem(IDC_SWITCH13);
		mSwitches[13] = (CButton) GetDlgItem(IDC_SWITCH14);
		mSwitches[14] = (CButton) GetDlgItem(IDC_SWITCH15);
		mSwitches[15] = (CButton) GetDlgItem(IDC_SWITCH16);

		for (int idx = 0; idx < 16; ++idx)
		{
			if (mLeds[idx].IsWindow())
				mLeds[idx].SetRange(1, 10);
			mStupidSwitchStates[idx] = false;
		}

		EngineLoader ldr(this, this, this, this);
		mEngine = ldr.CreateEngine("test.xml");
	}

	return wnd;
}

BOOL CMControlUIView::PreTranslateMessage(MSG* pMsg)
{
	return CWindow::IsDialogMessage(pMsg);
}


// IMainDisplay
void
CMControlUIView::TextOut(const std::string & txt)
{
	mMainDisplay.SetWindowText(txt.c_str());
}

void
CMControlUIView::ClearDisplay()
{
	mMainDisplay.SetWindowText("");
}

// ITraceDisplay
void
CMControlUIView::Trace(const std::string & txt)
{

}

// ISwitchDisplay
void
CMControlUIView::SetSwitchDisplay(int switchNumber, bool isOn)
{
	if (!mLeds[switchNumber].IsWindow())
		return;

	mLeds[switchNumber].SetPos(isOn ? 10 : 1);
}

bool
CMControlUIView::SupportsSwitchText() const
{
	return false;
}

void
CMControlUIView::SetSwitchText(int switchNumber, const std::string & txt)
{
	if (!mSwitchTextDisplays[switchNumber].IsWindow())
		return;

	mSwitchTextDisplays[switchNumber].SetWindowText(txt.c_str());
}

void
CMControlUIView::ClearSwitchText(int switchNumber)
{
	SetSwitchText(switchNumber, "");
}

// IMidiOut
int
CMControlUIView::GetDeviceCount()
{
	return 0;
}

std::string
CMControlUIView::GetDeviceName(int deviceIdx)
{
	return "";
}

bool
CMControlUIView::OpenMidiOut(int deviceIdx)
{
	return false;
}

bool
CMControlUIView::MidiOut(const Bytes & bytes)
{
	return false;
}

void
CMControlUIView::CloseMidiOut()
{
}

LRESULT 
CMControlUIView::OnDrawItem(UINT /*uMsg*/, 
							WPARAM wParam, 
							LPARAM lParam, 
							BOOL& /*bHandled*/)
{
	DRAWITEMSTRUCT * drStr = (DRAWITEMSTRUCT *) lParam;

	_ASSERTE(drStr->CtlType == ODT_BUTTON);
	_ASSERTE(wParam == drStr->CtlID);

	int idx = -1;
	switch (wParam)
	{
	case IDC_SWITCH1:	idx = 0;	break;
	case IDC_SWITCH2:	idx = 1;	break;
	case IDC_SWITCH3:	idx = 2;	break;
	case IDC_SWITCH4:	idx = 3;	break;
	case IDC_SWITCH5:	idx = 4;	break;
	case IDC_SWITCH6:	idx = 5;	break;
	case IDC_SWITCH7:	idx = 6;	break;
	case IDC_SWITCH8:	idx = 7;	break;
	case IDC_SWITCH9:	idx = 8;	break;
	case IDC_SWITCH10:	idx = 9;	break;
	case IDC_SWITCH11:	idx = 10;	break;
	case IDC_SWITCH12:	idx = 11;	break;
	case IDC_SWITCH13:	idx = 12;	break;
	case IDC_SWITCH14:	idx = 13;	break;
	case IDC_SWITCH15:	idx = 14;	break;
	case IDC_SWITCH16:	idx = 15;	break;
	}

	if (-1 != idx)
	{
		if (drStr->itemState & ODS_SELECTED)
		{
			if (!mStupidSwitchStates[idx])
			{
				mStupidSwitchStates[idx] = true;
				mEngine->SwitchPressed(idx);
			}
		}
		else
		{
			if (mStupidSwitchStates[idx])
			{
				mStupidSwitchStates[idx] = false;
				mEngine->SwitchReleased(idx);
			}
		}
	}

	UINT uStyle = DFCS_BUTTONPUSH;

	// If drawing selected, add the pushed style to DrawFrameControl.
	if (drStr->itemState & ODS_SELECTED)
		uStyle |= DFCS_PUSHED;

	// Draw the button frame.
	::DrawFrameControl(drStr->hDC, &drStr->rcItem, DFC_BUTTON, uStyle);

	// Get the button's text.
	CString strText;
	GetWindowText(strText);

	// Draw the button text using the text color red.
	COLORREF crOldColor = ::SetTextColor(drStr->hDC, RGB(0,0,128));
	::DrawText(drStr->hDC, strText, strText.GetLength(), &drStr->rcItem, DT_SINGLELINE|DT_VCENTER|DT_CENTER);
	::SetTextColor(drStr->hDC, crOldColor);
	return 1;
}
