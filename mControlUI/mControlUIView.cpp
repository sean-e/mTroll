// mControlUIView.cpp : implementation of the CMControlUIView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"
#include <atlstr.h>
#include <atlmisc.h>
#include <algorithm>

#include "mControlUIView.h"
#include "..\Engine\EngineLoader.h"
#include "..\Engine\MidiControlEngine.h"
#include "..\Engine\UiLoader.h"


CMControlUIView::CMControlUIView() :
	mEngine(NULL),
	mMainDisplay(NULL),
	mTraceDisplay(NULL),
	mPreferredHeight(0),
	mPreferredWidth(0),
	mMaxSwitchId(0),
	mMidiOut(this)
{
}

CMControlUIView::~CMControlUIView()
{
	Unload();
}

struct DeleteSwitchLed
{
	void operator()(const std::pair<int, CMControlUIView::SwitchLed *> & pr)
	{
		delete pr.second;
	}
};

struct DeleteSwitch
{
	void operator()(const std::pair<int, CMControlUIView::Switch *> & pr)
	{
		delete pr.second;
	}
};

struct DeleteSwitchTextDisplay
{
	void operator()(const std::pair<int, CMControlUIView::SwitchTextDisplay *> & pr)
	{
		delete pr.second;
	}
};

void
CMControlUIView::Unload()
{
	delete mEngine;
	mEngine = NULL;
	delete mMainDisplay;
	mMainDisplay = NULL;
	delete mTraceDisplay;
	mTraceDisplay = NULL;
	if (mMidiOut.IsMidiOutOpen())
		mMidiOut.CloseMidiOut();

	mMaxSwitchId = 0;
	mStupidSwitchStates.clear();

	for_each(mLeds.begin(), mLeds.end(), DeleteSwitchLed());
	mLeds.clear();

	for_each(mSwitches.begin(), mSwitches.end(), DeleteSwitch());
	mSwitches.clear();

	for_each(mSwitchTextDisplays.begin(), mSwitchTextDisplays.end(), DeleteSwitchTextDisplay());
	mSwitchTextDisplays.clear();

	if (mSwitchButtonFont)
		mSwitchButtonFont.DeleteObject();
	
	if (mSwitchDisplayFont)
		mSwitchDisplayFont.DeleteObject();

	if (mMainTextFont)
		mMainTextFont.DeleteObject();

	if (mTraceFont)
		mTraceFont.DeleteObject();
}

HWND
CMControlUIView::Create(HWND hWndParent, LPARAM dwInitParam /*= NULL*/)
{
	HWND wnd = CDialogImpl<CMControlUIView>::Create(hWndParent, dwInitParam);
	return wnd;
}

void
CMControlUIView::LoadUi(const std::string & uiSettingsFile)
{
	Unload();
	UiLoader ldr(this, uiSettingsFile);

	Trace("Midi Devices:\n");
	const int kMidiOutCnt = mMidiOut.GetMidiOutDeviceCount();
	for (int idx = 0; idx < kMidiOutCnt; ++idx)
		Trace(mMidiOut.GetMidiOutDeviceName(idx) + "\n");
	Trace("\n");
}

void
CMControlUIView::LoadMidiSettings(const std::string & file)
{
	delete mEngine;
	EngineLoader ldr(&mMidiOut, this, this, this);
	mEngine = ldr.CreateEngine(file);
}

BOOL CMControlUIView::PreTranslateMessage(MSG* pMsg)
{
	return CWindow::IsDialogMessage(pMsg);
}


// IMainDisplay
void
CMControlUIView::TextOut(const std::string & txt)
{
	if (mMainDisplay)
	{
		CStringA newTxt(txt.c_str());
		newTxt.Replace("\n", "\r\n");
		mMainDisplay->SetWindowText(newTxt);
	}
}

void
CMControlUIView::ClearDisplay()
{
	if (mMainDisplay)
		mMainDisplay->SetWindowText("");
}


// ITraceDisplay
void
CMControlUIView::Trace(const std::string & txt)
{
	if (mTraceDisplay)
	{
		ATL::CString newTxt(txt.c_str());
		newTxt.Replace("\n", "\r\n");
		mTraceDisplay->AppendText(newTxt);
	}
}


// ISwitchDisplay
void
CMControlUIView::SetSwitchDisplay(int switchNumber, bool isOn)
{
	if (!mLeds[switchNumber] || !mLeds[switchNumber]->IsWindow())
		return;

	mLeds[switchNumber]->SetPos(isOn ? 4 : 0);
}

bool
CMControlUIView::SupportsSwitchText() const
{
	return false;
}

void
CMControlUIView::SetSwitchText(int switchNumber, const std::string & txt)
{
	if (!mSwitchTextDisplays[switchNumber] || !mSwitchTextDisplays[switchNumber]->IsWindow())
		return;

	mSwitchTextDisplays[switchNumber]->SetWindowText(txt.c_str());
}

void
CMControlUIView::SetSwitchDisplayPos(int switchNumber, int pos, int range)
{
	if (!mSwitchTextDisplays[switchNumber] || !mSwitchTextDisplays[switchNumber]->IsWindow())
		return;

	mLeds[switchNumber]->SetRange(0, range);
	mLeds[switchNumber]->SetPos(pos);
}

void
CMControlUIView::ClearSwitchText(int switchNumber)
{
	SetSwitchText(switchNumber, std::string(""));
}

