#include <algorithm>
#include <strstream>

#include <qthread.h>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>

#include "ControlUi.h"
#include "../Engine/EngineLoader.h"
#include "../Engine/MidiControlEngine.h"
#include "../Engine/UiLoader.h"
#include "../Monome40h/IMonome40h.h"

#ifdef _WINDOWS
	#include "../winUtil/SEHexception.h"
	#include "../Monome40h/Win32/Monome40hFtw.h"
	typedef Monome40hFtw XMonome40h;
#else
	#error "include the monome header file for this platform"
	typedef YourMonome40h	XMonome40h;
#endif


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
//	setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
}

ControlUi::~ControlUi()
{
	Unload();
}

struct DeleteMidiOut
{
	void operator()(const std::pair<unsigned int, XMidiOut *> & pr)
	{
		delete pr.second;
	}
};

struct DeleteSwitchLed
{
	void operator()(const std::pair<int, ControlUi::SwitchLed *> & pr)
	{
		delete pr.second;
	}
};

struct DeleteSwitch
{
	void operator()(const std::pair<int, ControlUi::Switch *> & pr)
	{
		delete pr.second;
	}
};

struct DeleteSwitchTextDisplay
{
	void operator()(const std::pair<int, ControlUi::SwitchTextDisplay *> & pr)
	{
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

	delete mMainDisplay;
	mMainDisplay = NULL;

	delete mTraceDisplay;
	mTraceDisplay = NULL;

	mMaxSwitchId = 0;

	QFont tmp;
	mTraceFont = mSwitchButtonFont = tmp;

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

	repaint();

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
		mSwitches[0]->setFocus();
}

void
ControlUi::LoadMonome()
{
	XMonome40h * monome = NULL;
	try
	{
		monome = new XMonome40h(this);
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
		QString * str = new QString(txt.c_str());
		AsyncTextOut(str);
//		PostMessage(WM_ASYNCTEXTOUT, (WPARAM)str);
	}
}

void
ControlUi::AsyncTextOut(void * wParam)
{
	QString *newTxt= reinterpret_cast<QString*>(wParam);
	_ASSERTE(newTxt);
	if (mMainDisplay)
	{
//		newTxt->replace("\n", "\r\n");
		const QString prevTxt(mMainDisplay->text());
		if (prevTxt != newTxt)
			mMainDisplay->setText(*newTxt);
	}
	delete newTxt;
}

void
ControlUi::ClearDisplay()
{
	if (mMainDisplay)
	{
		const QString prevTxt(mMainDisplay->text());
		if (!prevTxt.isEmpty())
			mMainDisplay->setText("");
	}
}


// ITraceDisplay
void
ControlUi::Trace(const std::string & txt)
{
	if (mTraceDisplay)
	{
		QString newTxt(txt.c_str());
//		newTxt.replace("\n", "\r\n");
		mTraceDisplay->append(newTxt);
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

	if (!mLeds[switchNumber] || !mLeds[switchNumber]->isEnabled())
		return;

//	mLeds[switchNumber]->SetOnOff(isOn);
	QPalette pal;
	pal.setColor(QPalette::Window, isOn ? mLedConfig.mOnColor : mLedConfig.mOffColor);
	mLeds[switchNumber]->setPalette(pal);
}

void
ControlUi::SetSwitchText(int switchNumber, 
						 const std::string & txt)
{
	if (!mSwitchTextDisplays[switchNumber] || !mSwitchTextDisplays[switchNumber]->isEnabled())
		return;

	mSwitchTextDisplays[switchNumber]->setText(txt.c_str());
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
	SwitchLed * curSwitchLed = new SwitchLed(parentWidget());
//	curSwitchLed->SetShape(ID_SHAPE_SQUARE);
//	curSwitchLed->SetColor(mLedConfig.mOnColor, mLedConfig.mOffColor);

	curSwitchLed->setFrameShape(QFrame::Panel);
	curSwitchLed->setFrameShadow(QFrame::Sunken);
	curSwitchLed->setAutoFillBackground(true);
	curSwitchLed->move(left, top);
	curSwitchLed->resize(mLedConfig.mWidth, mLedConfig.mHeight);

	QPalette pal;
	pal.setColor(QPalette::Window, mLedConfig.mOffColor);
	curSwitchLed->setPalette(pal);

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
	mSwitchConfig.mFontname = fontName.c_str();
	mSwitchConfig.mFontHeight = fontHeight;
	mSwitchConfig.mBold = bold;

	mSwitchButtonFont.setFamily(mSwitchConfig.mFontname);
	mSwitchButtonFont.setPointSize(mSwitchConfig.mFontHeight);
	mSwitchButtonFont.setBold(mSwitchConfig.mBold);
}

void
ControlUi::CreateSwitch(int id, 
						const std::string & label, 
						int top, 
						int left)
{
	Switch * curSwitch = new Switch(label.c_str(), parentWidget());
	curSwitch->setFont(mSwitchButtonFont);
	curSwitch->move(left, top);
	curSwitch->resize(mSwitchConfig.mWidth, mSwitchConfig.mHeight);

	char slt[100];
	::sprintf(slt, "1UiButtonPressed_%d()", id);
	connect(curSwitch, SIGNAL(pressed()), slt);

	::sprintf(slt, "1UiButtonReleased_%d()", id);
	connect(curSwitch, SIGNAL(released()), slt);

	// todo: create connection mnemonic so that Alt key does not need
	// to be held down (like Win dialog accel)
	
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
	mSwitchTextDisplayConfig.mFontname = fontName.c_str();
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
	SwitchTextDisplay * curSwitchDisplay = new SwitchTextDisplay(parentWidget());
	curSwitchDisplay->setMargin(2);
	curSwitchDisplay->setFrameShape(QFrame::Panel);
	curSwitchDisplay->setFrameShadow(QFrame::Sunken);
	curSwitchDisplay->setAutoFillBackground(true);
	curSwitchDisplay->move(left, top);
	curSwitchDisplay->resize(mSwitchTextDisplayConfig.mWidth, mSwitchTextDisplayConfig.mHeight);

	QFont font(mSwitchTextDisplayConfig.mFontname, mSwitchTextDisplayConfig.mFontHeight, mSwitchTextDisplayConfig.mBold ? QFont::Bold : QFont::Normal);
    curSwitchDisplay->setFont(font);

	QPalette pal;
	pal.setColor(QPalette::Window, mSwitchTextDisplayConfig.mBgColor);
	pal.setColor(QPalette::WindowText, mSwitchTextDisplayConfig.mFgColor);
	curSwitchDisplay->setPalette(pal);

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
	mMainDisplay = new QLabel(parentWidget());
	mMainDisplay->setFrameShape(QFrame::Panel);
	mMainDisplay->setFrameShadow(QFrame::Sunken);
	mMainDisplay->setAutoFillBackground(true);
	mMainDisplay->setAlignment(Qt::AlignLeft);
	mMainDisplay->move(left, top);
	mMainDisplay->resize(width, height);

	QFont font(fontName.c_str(), fontHeight, bold ? QFont::Bold : QFont::Normal);
    mMainDisplay->setFont(font);

	QPalette pal;
	pal.setColor(QPalette::Window, bgColor);
	pal.setColor(QPalette::WindowText, fgColor);
	mMainDisplay->setPalette(pal);
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
	mTraceDisplay = new QTextEdit(parentWidget());
	mTraceDisplay->setFrameShape(QFrame::Panel);
	mTraceDisplay->setFrameShadow(QFrame::Sunken);
	mTraceDisplay->setReadOnly(true);
	mTraceDisplay->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	mTraceDisplay->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	mTraceDisplay->setAcceptRichText(false);
	mTraceDisplay->setLineWrapMode(QTextEdit::NoWrap);
	mTraceDisplay->move(left, top);
	mTraceDisplay->resize(width, height);

	mTraceFont.setFamily(fontName.c_str());
	mTraceFont.setPointSize(fontHeight);
	mTraceFont.setBold(bold);
	mTraceDisplay->setFont(mTraceFont);
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
		mMidiOuts[deviceIdx] = new XMidiOut(this);

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
		Sleep(100);
		mHardwareUi->EnableLedRow(idx, 0);
	}

	for (idx = 0; idx < 8; ++idx)
	{
		mHardwareUi->EnableLedColumn(idx, vals);
		Sleep(100);
		mHardwareUi->EnableLedColumn(idx, 0);
	}

	mHardwareUi->TestLed(true);
	Sleep(250);
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
