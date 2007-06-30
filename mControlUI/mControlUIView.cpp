// mControlUIView.cpp : implementation of the CMControlUIView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"
#include <atlstr.h>
#include <atlmisc.h>
#include <algorithm>
#include <strstream>

#include "mControlUIView.h"
#include "..\Engine\EngineLoader.h"
#include "..\Engine\MidiControlEngine.h"
#include "..\Engine\UiLoader.h"
#include "ATLLabel.h"


CMControlUIView::CMControlUIView() :
	mEngine(NULL),
	mMainDisplay(NULL),
	mTraceDisplay(NULL),
	mPreferredHeight(0),
	mPreferredWidth(0),
	mMaxSwitchId(0),
	mKeyMessage(0)
{
}

CMControlUIView::~CMControlUIView()
{
	Unload();
}

struct DeleteMidiOut
{
	void operator()(const std::pair<unsigned int, WinMidiOut *> & pr)
	{
		delete pr.second;
	}
};

struct DeleteSwitchLed
{
	void operator()(const std::pair<int, CMControlUIView::SwitchLed *> & pr)
	{
		if (pr.second && pr.second->m_hWnd && ::IsWindow(pr.second->m_hWnd))
			pr.second->DestroyWindow();
		delete pr.second;
	}
};

struct DeleteSwitch
{
	void operator()(const std::pair<int, CMControlUIView::Switch *> & pr)
	{
		if (pr.second && pr.second->m_hWnd && ::IsWindow(pr.second->m_hWnd))
			pr.second->DestroyWindow();
		delete pr.second;
	}
};

struct DeleteSwitchTextDisplay
{
	void operator()(const std::pair<int, CMControlUIView::SwitchTextDisplay *> & pr)
	{
		if (pr.second && pr.second->m_hWnd && ::IsWindow(pr.second->m_hWnd))
			pr.second->DestroyWindow();
		delete pr.second;
	}
};

void
CMControlUIView::Unload()
{
	CloseMidiOuts();

	delete mEngine;
	mEngine = NULL;

	if (mMainDisplay && mMainDisplay->m_hWnd && ::IsWindow(mMainDisplay->m_hWnd))
		mMainDisplay->DestroyWindow();
	delete mMainDisplay;

	mMainDisplay = NULL;
	if (mTraceDisplay && mTraceDisplay->m_hWnd && ::IsWindow(mTraceDisplay->m_hWnd))
		mTraceDisplay->DestroyWindow();
	delete mTraceDisplay;
	mTraceDisplay = NULL;

	mMaxSwitchId = 0;
	mStupidSwitchStates.clear();

	if (mSwitchButtonFont)
		mSwitchButtonFont.DeleteObject();
	
	if (mTraceFont)
		mTraceFont.DeleteObject();

	for_each(mMidiOuts.begin(), mMidiOuts.end(), DeleteMidiOut());
	mMidiOuts.clear();

	for_each(mLeds.begin(), mLeds.end(), DeleteSwitchLed());
	mLeds.clear();

	for_each(mSwitches.begin(), mSwitches.end(), DeleteSwitch());
	mSwitches.clear();

	for_each(mSwitchTextDisplays.begin(), mSwitchTextDisplays.end(), DeleteSwitchTextDisplay());
	mSwitchTextDisplays.clear();
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
	IMidiOut * midiOut = GetMidiOut(0);
	if (midiOut)
	{
		const int kMidiOutCnt = midiOut->GetMidiOutDeviceCount();
		for (int idx = 0; idx < kMidiOutCnt; ++idx)
			Trace(midiOut->GetMidiOutDeviceName(idx) + "\n");
		Trace("\n");
	}
}

void
CMControlUIView::LoadMidiSettings(const std::string & file)
{
	delete mEngine;
	EngineLoader ldr(this, this, this, this);
	mEngine = ldr.CreateEngine(file);
	if (!mEngine)
		TextOut("Failed to load MIDI settings.");
}

BOOL CMControlUIView::PreTranslateMessage(MSG* pMsg)
{
	if (WM_CHAR == pMsg->message)
	{
		if (VK_RETURN == pMsg->wParam || VK_SPACE == pMsg->wParam)
			mKeyMessage = 0;
		else
			mKeyMessage = WM_CHAR;
	}
	else
		mKeyMessage = 0;

	BOOL retval = CWindow::IsDialogMessage(pMsg);
	mKeyMessage = 0;
	return retval;
}