LRESULT
CMControlUIView::OnNotifyCustomDraw(int idCtrl,
									LPNMHDR pNotifyStruct,
									BOOL& /*bHandled*/)
{
	LPNMCUSTOMDRAW pCustomDraw = (LPNMCUSTOMDRAW) pNotifyStruct;
	_ASSERTE(pCustomDraw->hdr.code == NM_CUSTOMDRAW);
	_ASSERTE(pCustomDraw->hdr.idFrom == idCtrl);

	const int idx = pCustomDraw->hdr.idFrom;
	if (idx >= 0 && idx <= mMaxSwitchId)
	{
		if (pCustomDraw->uItemState & ODS_SELECTED)
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

	COLORREF crOldColor = ::SetTextColor(pCustomDraw->hdc, RGB(0,0,200));

	return 0;
}


// IMidiControlUi
void
CMControlUIView::CreateSwitchLed(int id, 
								 int top, 
								 int left, 
								 int width, 
								 int height)
{
	SwitchLed * curSwitchLed = new SwitchLed;
	RECT rc;
	rc.top = top;
	rc.left = left;
	rc.bottom = top + height;
	rc.right = left + width;
	curSwitchLed->Create(m_hWnd, rc, NULL, 
		WS_VISIBLE | WS_CHILDWINDOW | PBS_SMOOTH, 
		WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_NOPARENTNOTIFY | WS_EX_RIGHTSCROLLBAR);
	_ASSERTE(!mLeds[id]);
	curSwitchLed->SetRange(0, 4);
	mLeds[id] = curSwitchLed;
}

void
CMControlUIView::CreateSwitchFont(const std::string & fontName,
								  int fontHeight, 
								  bool bold)
{
	mSwitchButtonFont.CreatePointFont(fontHeight * 10, fontName.c_str(), NULL, bold);
}

void
CMControlUIView::CreateSwitch(int id, 
							  const std::string & label, 
							  int top, 
							  int left, 
							  int width, 
							  int height)
{
	Switch * curSwitch = new Switch;
	RECT rc;
	rc.top = top;
	rc.left = left;
	rc.bottom = top + height;
	rc.right = left + width;
	curSwitch->Create(m_hWnd, rc, label.c_str(), 
		WS_VISIBLE | WS_CHILDWINDOW | BS_PUSHBUTTON | BS_TEXT | WS_TABSTOP, 
		WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR | WS_EX_NOPARENTNOTIFY,
		id);
	curSwitch->SetFont(mSwitchButtonFont);
	_ASSERTE(!mSwitches[id]);
	mSwitches[id] = curSwitch;
	if (id > mMaxSwitchId)
		mMaxSwitchId = id;
}

void
CMControlUIView::CreateSwitchTextDisplayFont(const std::string & fontName,
											 int fontHeight, 
											 bool bold)
{
	mSwitchDisplayFont.CreatePointFont(fontHeight * 10, fontName.c_str(), NULL, bold);
}

void
CMControlUIView::CreateSwitchTextDisplay(int id, 
										 int top, 
										 int left, 
										 int width, 
										 int height)
{
	SwitchTextDisplay * curSwitchDisplay = new SwitchTextDisplay;
	RECT rc;
	rc.top = top;
	rc.left = left;
	rc.bottom = top + height;
	rc.right = left + width;
	curSwitchDisplay->Create(m_hWnd, rc, NULL, 
		ES_AUTOHSCROLL | /*ES_READONLY |*/ ES_LEFT | WS_VISIBLE | WS_CHILDWINDOW, 
		WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR | WS_EX_NOPARENTNOTIFY | WS_EX_CLIENTEDGE);
	curSwitchDisplay->SetFont(mSwitchDisplayFont);
	_ASSERTE(!mSwitchTextDisplays[id]);
	mSwitchTextDisplays[id] = curSwitchDisplay;
}

void
CMControlUIView::CreateMainDisplay(int top, 
								   int left, 
								   int width, 
								   int height,
								   const std::string & fontName,
								   int fontHeight, 
								   bool bold)
{
	_ASSERTE(!mMainDisplay);
	mMainDisplay = new CEdit;
	RECT rc;
	rc.top = top;
	rc.left = left;
	rc.bottom = top + height;
	rc.right = left + width;
	mMainDisplay->Create(m_hWnd, rc, NULL, 
		WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_READONLY | WS_VISIBLE | WS_CHILDWINDOW, 
		WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR | WS_EX_NOPARENTNOTIFY | WS_EX_CLIENTEDGE);

	mMainTextFont.CreatePointFont(fontHeight * 10, fontName.c_str(), NULL, bold);
	mMainDisplay->SetFont(mMainTextFont);
}

void
CMControlUIView::CreateTraceDisplay(int top, 
									int left, 
									int width, 
									int height, 
									const std::string & fontName,
									int fontHeight, 
									bool bold)
{
	_ASSERTE(!mTraceDisplay);
	mTraceDisplay = new CEdit;
	RECT rc;
	rc.top = top;
	rc.left = left;
	rc.bottom = top + height;
	rc.right = left + width;
	mTraceDisplay->Create(m_hWnd, rc, NULL, 
		WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_READONLY | WS_VISIBLE | WS_CHILDWINDOW, 
		WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR | WS_EX_NOPARENTNOTIFY | WS_EX_CLIENTEDGE);

	mTraceFont.CreatePointFont(fontHeight * 10, fontName.c_str(), NULL, bold);
	mTraceDisplay->SetFont(mTraceFont);
}

void
CMControlUIView::CreateStaticLabel(const std::string & label, 
								   int top, 
								   int left, 
								   int width, 
								   int height, 
								   const std::string & fontName,
								   int fontHeight, 
								   bool bold)
{
	_ASSERTE(!"not implemented yet");
}

void
CMControlUIView::SetMainSize(int width, 
							 int height)
{
	mPreferredHeight = height;
	mPreferredWidth = width;
}
