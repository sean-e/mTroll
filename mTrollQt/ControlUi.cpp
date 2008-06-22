#include <algorithm>
#include <strstream>

#include <qthread.h>
#include <QLabel>
#include <QTextEdit>

#include "ControlUi.h"
#include "../Engine/EngineLoader.h"
#include "../Engine/MidiControlEngine.h"
#include "../Engine/UiLoader.h"
#include "../Monome40h/IMonome40h.h"

#ifdef _WINDOWS
#include "../Monome40h/Win32/Monome40hFtw.h"
#include "../winUtil/SEHexception.h"
#endif // _WINDOWS


ControlUi::ControlUi(QWidget * parent) :
	QWidget(parent),
	mEngine(NULL),
	mMainDisplay(NULL),
	mTraceDisplay(NULL),
	mPreferredHeight(0),
	mPreferredWidth(0),
	mMaxSwitchId(0),
	mHardwareUi(NULL),
	mLedIntensity(0),
	mInvertLeds(false),
	mSystemPowerOverride(NULL)
{
}

ControlUi::~ControlUi()
{
	Unload();
}

struct DeleteMidiOut
{
	void operator()(const std::pair<unsigned int, ControlUi::TMidiOut *> & pr)
	{
		delete pr.second;
	}
};

struct DeleteSwitchLed
{
	void operator()(const std::pair<int, ControlUi::SwitchLed *> & pr)
	{
		if (pr.second && pr.second->m_hWnd && ::IsWindow(pr.second->m_hWnd))
			pr.second->DestroyWindow();
		delete pr.second;
	}
};

struct DeleteSwitch
{
	void operator()(const std::pair<int, ControlUi::Switch *> & pr)
	{
		if (pr.second && pr.second->m_hWnd && ::IsWindow(pr.second->m_hWnd))
			pr.second->DestroyWindow();
		delete pr.second;
	}
};

struct DeleteSwitchTextDisplay
{
	void operator()(const std::pair<int, ControlUi::SwitchTextDisplay *> & pr)
	{
		if (pr.second && pr.second->m_hWnd && ::IsWindow(pr.second->m_hWnd))
			pr.second->DestroyWindow();
		delete pr.second;
	}
};

void
ControlUi::Unload()
{
	if (mHardwareUi)
	{
		mHardwareUi->Unsubscribe(mEngine);
		mHardwareUi->Unsubscribe(this);
	}

	CloseMidiOuts();

	delete mEngine;
	mEngine = NULL;

	// clear leds
	if (mHardwareUi)
	{
		for (std::map<int, bool>::iterator it = mStupidSwitchStates.begin();
			it != mStupidSwitchStates.end();
			++it)
		{
			SetSwitchDisplay((*it).first, false);
		}
	}
	mStupidSwitchStates.clear();

	delete mHardwareUi;
	mHardwareUi = NULL;

	if (mMainDisplay && mMainDisplay->m_hWnd && ::IsWindow(mMainDisplay->m_hWnd))
		mMainDisplay->DestroyWindow();
	delete mMainDisplay;

	mMainDisplay = NULL;
	if (mTraceDisplay && mTraceDisplay->m_hWnd && ::IsWindow(mTraceDisplay->m_hWnd))
		mTraceDisplay->DestroyWindow();
	delete mTraceDisplay;
	mTraceDisplay = NULL;

	mMaxSwitchId = 0;

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

	mSwitchNumberToRowCol.clear();
	mRowColToSwitchNumber.clear();

	if (m_hWnd)
		Invalidate();

	delete mSystemPowerOverride;
	mSystemPowerOverride = NULL;
}

void
ControlUi::Load(const std::string & uiSettingsFile, 
				const std::string & configSettingsFile)
{
	Unload();
	LoadUi(uiSettingsFile);
	LoadMonome();
	LoadMidiSettings(configSettingsFile);
	if (mHardwareUi)
	{
		mHardwareUi->Subscribe(this);
		mHardwareUi->Subscribe(mEngine);

		bool anyMidiOutOpen = false;
		for (MidiOuts::iterator it = mMidiOuts.begin();
			it != mMidiOuts.end();
			++it)
		{
			IMidiOut * curOut = (*it).second;
			if (curOut->IsMidiOutOpen())
			{
				anyMidiOutOpen = true;
				break;
			}
		}

		if (anyMidiOutOpen)
			mSystemPowerOverride = new KeepDisplayOn;
	}

	ActivityIndicatorHack();

	for (int idx = 0; mInvertLeds && idx < 64; ++idx)
		SetSwitchDisplay(idx, false);
}

