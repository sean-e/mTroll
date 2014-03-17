/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2014 Sean Echevarria
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

#include <algorithm>
#include <strstream>

#include <QApplication>
#include <qthread.h>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QtEvents>
#include <QTimer>
#include <QDateTime>

#include "ControlUi.h"
#include "../Engine/EngineLoader.h"
#include "../Engine/MidiControlEngine.h"
#include "../Engine/UiLoader.h"
#include "../Engine/HexStringUtils.h"
#include "../Monome40h/IMonome40h.h"
#include "../Monome40h/qt/Monome40hFtqt.h"

#ifdef _WINDOWS
	#include "../winUtil/SEHexception.h"
	#include "../midi/WinMidiOut.h"
	#include "../midi/WinMidiIn.h"
	#define USE_MIDI_IN
	typedef WinMidiOut	XMidiOut;
	typedef WinMidiIn	XMidiIn;
	#define SLEEP	Sleep
#else
	#error "include the midiOut header file for this platform"
	typedef YourMidiOut		XMidiOut;

//	#define USE_MIDI_IN
	#ifdef USE_MIDI_IN
		#error "include the midiIn header file for this platform"
		typedef YourMidiIn		XMidiIn;
	#endif
	#define SLEEP	sleep
#endif

const int kMaxRows = 8, kMaxCols = 8;
const int kMaxButtons = kMaxRows * kMaxCols;

ControlUi::ControlUi(QWidget * parent, ITrollApplication * app) :
	QWidget(parent),
	mParent(parent),
	mApp(app),
	mEngine(NULL),
	mMainDisplay(NULL),
	mTraceDisplay(NULL),
	mPreferredHeight(0),
	mPreferredWidth(0),
	mMaxSwitchId(0),
	mHardwareUi(NULL),
	mLedIntensity(0),
	mInvertLeds(false),
	mTimeDisplayTimer(NULL),
	mDisplayTime(false),
	mSystemPowerOverride(NULL),
	mBackgroundColor(0x1a1a1a),
	mFrameHighlightColor(0x5a5a5a),
	mSwitchLedUpdateEnabled(true),
	mLastUiButtonPressed(-1),
	mLastUiButtonEventTime(0),
	mStartTime(QDateTime::currentDateTime())
{
}

ControlUi::~ControlUi()
{
	Unload();
}

struct DeleteMidiOut
{
	void operator()(const std::pair<unsigned int, IMidiOut *> & pr)
	{
		delete pr.second;
	}
};

struct DeleteMidiIn
{
	void operator()(const std::pair<unsigned int, IMidiIn *> & pr)
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

class ControlUiEvent : public QEvent
{
public:
	ControlUiEvent(QEvent::Type t) : QEvent(t) {}
	virtual ~ControlUiEvent() {}

	virtual void exec() = 0;
};

void
ControlUi::Unload()
{
	StopTimer();

	if (mHardwareUi)
	{
		mHardwareUi->Unsubscribe(mEngine);
		mHardwareUi->Unsubscribe(this);
	}

	CloseMidiIns();
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

	QCoreApplication::removePostedEvents(this, QEvent::User);
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

	for_each(mMidiIns.begin(), mMidiIns.end(), DeleteMidiIn());
	mMidiIns.clear();

	for_each(mLeds.begin(), mLeds.end(), DeleteSwitchLed());
	mLeds.clear();

	for_each(mSwitches.begin(), mSwitches.end(), DeleteSwitch());
	mSwitches.clear();

	for_each(mSwitchTextDisplays.begin(), mSwitchTextDisplays.end(), DeleteSwitchTextDisplay());
	mSwitchTextDisplays.clear();

	mSwitchNumberToRowCol.clear();
	mRowColToSwitchNumber.clear();

	delete mSystemPowerOverride;
	mSystemPowerOverride = NULL;

	delete mTimeDisplayTimer;
	mTimeDisplayTimer = NULL;

	repaint();
}

void
ControlUi::Load(const std::string & uiSettingsFile, 
				const std::string & configSettingsFile,
				const bool adcOverrides[ExpressionPedals::PedalCount])
{
	Unload();

	QPalette pal;
	pal.setColor(QPalette::Window, mBackgroundColor);
	mParent->setPalette(pal);

	for (int idx = 0; idx < ExpressionPedals::PedalCount; ++idx)
		mUserAdcSettings[idx] = false;

	LoadUi(uiSettingsFile);
	LoadMonome(true);
	LoadMidiSettings(configSettingsFile, adcOverrides);

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
			if (curOut && curOut->IsMidiOutOpen())
			{
				anyMidiOutOpen = true;
				break;
			}
		}