LRESULT
CMControlUIView::OnBnPushed(WORD wNotifyCode, 
							WORD wID, 
							HWND hWndCtl, 
							BOOL& bHandled)
{
	if (WM_CHAR == mKeyMessage)
	{
		ButtonPressed(wID);
		ButtonReleased(wID);
	}
	return 0;
}

void
CMControlUIView::ButtonPressed(const int idx)
{
	if (!mStupidSwitchStates[idx])
	{
		mStupidSwitchStates[idx] = true;
		if (mEngine)
			mEngine->SwitchPressed(idx);
	}
}

void
CMControlUIView::ButtonReleased(const int idx)
{
	if (mStupidSwitchStates[idx])
	{
		mStupidSwitchStates[idx] = false;
		if (mEngine)
			mEngine->SwitchReleased(idx);
	}
}

// IMainDisplay
void
CMControlUIView::TextOut(const std::string & txt)
{
	if (mMainDisplay)
	{
		CStringA newTxt(txt.c_str());
		newTxt.Replace("\n", "\r\n");
		CStringA prevTxt;
		mMainDisplay->GetWindowText(prevTxt);
		if (newTxt != prevTxt)
			mMainDisplay->SetText(newTxt);
	}
}

void
CMControlUIView::ClearDisplay()
{
	if (mMainDisplay)
	{
		CStringA prevTxt;
		mMainDisplay->GetWindowText(prevTxt);
		if (!prevTxt.IsEmpty())
			mMainDisplay->SetWindowText("");
	}
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

	mLeds[switchNumber]->SetOnOff(isOn);
}

void
CMControlUIView::SetSwitchText(int switchNumber, const std::string & txt)
{
	if (!mSwitchTextDisplays[switchNumber] || !mSwitchTextDisplays[switchNumber]->IsWindow())
		return;

	mSwitchTextDisplays[switchNumber]->SetText(txt);
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
			ButtonPressed(idx);
		else
			ButtonReleased(idx);
	}

	return 0;
}


// IMidiControlUi
void
CMControlUIView::SetSwitchLedConfig(int width, 
									int height, 
									unsigned int onColor, 
									unsigned int offColor)
{
	mLedConfig.mWidth = width;
	mLedConfig.mHeight = height;
	mLedConfig.mOnColor = onColor;
	mLedConfig.mOffColor = offColor;
}

void
CMControlUIView::CreateSwitchLed(int id, 
								 int top, 
								 int left)
{
	SwitchLed * curSwitchLed = new SwitchLed;
	RECT rc;
	rc.top = top;
	rc.left = left;
	rc.bottom = top + mLedConfig.mHeight;
	rc.right = left + mLedConfig.mWidth;
	curSwitchLed->SetShape(ID_SHAPE_SQUARE);
	curSwitchLed->SetColor(mLedConfig.mOnColor, mLedConfig.mOffColor);
	curSwitchLed->Create(m_hWnd, rc, NULL, WS_VISIBLE | WS_CHILDWINDOW, WS_EX_NOPARENTNOTIFY);
	_ASSERTE(!mLeds[id]);
	mLeds[id] = curSwitchLed;
}

void
CMControlUIView::SetSwitchConfig(int width, 
								 int height, 
								 const std::string & fontName, 
								 int fontHeight, 
								 bool bold)
{
	mSwitchConfig.mWidth = width;
	mSwitchConfig.mHeight = height;
	mSwitchConfig.mFontname = fontName;
	mSwitchConfig.mFontHeight = fontHeight;
	mSwitchConfig.mBold = bold;
	mSwitchButtonFont.CreatePointFont(mSwitchConfig.mFontHeight * 10, mSwitchConfig.mFontname.c_str(), NULL, mSwitchConfig.mBold);
}