void
ControlUi::LoadUi(const std::string & uiSettingsFile)
{
	UiLoader ldr(this, uiSettingsFile);

	Trace("Midi Devices:\n");
	IMidiOut * midiOut = CreateMidiOut(0, -1);
	if (midiOut)
	{
		const int kMidiOutCnt = midiOut->GetMidiOutDeviceCount();
		for (int idx = 0; idx < kMidiOutCnt; ++idx)
		{
			std::strstream msg;
			msg << "  " << idx << ": " << midiOut->GetMidiOutDeviceName(idx) << std::endl << std::ends;
			Trace(msg.str());
		}
		Trace("\n");
		midiOut->CloseMidiOut();
		mMidiOuts.erase(0);
	}

	if (mSwitches[0])
		mSwitches[0]->SetFocus();
}

void
ControlUi::LoadMonome()
{
	TMonome40h * monome = NULL;
	try
	{
		monome = new TMonome40h(this);
		int devIdx = monome->LocateMonomeDeviceIdx();
		if (-1 != devIdx)
		{
			const std::string devSerial(monome->GetDeviceSerialNumber(devIdx));
			if (!devSerial.empty())
			{
				if (monome->AcquireDevice(devSerial))
				{
					mHardwareUi = monome;
					monome = NULL;
					MonomeStartupSequence();
					mHardwareUi->SetLedIntensity(mLedIntensity);
				}
			}
		}
	}
	catch (const std::string & e)
	{
		Trace(e);
	}
#ifdef _WINDOWS
	catch (const SEHexception &)
	{
		Trace("ERROR: failed to load monome\n");
	}
#endif // _WINDOWS

	delete monome;
}

void
ControlUi::LoadMidiSettings(const std::string & file)
{
	delete mEngine;
	EngineLoader ldr(this, this, this, this);
	mEngine = ldr.CreateEngine(file);
	if (mEngine)
	{
		if (mHardwareUi)
			ldr.InitMonome(mHardwareUi);
	}
	else
		TextOut("Failed to load MIDI settings.");
}

void
ControlUi::ButtonPressed(const int idx)
{
	if (!mStupidSwitchStates[idx])
	{
		byte row, col;
		RowColFromSwitchNumber(idx, row, col);
		SwitchPressed(row, col);
	}
}

void
ControlUi::ButtonReleased(const int idx)
{
	if (mStupidSwitchStates[idx])
	{
		byte row, col;
		RowColFromSwitchNumber(idx, row, col);
		SwitchReleased(row, col);
	}
}

// IMainDisplay
void
ControlUi::TextOut(const std::string & txt)
{
	if (mMainDisplay)
	{
		std::string * str = new std::string(txt);
		PostMessage(WM_ASYNCTEXTOUT, (WPARAM)str);
	}
}

LRESULT
ControlUi::AsyncTextOut(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	_ASSERTE(uMsg == WM_ASYNCTEXTOUT);
	std::string *txt = reinterpret_cast<std::string*>(wParam);
	_ASSERTE(txt);
	if (mMainDisplay)
	{
		CStringA newTxt(txt->c_str());
		newTxt.Replace("\n", "\r\n");
		CStringA prevTxt;
		mMainDisplay->GetWindowText(prevTxt);
		if (newTxt != prevTxt)
			mMainDisplay->SetText(newTxt);
	}
	delete txt;
	bHandled = TRUE;
	return 1;
}

void
ControlUi::ClearDisplay()
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
ControlUi::Trace(const std::string & txt)
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
ControlUi::SetSwitchDisplay(int switchNumber, 
							bool isOn)
{
	if (mInvertLeds)
		isOn = !isOn;

	if (mHardwareUi)
	{
		byte row, col;
		RowColFromSwitchNumber(switchNumber, row, col);
		mHardwareUi->EnableLed(row, col, isOn);
	}

	if (!mLeds[switchNumber] || !mLeds[switchNumber]->IsWindow())
		return;

	mLeds[switchNumber]->SetOnOff(isOn);
}