		if (anyMidiOutOpen)
			mSystemPowerOverride = new KeepDisplayOn;
	}

	for (int idx = 0; mInvertLeds && idx < kMaxButtons; ++idx)
		SetSwitchDisplay(idx, false);
}

void
ControlUi::LoadUi(const std::string & uiSettingsFile)
{
	UiLoader ldr(this, uiSettingsFile);

	Trace("Midi Output Devices:\n");
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

	IMidiIn * midiIn = CreateMidiIn(0);
	if (midiIn)
	{
		Trace("Midi Input Devices:\n");
		const int kMidiInCnt = midiIn->GetMidiInDeviceCount();
		for (int idx = 0; idx < kMidiInCnt; ++idx)
		{
			std::strstream msg;
			msg << "  " << idx << ": " << midiIn->GetMidiInDeviceName(idx) << std::endl << std::ends;
			Trace(msg.str());
		}
		Trace("\n");
		midiIn->CloseMidiIn();
		mMidiIns.erase(0);
	}

	if (mSwitches[0])
		mSwitches[0]->setFocus();
}

void
ControlUi::LoadMonome(bool displayStartSequence)
{
	IMonome40h * monome = NULL;
	try
	{
		monome = new Monome40hFtqt(this);
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
					if (displayStartSequence)
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
ControlUi::LoadMidiSettings(const std::string & file, 
							const bool adcOverrides[ExpressionPedals::PedalCount])
{
	delete mEngine;
	EngineLoader ldr(mApp, this, this, this, this, this);
	mEngine = ldr.CreateEngine(file);
	if (mEngine)
	{
		if (mHardwareUi)
			ldr.InitMonome(mHardwareUi, adcOverrides, mUserAdcSettings);
	}
	else
		TextOut("Failed to load MIDI settings.");
}

void
ControlUi::ButtonPressed(const int idx)
{
	if (!mStupidSwitchStates[idx])
	{
		clock_t curTime = ::clock();
		if (idx == mLastUiButtonPressed && 
			0 != mLastUiButtonEventTime &&
			curTime < (mLastUiButtonEventTime + 50))
		{
			// ignore press due to Qt emitting pressed twice when using touch
			mLastUiButtonEventTime = 0;
			return;
		}

		mLastUiButtonEventTime = curTime;
		mLastUiButtonPressed = idx;
		byte row, col;
		if (RowColFromSwitchNumber(idx, row, col))
			SwitchPressed(row, col);
	}
}

void
ControlUi::ButtonReleased(const int idx)
{
	if (mStupidSwitchStates[idx])
	{
		if (0 == mLastUiButtonEventTime)
		{
			// ignore release (though touch doesn't appear to emit 
			// a release signal)
			return;
		}

		// reset so that next press is honored (otherwise potentially ignored)
		mLastUiButtonEventTime = ::clock();

		byte row, col;
		if (RowColFromSwitchNumber(idx, row, col))
			SwitchReleased(row, col);
	}
}

class RestoreMainTextEvent : public ControlUiEvent
{
	ControlUi * mUi;
	QPlainTextEdit *mLabel;

public:
	RestoreMainTextEvent(ControlUi * ui, QPlainTextEdit* label) : 
		ControlUiEvent(User),
		mUi(ui),
		mLabel(label)
	{
	}

	virtual void exec()
	{
		const QString prevTxt(mLabel->toPlainText());
		const QString newTxt(mUi->mMainText);
		if (prevTxt != newTxt)
			mLabel->setPlainText(newTxt);
	}
};

class EditTextOutEvent : public ControlUiEvent
{
	ControlUi * mUi;
	QPlainTextEdit *mLabel;
	QString mText;
	bool mTransientText;

public:
	EditTextOutEvent(ControlUi * ui, QPlainTextEdit* label, const QString text, bool transientTxt = false) : 
		ControlUiEvent(User),
		mUi(ui),
		mText(text),
		mLabel(label),
		mTransientText(transientTxt)
	{
	}

	virtual void exec()
	{
		const QString prevTxt(mLabel->toPlainText());
		if (mTransientText)
		{
			// transient text is prepended to non-transient
			mText = mText + mUi->mMainText;
		}

		if (prevTxt != mText)
		{
			if (!mTransientText)
				mUi->mMainText = mText;
			mLabel->setPlainText(mText);
		}
	}
};

class EditAppendEvent : public ControlUiEvent
{
	ControlUi * mUi;
	QPlainTextEdit *mLabel;
	QString mText;

public:
	EditAppendEvent(ControlUi * ui, QPlainTextEdit* label, const QString text) : 
		ControlUiEvent(User),
		mUi(ui),
		mText(text),
		mLabel(label)
	{
	}

	virtual void exec()
	{
		// assumes non-transient text
		QString txt(mUi->mMainText);
		txt += mText;
		mUi->mMainText = txt;
		mLabel->setPlainText(txt);
	}
};

// IMainDisplay
void
ControlUi::TextOut(const std::string & txt)
{
	if (!mMainDisplay)
		return;

	QCoreApplication::postEvent(this, 
		new EditTextOutEvent(this, mMainDisplay, txt.c_str()));
}

void
ControlUi::AppendText(const std::string & text)
{
	if (!mMainDisplay)
		return;

	QCoreApplication::postEvent(this, 
		new EditAppendEvent(this, mMainDisplay, text.c_str()));
}

void
ControlUi::ClearDisplay()
{
	if (!mMainDisplay)
		return;

	QCoreApplication::postEvent(this, 
		new EditTextOutEvent(this, mMainDisplay, ""));
}

void
ControlUi::TransientTextOut(const std::string & txt)
{
	if (!mMainDisplay)
		return;

	QCoreApplication::postEvent(this, 
		new EditTextOutEvent(this, mMainDisplay, txt.c_str(), true));
}

void
ControlUi::ClearTransientText()
{
	QCoreApplication::postEvent(this, 
		new RestoreMainTextEvent(this, mMainDisplay));
}

std::string
ControlUi::GetCurrentText()
{
	return std::string(mMainText.toUtf8());
}

// ITraceDisplay
void
ControlUi::Trace(const std::string & txt)
{
	class TextEditAppend : public ControlUiEvent
	{
		QPlainTextEdit *mTextEdit;
		QString mText;

	public:
		TextEditAppend(QPlainTextEdit * txtEdit, const QString & text) : 
			ControlUiEvent(User),
			mText(text),
			mTextEdit(txtEdit)
		{
		}

		virtual void exec()
		{
			mTextEdit->moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
			mTextEdit->insertPlainText(mText);
		}
	};

	if (mTraceDisplay)
	{
		QCoreApplication::postEvent(this, 
			new TextEditAppend(mTraceDisplay, txt.c_str()));
	}
}


// ISwitchDisplay
void
ControlUi::SetSwitchDisplay(int switchNumber, 
							bool isOn)
{
	if (!mSwitchLedUpdateEnabled)
		return;

	ForceSwitchDisplay(switchNumber, isOn);
}

void
ControlUi::ForceSwitchDisplay(int switchNumber, 
							  bool isOn)
{
	_ASSERTE(switchNumber < kMaxButtons);
	if (mInvertLeds)
		isOn = !isOn;

	if (mHardwareUi)
	{
		byte row, col;
		if (RowColFromSwitchNumber(switchNumber, row, col))
			mHardwareUi->EnableLed(row, col, isOn);
	}

	if (!mLeds[switchNumber] || !mLeds[switchNumber]->isEnabled())
		return;


	class UpdateSwitchDisplayEvent : public ControlUiEvent
	{
		ControlUi::SwitchLed * mLed;
		DWORD mColor;
		DWORD mFrameHighlightColor;

	public:
		UpdateSwitchDisplayEvent(ControlUi::SwitchLed * led, DWORD color, DWORD frameHighlightColor) : 
			ControlUiEvent(User),
			mLed(led),
			mColor(color),
			mFrameHighlightColor(frameHighlightColor)
		{
		}

		virtual void exec()
		{
			QPalette pal;
			pal.setColor(QPalette::Window, mColor);
			pal.setColor(QPalette::Light, mColor);
			pal.setColor(QPalette::Dark, mFrameHighlightColor);
			mLed->setPalette(pal);
		}
	};

	QCoreApplication::postEvent(this, 
		new UpdateSwitchDisplayEvent(mLeds[switchNumber], isOn ? mLedConfig.mOnColor : mLedConfig.mOffColor, mFrameHighlightColor));
}

class LabelTextOutEvent : public ControlUiEvent
{
	QLabel *mLabel;
	QString mText;

public:
	LabelTextOutEvent(QLabel * label, const QString & text) : 
		ControlUiEvent(User),
		mText(text),
		mLabel(label)
	{
	}

	virtual void exec()
	{
		const QString prevTxt(mLabel->text());
		if (prevTxt != mText)
			mLabel->setText(mText);
	}
};

void
ControlUi::SetSwitchText(int switchNumber, 
						 const std::string & txt)
{
	_ASSERTE(switchNumber < kMaxButtons);
	if (!mSwitchTextDisplays[switchNumber] || !mSwitchTextDisplays[switchNumber]->isEnabled())
		return;

	QCoreApplication::postEvent(this, 
		new LabelTextOutEvent(mSwitchTextDisplays[switchNumber], txt.c_str()));
}

void
ControlUi::ClearSwitchText(int switchNumber)
{
	SetSwitchText(switchNumber, std::string(""));
}

void
SetIndicatorTimerCallback::TimerFired()
{
	mPatch->ActivateSwitchDisplay(mSwitchDisplay, mOn);
	deleteLater();
}

void
ControlUi::SetIndicatorThreadSafe(bool isOn, Patch * patch, int time)
{
	// ClearIndicatorTimer
	// --------------------------------------------------------------------
	// class used to handle LED state change on UI thread (via postEvent)
	//
	class ClearIndicatorTimer : public QEvent
	{
		int mTime;
		ISwitchDisplay * mSwitchDisplay;
		Patch * mPatch;
		bool mOn;

	public:
		ClearIndicatorTimer(bool isOn, ISwitchDisplay * switchDisplay, Patch * patch, int time) : 
		  QEvent(User), 
		  mTime(time),
		  mPatch(patch),
		  mSwitchDisplay(switchDisplay),
		  mOn(isOn)
		{
		}

		virtual void exec()
		{
			if (mTime)
			{
				// use timer
				SetIndicatorTimerCallback * clr = new SetIndicatorTimerCallback(mOn, mSwitchDisplay, mPatch);
				QTimer::singleShot(mTime, clr, SLOT(TimerFired()));
			}
			else
			{
				// immediate
				mPatch->ActivateSwitchDisplay(mSwitchDisplay, mOn);
			}
		}
	};

	QCoreApplication::postEvent(this, new ClearIndicatorTimer(isOn, this, patch, time));
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
	SwitchLed * curSwitchLed = new SwitchLed(this);
//	curSwitchLed->SetShape(ID_SHAPE_SQUARE);
//	curSwitchLed->SetColor(mLedConfig.mOnColor, mLedConfig.mOffColor);

	curSwitchLed->setFrameShape(QFrame::Panel);
	curSwitchLed->setFrameShadow(QFrame::Sunken);
	curSwitchLed->setAutoFillBackground(true);
	curSwitchLed->move(left, top);
	curSwitchLed->resize(mLedConfig.mWidth, mLedConfig.mHeight);

	QPalette pal;
	pal.setColor(QPalette::Window, mLedConfig.mOffColor);
	pal.setColor(QPalette::Light, mLedConfig.mOffColor);
	pal.setColor(QPalette::Dark, mFrameHighlightColor);
	curSwitchLed->setPalette(pal);

	_ASSERTE(id < kMaxButtons);
	_ASSERTE(!mLeds[id]);
	mLeds[id] = curSwitchLed;
}

void
ControlUi::SetSwitchConfig(int width, 
						   int height, 
						   const std::string & fontName, 
						   int fontHeight, 
						   bool bold, 
						   unsigned int fgColor)
{
	mSwitchConfig.mWidth = width;
	mSwitchConfig.mHeight = height;
	mSwitchConfig.mFontname = fontName.c_str();
	mSwitchConfig.mFontHeight = fontHeight;
	mSwitchConfig.mBold = bold;
	mSwitchConfig.mFgColor = fgColor;

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
	Switch * curSwitch = new Switch(label.c_str(), this);
	curSwitch->setFont(mSwitchButtonFont);
	curSwitch->move(left, top);
	curSwitch->resize(mSwitchConfig.mWidth, mSwitchConfig.mHeight);

	curSwitch->setFlat(true);
	QPalette pal;
	pal.setColor(QPalette::Button, mFrameHighlightColor);
	pal.setColor(QPalette::Dark, mFrameHighlightColor);
	pal.setColor(QPalette::Light, mFrameHighlightColor);
	if (mSwitchConfig.mFgColor != -1)
		pal.setColor(QPalette::ButtonText, mSwitchConfig.mFgColor);
	curSwitch->setPalette(pal);
	curSwitch->setAutoFillBackground(true);

	const DWORD pressedColor(mFrameHighlightColor + 0x0a0a0a); // yeah, this could overflow...
	std::string pressedColorStr;
	pressedColorStr += ::GetAsciiHexStr(&((byte*)&pressedColor)[2], 1, false);
	pressedColorStr += ::GetAsciiHexStr(&((byte*)&pressedColor)[1], 1, false);
	pressedColorStr += ::GetAsciiHexStr(&((byte*)&pressedColor)[0], 1, false);

	const DWORD normalColor(mFrameHighlightColor); // yeah, this could overflow...
	std::string normalColorStr;
	normalColorStr += ::GetAsciiHexStr(&((byte*)&normalColor)[2], 1, false);
	normalColorStr += ::GetAsciiHexStr(&((byte*)&normalColor)[1], 1, false);
	normalColorStr += ::GetAsciiHexStr(&((byte*)&normalColor)[0], 1, false);

	// http://doc.qt.nokia.com/4.4/stylesheet-examples.html#customizing-qpushbutton
	// http://doc.qt.digia.com/4.4/stylesheet-examples.html#customizing-qpushbutton
	// http://qt-project.org/doc/qt-5.0/qtwidgets/stylesheet-examples.html#customizing-qpushbutton
	QString buttonStyleSheet(
	" \
	QPushButton { \
		border: none; \
		background-color: #" + QString(normalColorStr.c_str()) + "; \
	} \
	QPushButton:pressed { \
		padding-left: 2px; \
		padding-top: 2px; \
		border: none; \
		background-color: #" + QString(pressedColorStr.c_str()) + "; \
	} \
	");
	curSwitch->setStyleSheet(buttonStyleSheet);

	QString qSlot;

	qSlot = QString("1UiButtonPressed_%1()").arg(id);
	connect(curSwitch, SIGNAL(pressed()), qSlot.toUtf8().constData());

	qSlot = QString("1UiButtonReleased_%1()").arg(id);
	connect(curSwitch, SIGNAL(released()), qSlot.toUtf8().constData());

	std::string::size_type pos = label.find("&");
	if (std::string::npos != pos && pos+1 < label.length())
	{
		// override default shortcut so that Alt key does 
		// not need to be held down
		const QString shortCutKey = label[pos + 1];
		curSwitch->setShortcut(QKeySequence(shortCutKey));
	}
	
	_ASSERTE(id < kMaxButtons);
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
	SwitchTextDisplay * curSwitchDisplay = new SwitchTextDisplay(this);
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
	pal.setColor(QPalette::Light, mSwitchTextDisplayConfig.mBgColor);
	pal.setColor(QPalette::Dark, mFrameHighlightColor);
	curSwitchDisplay->setPalette(pal);

	_ASSERTE(id < kMaxButtons);
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
	mMainDisplay = new QPlainTextEdit(this);
	mMainDisplay->setFrameShape(QFrame::Panel);
	mMainDisplay->setFrameShadow(QFrame::Sunken);
	mMainDisplay->setReadOnly(true);
	mMainDisplay->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	mMainDisplay->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	mMainDisplay->setLineWrapMode(QPlainTextEdit::NoWrap);
	mMainDisplayRc.setTopLeft(QPoint(left, top));
	mMainDisplayRc.setBottomRight(QPoint(left+width, top+height));
	mMainDisplay->move(left, top);
	mMainDisplay->resize(width, height);

	QFont font(fontName.c_str(), fontHeight, bold ? QFont::Bold : QFont::Normal);
    mMainDisplay->setFont(font);

	QPalette pal;
	pal.setColor(QPalette::Window, bgColor);
	pal.setColor(QPalette::WindowText, fgColor);
	pal.setColor(QPalette::Text, fgColor);
	pal.setColor(QPalette::Base, bgColor);
	pal.setColor(QPalette::Light, mFrameHighlightColor);
	pal.setColor(QPalette::Dark, mFrameHighlightColor);
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
	mTraceDisplay = new QPlainTextEdit(this);
	mTraceDisplay->setFrameShape(QFrame::Panel);
	mTraceDisplay->setFrameShadow(QFrame::Sunken);
	mTraceDisplay->setReadOnly(true);
	mTraceDisplay->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	mTraceDisplay->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	mTraceDisplay->setLineWrapMode(QPlainTextEdit::NoWrap);
	mTraceDiplayRc.setTopLeft(QPoint(left, top));
	mTraceDiplayRc.setBottomRight(QPoint(left+width, top+height));
	mTraceDisplay->move(left, top);
	mTraceDisplay->resize(width, height);

	mTraceFont.setFamily(fontName.c_str());
	mTraceFont.setPointSize(fontHeight);
	mTraceFont.setBold(bold);
	mTraceDisplay->setFont(mTraceFont);

	QPalette pal;
	pal.setColor(QPalette::Light, mFrameHighlightColor);
	pal.setColor(QPalette::Dark, mFrameHighlightColor);
	mTraceDisplay->setPalette(pal);
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

unsigned int
ControlUi::GetMidiOutDeviceIndex(const std::string &deviceName)
{
	XMidiOut midiOut(nullptr);
	std::string devNameLower(deviceName);
	std::transform(devNameLower.begin(), devNameLower.end(), devNameLower.begin(), ::tolower);
	const unsigned int kCnt = midiOut.GetMidiOutDeviceCount();
	for (unsigned int idx = 0; idx < kCnt; ++idx)
	{
		std::string curName(midiOut.GetMidiOutDeviceName(idx));
		std::transform(curName.begin(), curName.end(), curName.begin(), ::tolower);
		if (-1 != curName.find(devNameLower))
			return idx;
	}

	return -1;
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
		if (!curOut || curOut->IsMidiOutOpen())
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
		if (curOut && curOut->IsMidiOutOpen())
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
	if (!mHardwareUi)
		return;

	int idx;
	byte vals = 0xff;
	for (idx = 0; idx < kMaxRows; ++idx)
	{
		mHardwareUi->EnableLedRow(idx, vals);
		SLEEP(100);
		mHardwareUi->EnableLedRow(idx, 0);
	}

	for (idx = 0; idx < kMaxCols; ++idx)
	{
		mHardwareUi->EnableLedColumn(idx, vals);
		SLEEP(100);
		mHardwareUi->EnableLedColumn(idx, 0);
	}

	mHardwareUi->TestLed(true);
	SLEEP(250);
	mHardwareUi->TestLed(false);
}

void
ControlUi::Reconnect()
{
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	const int kPorts = ExpressionPedals::PedalCount;
	// temporaries - possibly different than startup state due to interactive override
	bool adcEnables[kPorts] = {false, false, false, false};

	if (mHardwareUi)
	{
		mHardwareUi->Unsubscribe(mEngine);
		mHardwareUi->Unsubscribe(this);
		for (int idx = 0; idx < kPorts; ++idx)
			adcEnables[idx] = mHardwareUi->IsAdcEnabled(idx);
		delete mHardwareUi;
		mHardwareUi = NULL;
	}

	LoadMonome(false);

	if (mHardwareUi)
	{
		for (int idx = 0; idx < kPorts; ++idx)
			mHardwareUi->EnableAdc(idx, adcEnables[idx]);

		mHardwareUi->Subscribe(this);
		mHardwareUi->Subscribe(mEngine);
	}

	QApplication::restoreOverrideCursor();
}

void
ControlUi::ToggleTraceWindow()
{
	if (!mTraceDisplay)
		return;

	// ToggleTraceWindowEvent
	// --------------------------------------------------------------------
	// class used to toggle trace wnd on UI thread (via postEvent)
	//
	class ToggleTraceWindowEvent : public QEvent
	{
		ControlUi * mUi;

	public:
		ToggleTraceWindowEvent(ControlUi * ui) : 
		  QEvent(User), 
		  mUi(ui)
		{
		}

		virtual void exec()
		{
			mUi->ToggleTraceWindowCallback();
		}
	};

	QCoreApplication::postEvent(this, new ToggleTraceWindowEvent(this));
}

void
ControlUi::ToggleTraceWindowCallback()
{
	if (mTraceDisplay->isVisible())
	{
		mTraceDisplay->hide();
		QRect mainRc(mMainDisplayRc);

		if (mTraceDiplayRc.top() >= mMainDisplayRc.top() &&
			mTraceDiplayRc.bottom() <= mMainDisplayRc.bottom())
		{
			// change width of main display to use space vacated by trace display
			if (mMainDisplayRc.left() > mTraceDiplayRc.left())
				mainRc.setLeft(mTraceDiplayRc.left());
			else if (mMainDisplayRc.right() < mTraceDiplayRc.right())
				mainRc.setRight(mTraceDiplayRc.left() + mTraceDiplayRc.width());
		}

		if (mTraceDiplayRc.left() >= mMainDisplayRc.left() &&
			mTraceDiplayRc.right() <= mMainDisplayRc.right())
		{
			// change height of main display to use space vacated by trace display
			if (mMainDisplayRc.top() > mTraceDiplayRc.top())
				mainRc.setTop(mTraceDiplayRc.top());
			else if (mMainDisplayRc.bottom() < mTraceDiplayRc.bottom())
				mainRc.setBottom(mTraceDiplayRc.top() + mTraceDiplayRc.height());
		}

		mMainDisplay->move(mainRc.left(), mainRc.top());
		mMainDisplay->resize(mainRc.width(), mainRc.height());
	}
	else
	{
		mMainDisplay->move(mMainDisplayRc.left(), mMainDisplayRc.top());
		mMainDisplay->resize(mMainDisplayRc.width(), mMainDisplayRc.height());
		mTraceDisplay->show();
	}
}

bool 
ControlUi::event(QEvent* event)
{
	if (QEvent::User == event->type())
	{
		ControlUiEvent * evt = static_cast<ControlUiEvent*>(event);
		evt->exec();
		evt->accept();
		return true;
	}

	return QWidget::event(event);
}

void
ControlUi::UpdateAdcs(const bool adcOverrides[ExpressionPedals::PedalCount])
{
	if (!mHardwareUi)
		return;

	// handle in reverse order using (idx - 1)
	for (int idx = ExpressionPedals::PedalCount; idx; --idx)
	{
		const bool enable = !adcOverrides[idx - 1] && mUserAdcSettings[idx - 1];
		mHardwareUi->EnableAdc(idx - 1, enable);
		if (mTraceDisplay)
		{
			std::strstream traceMsg;
			traceMsg << "  ADC port " << idx << (enable ? " enabled" : " disabled") << std::endl << std::ends;
			Trace(std::string(traceMsg.str()));
		}
	}
}

void
ControlUi::InvertLeds(bool invert)
{
	mInvertLeds = invert;
}

void
ControlUi::TestLeds()
{
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	int row, col, switchNumber;

	// turn all on
	for (row = 0; row < kMaxRows; ++row)
	{
		for (col = 0; col < kMaxCols; ++col)
			SetSwitchDisplay((row * kMaxCols) + col, true);
	}

	QApplication::processEvents();
	QApplication::processEvents();
	SLEEP(750);

	// turn all off
	for (row = 0; row < kMaxRows; ++row)
	{
		for (col = 0; col < kMaxCols; ++col)
			SetSwitchDisplay((row * kMaxCols) + col, false);
	}

	// walk rows
	for (row = 0; row < kMaxRows; ++row)
	{
		for (col = 0; col < kMaxCols; ++col)
		{
			switchNumber = mRowColToSwitchNumber[(row << 16) | col];
			if (switchNumber || (!row && !col)) // assume only row0, col0 can be switchNumber 0
				SetSwitchDisplay(switchNumber, true);
		}
		QApplication::processEvents();
		QApplication::processEvents();
		SLEEP(200);
		for (col = 0; col < kMaxCols; ++col)
		{
			switchNumber = mRowColToSwitchNumber[(row << 16) | col];
			if (switchNumber || (!row && !col))
				SetSwitchDisplay(switchNumber, false);
		}
	}

	// walk columns
	for (col = 0; col < kMaxCols; ++col)
	{
		for (row = 0; row < kMaxRows; ++row)
		{
			switchNumber = mRowColToSwitchNumber[(row << 16) | col];
			if (switchNumber || (!row && !col))
				SetSwitchDisplay(switchNumber, true);
		}
		QApplication::processEvents();
		QApplication::processEvents();
		SLEEP(200);
		for (row = 0; row < kMaxRows; ++row)
		{
			switchNumber = mRowColToSwitchNumber[(row << 16) | col];
			if (switchNumber || (!row && !col))
				SetSwitchDisplay(switchNumber, false);
		}
	}

	// turn all on
	for (row = 0; row < kMaxRows; ++row)
	{
		for (col = 0; col < kMaxCols; ++col)
			SetSwitchDisplay((row * kMaxCols) + col, true);
	}

	QApplication::processEvents();
	QApplication::processEvents();
	SLEEP(750);

	// turn all off
	for (row = 0; row < kMaxRows; ++row)
	{
		for (col = 0; col < kMaxCols; ++col)
			SetSwitchDisplay((row * kMaxCols) + col, false);
	}

	QApplication::restoreOverrideCursor();
}

bool
ControlUi::EnableTimeDisplay(bool enable)
{
	if (!enable)
	{
		mDisplayTime = false;
		return false;
	}

	// CreateDisplayTimeTimer
	// --------------------------------------------------------------------
	// class used to create timer to fire time updates on UI thread (via postEvent)
	//
	class CreateDisplayTimeTimer : public QEvent
	{
		ControlUi * mUi;

	public:
		CreateDisplayTimeTimer(ControlUi * ui) : 
		  QEvent(User), 
		  mUi(ui)
		{
		}

		virtual void exec()
		{
			mUi->CreateTimeDisplayTimer();
		}
	};

	QCoreApplication::postEvent(this, new CreateDisplayTimeTimer(this));
	return true;
}

void
ControlUi::CreateTimeDisplayTimer()
{
	mDisplayTime = true;
	DisplayTime();
	if (!mTimeDisplayTimer)
		mTimeDisplayTimer = new QTimer(this);
	connect(mTimeDisplayTimer, SIGNAL(timeout()), this, SLOT(DisplayTime()));
	mTimeDisplayTimer->start(1000);
}

void
ControlUi::DisplayTime()
{
	if (mDisplayTime)
	{
		if (mMainDisplay)
		{
			const int kSecsInMinute = 60;
			const int kSecsInHour = kSecsInMinute * 60;
			const int kSecsInDay = kSecsInHour * 24;
			QDateTime now(QDateTime::currentDateTime());
			QString ts(now.toString("h:mm:ss ap \nddd, MMM d, yyyy"));

			qint64 secs = mStartTime.secsTo(now);
			const int days = secs > kSecsInDay ? secs / kSecsInDay : 0;
			if (days)
				secs -= (days * kSecsInDay);
			const int hours = secs > kSecsInHour ? secs / kSecsInHour : 0;
			if (hours)
				secs -= (hours * kSecsInHour);
			const int minutes = secs > kSecsInMinute ? secs / kSecsInMinute : 0;
			if (minutes)
				secs -= (minutes * kSecsInMinute);

			QString tmp;
			if (days)
				tmp.sprintf("\nelapsed: %d days, %d:%02d:%02d", days, hours, minutes, secs);
			else if (hours)
				tmp.sprintf("\nelapsed: %d hours, %02d:%02d", hours, minutes, secs);
			else
				tmp.sprintf("\nelapsed: %02d:%02d", minutes, secs);
			ts += tmp;

			const std::string msg(ts.toUtf8());
			TextOut(msg);
		}
	}
	else
	{
		mTimeDisplayTimer->stop();
		disconnect(mTimeDisplayTimer, SIGNAL(timeout()), this, SLOT(DisplayTime()));
	}
}

void
ControlUi::StopTimer()
{
	mDisplayTime = false;
}

IMidiIn *
ControlUi::GetMidiIn(unsigned int deviceIdx)
{
	return mMidiIns[deviceIdx];
}

void
ControlUi::CloseMidiIns()
{
	for (MidiIns::iterator it = mMidiIns.begin();
		it != mMidiIns.end();
		++it)
	{
		IMidiIn * curIn = (*it).second;
		if (curIn && curIn->IsMidiInOpen())
			curIn->CloseMidiIn();
	}
}

void
ControlUi::OpenMidiIns()
{
	for (MidiIns::iterator it = mMidiIns.begin();
		it != mMidiIns.end();
		++it)
	{
		IMidiIn * curIn = (*it).second;
		if (!curIn || curIn->IsMidiInOpen())
			continue;

		std::strstream traceMsg;
		const unsigned int kDeviceIdx = (*it).first;

		if (curIn->OpenMidiIn(kDeviceIdx))
			traceMsg << "Opened MIDI in " << kDeviceIdx << " " << curIn->GetMidiInDeviceName(kDeviceIdx) << std::endl << std::ends;
		else
			traceMsg << "Failed to open MIDI in " << kDeviceIdx << std::endl << std::ends;

		Trace(traceMsg.str());
	}
}

IMidiIn *
ControlUi::CreateMidiIn(unsigned int deviceIdx)
{
#ifdef USE_MIDI_IN
	if (!mMidiIns[deviceIdx])
		mMidiIns[deviceIdx] = new XMidiIn(this);

	return mMidiIns[deviceIdx];
#else
	return NULL;
#endif // USE_MIDI_IN
}

unsigned int
ControlUi::GetMidiInDeviceIndex(const std::string &deviceName)
{
#ifdef USE_MIDI_IN
	XMidiIn midiIn(nullptr);
	std::string devNameLower(deviceName);
	std::transform(devNameLower.begin(), devNameLower.end(), devNameLower.begin(), ::tolower);
	const unsigned int kCnt = midiIn.GetMidiInDeviceCount();
	for (unsigned int idx = 0; idx < kCnt; ++idx)
	{
		std::string curName(midiIn.GetMidiInDeviceName(idx));
		std::transform(curName.begin(), curName.end(), curName.begin(), ::tolower);
		if (-1 != curName.find(devNameLower))
			return idx;
	}

	return -1;
#else
	return NULL;
#endif // USE_MIDI_IN
}


bool
ControlUi::SuspendMidi()
{
	bool anySuspended = false;
	std::for_each(mMidiOuts.begin(), mMidiOuts.end(), 
		[&anySuspended](std::pair<unsigned int, IMidiOut *> pr)
	{
		if (pr.second->SuspendMidiOut())
			anySuspended = true;
	});

	std::for_each(mMidiIns.begin(), mMidiIns.end(), 
		[&anySuspended](const std::pair<unsigned int, IMidiIn *> pr)
	{
		if (pr.second->SuspendMidiIn())
			anySuspended = true;
	});

	if (anySuspended)
	{
		std::strstream msg;
		msg << "Suspended MIDI connections" << std::endl << std::ends;
		Trace(msg.str());
	}

	return anySuspended;
}

bool
ControlUi::ResumeMidi()
{
	bool allResumed = true;
	std::for_each(mMidiOuts.begin(), mMidiOuts.end(), 
		[&allResumed](std::pair<unsigned int, IMidiOut *> pr)
	{
		if (!pr.second->ResumeMidiOut())
			allResumed = false;
	});

	std::for_each(mMidiIns.begin(), mMidiIns.end(), 
		[&allResumed](const std::pair<unsigned int, IMidiIn *> pr)
	{
		if (!pr.second->ResumeMidiIn())
			allResumed = false;
	});

	if (allResumed)
	{
		std::strstream msg;
		msg << "Resumed MIDI connections" << std::endl << std::ends;
		Trace(msg.str());
	}

	return allResumed;
}