void
CMControlUIView::CreateSwitch(int id, 
							  const std::string & label, 
							  int top, 
							  int left)
{
	Switch * curSwitch = new Switch;
	RECT rc;
	rc.top = top;
	rc.left = left;
	rc.bottom = top + mSwitchConfig.mHeight;
	rc.right = left + mSwitchConfig.mWidth;
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
CMControlUIView::SetSwitchTextDisplayConfig(int width, 
											int height, 
											const std::string & fontName, 
											int fontHeight, 
											bool bold, 
											unsigned int bgColor, 
											unsigned int fgColor)
{
	mSwitchTextDisplayConfig.mHeight = height;
	mSwitchTextDisplayConfig.mWidth = width;
	mSwitchTextDisplayConfig.mFontname = fontName;
	mSwitchTextDisplayConfig.mFontHeight = fontHeight;
	mSwitchTextDisplayConfig.mBold = bold;
	mSwitchTextDisplayConfig.mFgColor = fgColor;
	mSwitchTextDisplayConfig.mBgColor = bgColor;
}

void
CMControlUIView::CreateSwitchTextDisplay(int id, 
										 int top, 
										 int left)
{
	SwitchTextDisplay * curSwitchDisplay = new SwitchTextDisplay;
	RECT rc;
	rc.top = top;
	rc.left = left;
	rc.bottom = top + mSwitchTextDisplayConfig.mHeight;
	rc.right = left + mSwitchTextDisplayConfig.mWidth;
	curSwitchDisplay->Create(m_hWnd, rc, NULL, 
		/*ES_AUTOHSCROLL |*/ ES_READONLY | WS_VISIBLE | WS_CHILDWINDOW | SS_LEFTNOWORDWRAP /*SS_CENTERIMAGE*/, 
		/*WS_EX_LEFT |*/ WS_EX_LTRREADING /*| WS_EX_CLIENTEDGE*/);
	curSwitchDisplay->Created();
	curSwitchDisplay->SetMargin(2);
	curSwitchDisplay->SetSunken(true);

	curSwitchDisplay->SetFontName(mSwitchTextDisplayConfig.mFontname);
	curSwitchDisplay->SetFontSize(mSwitchTextDisplayConfig.mFontHeight);
	curSwitchDisplay->SetFontBold(mSwitchTextDisplayConfig.mBold);
	curSwitchDisplay->SetBkColor(mSwitchTextDisplayConfig.mBgColor);
	curSwitchDisplay->SetTextColor(mSwitchTextDisplayConfig.mFgColor);

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
								   bool bold,
								   unsigned int bgColor, 
								   unsigned int fgColor)
{
	_ASSERTE(!mMainDisplay);
	mMainDisplay = new CLabel;
	RECT rc;
	rc.top = top;
	rc.left = left;
	rc.bottom = top + height;
	rc.right = left + width;
	mMainDisplay->Create(m_hWnd, rc, NULL, 
		ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_READONLY | WS_VISIBLE | WS_CHILDWINDOW /*| WS_VSCROLL | ES_LEFT | ES_MULTILINE*/, 
		WS_EX_NOPARENTNOTIFY /*| WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR | | WS_EX_CLIENTEDGE*/);

	mMainDisplay->Created();
	mMainDisplay->SetMargin(2);
	mMainDisplay->SetSunken(true);

	mMainDisplay->SetFontName(fontName);
	mMainDisplay->SetFontSize(fontHeight);
	mMainDisplay->SetFontBold(bold);
	mMainDisplay->SetBkColor(bgColor);
	mMainDisplay->SetTextColor(fgColor);
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
		WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOHSCROLL | ES_READONLY | WS_VISIBLE | WS_CHILDWINDOW, 
		WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_NOPARENTNOTIFY | WS_EX_CLIENTEDGE);

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
								   bool bold,
								   unsigned int bgColor, 
								   unsigned int fgColor)
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

IMidiOut *
CMControlUIView::GetMidiOut(unsigned int deviceIdx)
{
	if (!mMidiOuts[deviceIdx])
		mMidiOuts[deviceIdx] = new WinMidiOut(this);
	return mMidiOuts[deviceIdx];
}

void
CMControlUIView::OpenMidiOuts()
{
	for (MidiOuts::iterator it = mMidiOuts.begin();
		it != mMidiOuts.end();
		++it)
	{
		IMidiOut * curOut = (*it).second;
		if (curOut->IsMidiOutOpen())
			continue;

		std::strstream traceMsg;
		const unsigned int kDeviceIdx = (*it).first;

		if (curOut->OpenMidiOut(kDeviceIdx))
			traceMsg << "Opened MIDI out " << kDeviceIdx << " " << curOut->GetMidiOutDeviceName(kDeviceIdx) << std::endl << std::ends;
		else
			traceMsg << "Failed to open MIDI out " << kDeviceIdx << std::endl << std::ends;

		Trace(traceMsg.str());
	}
}

void
CMControlUIView::CloseMidiOuts()
{
	for (MidiOuts::iterator it = mMidiOuts.begin();
		it != mMidiOuts.end();
		++it)
	{
		IMidiOut * curOut = (*it).second;
		if (curOut->IsMidiOutOpen())
			curOut->CloseMidiOut();
	}
}