void
ControlUi::SetSwitchText(int switchNumber, 
						 const std::string & txt)
{
	if (!mSwitchTextDisplays[switchNumber] || !mSwitchTextDisplays[switchNumber]->IsWindow())
		return;

	mSwitchTextDisplays[switchNumber]->SetText(txt);
}

void
ControlUi::ClearSwitchText(int switchNumber)
{
	SetSwitchText(switchNumber, std::string(""));
}


// IMidiControlUi
void
ControlUi::AddSwitchMapping(int switchNumber, 
							int row, 
							int col)
{
	int rc = (row << 16) | col;
	_ASSERTE(!mRowColToSwitchNumber[rc]);
	mRowColToSwitchNumber[rc] = switchNumber;
	_ASSERTE(!mSwitchNumberToRowCol[switchNumber]);
	mSwitchNumberToRowCol[switchNumber] = rc;
}

void
ControlUi::SetSwitchLedConfig(int width, 
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
ControlUi::CreateSwitchLed(int id, 
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
ControlUi::SetSwitchConfig(int width, 
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
ControlUi::CreateSwitch(int id, 
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
ControlUi::SetSwitchTextDisplayConfig(int width, 
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
ControlUi::CreateSwitchTextDisplay(int id, 
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
ControlUi::CreateMainDisplay(int top, 
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
ControlUi::CreateTraceDisplay(int top, 
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
ControlUi::CreateStaticLabel(const std::string & label, 
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
ControlUi::SetMainSize(int width, 
					   int height)
{
	mPreferredHeight = height;
	mPreferredWidth = width;
}

IMidiOut *
ControlUi::CreateMidiOut(unsigned int deviceIdx,
						 int activityIndicatorIdx)
{
	if (!mMidiOuts[deviceIdx])
		mMidiOuts[deviceIdx] = new TMidiOut(this);

	if (activityIndicatorIdx > 0)
		mMidiOuts[deviceIdx]->SetActivityIndicator(this, activityIndicatorIdx);

	return mMidiOuts[deviceIdx];
}

IMidiOut *
ControlUi::GetMidiOut(unsigned int deviceIdx)
{
	return mMidiOuts[deviceIdx];
}

void
ControlUi::OpenMidiOuts()
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
ControlUi::CloseMidiOuts()
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

// IMonome40hSwitchSubscriber
void
ControlUi::SwitchPressed(byte row, byte column)
{
	const int switchNumber = SwitchNumberFromRowCol(row, column);
	mStupidSwitchStates[switchNumber] = true;
	if (mEngine)
		mEngine->SwitchPressed(switchNumber);
}

void
ControlUi::SwitchReleased(byte row, byte column)
{
	const int switchNumber = SwitchNumberFromRowCol(row, column);
	mStupidSwitchStates[switchNumber] = false;
	if (mEngine)
		mEngine->SwitchReleased(switchNumber);
}

void
ControlUi::MonomeStartupSequence()
{
	int idx;
	byte vals = 0xff;
	for (idx = 0; idx < 8; ++idx)
	{
		mHardwareUi->EnableLedRow(idx, vals);
		QThread::msleep(100);
		mHardwareUi->EnableLedRow(idx, 0);
	}

	for (idx = 0; idx < 8; ++idx)
	{
		mHardwareUi->EnableLedColumn(idx, vals);
		QThread::msleep(100);
		mHardwareUi->EnableLedColumn(idx, 0);
	}

	mHardwareUi->TestLed(true);
	QThread::msleep(250);
	mHardwareUi->TestLed(false);
}

void
ControlUi::ActivityIndicatorHack()
{
	Bytes bytes;
	bytes.push_back(0xf0);
	bytes.push_back(0xf7);

	for (MidiOuts::iterator it = mMidiOuts.begin();
		it != mMidiOuts.end();
		++it)
	{
		IMidiOut * curOut = (*it).second;
		if (!curOut->IsMidiOutOpen())
			continue;

		curOut->MidiOut(bytes);
	}
}
