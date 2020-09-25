/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2015,2018,2020 Sean Echevarria
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
#include <QScrollBar>
#include <QGridLayout>
#include <QScrollArea>

#include "ControlUi.h"
#include "../Engine/ITrollApplication.h"
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
	using XMidiOut = WinMidiOut;
	using XMidiIn = WinMidiIn;
	#define SLEEP	Sleep
#else
	#error "include the midiOut header file for this platform"
	using XMidiOut = YourMidiOut;

//	#define USE_MIDI_IN
	#ifdef USE_MIDI_IN
		#error "include the midiIn header file for this platform"
		using XMidiIn = YourMidiIn;
	#endif
	#define SLEEP	sleep
#endif
#include "MainTrollWindow.h"

const int kMaxRows = 8, kMaxCols = 8;
const int kMaxButtons = kMaxRows * kMaxCols;

ControlUi::ControlUi(QWidget * parent, ITrollApplication * app) :
	QWidget(parent),
	mParent(parent),
	mApp(app),
	mEngine(nullptr),
	mMainDisplay(nullptr),
	mTraceDisplay(nullptr),
	mPreferredHeight(0),
	mPreferredWidth(0),
	mMaxSwitchId(0),
	mHardwareUi(nullptr),
	mLedIntensity(0),
	mTimeDisplayTimer(nullptr),
	mDisplayTime(false),
	mSystemPowerOverride(nullptr),
	mBackgroundColor(0x1a1a1a),
	mFrameHighlightColor(0x5a5a5a),
	mSwitchLedUpdateEnabled(true),
	mLastUiButtonPressed(-1),
	mLastUiButtonEventTime(0)
{
	mLedConfig.mPresetColors.fill(0x7f);
}

ControlUi::~ControlUi()
{
	Unload();
}

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
		mHardwareUi->Unsubscribe(mEngine.get());
		mHardwareUi->Unsubscribe(this);
	}

	CloseMidiIns();
	CloseMidiOuts();

	if (mEngine)
	{
		mEngine->Shutdown();
		mEngine = nullptr;
	}

	// clear leds
	if (mHardwareUi)
	{
		for (auto & mStupidSwitchState : mStupidSwitchStates)
			TurnOffSwitchDisplay(mStupidSwitchState.first);
	}

	QCoreApplication::removePostedEvents(this, QEvent::User);
	mStupidSwitchStates.clear();

	delete mHardwareUi;
	mHardwareUi = nullptr;

	delete mMainDisplay;
	mMainDisplay = nullptr;

	delete mTraceDisplay;
	mTraceDisplay = nullptr;

	mMaxSwitchId = 0;

	QFont tmp;
	mTraceFont = mSwitchButtonFont = tmp;

	mMidiOuts.clear();
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
	mSystemPowerOverride = nullptr;

	delete mTimeDisplayTimer;
	mTimeDisplayTimer = nullptr;

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

	for (bool & userAdcSetting : mUserAdcSettings)
		userAdcSetting = false;

	LoadUi(uiSettingsFile);
	LoadMonome(true);
	LoadMidiSettings(configSettingsFile, adcOverrides);

	if (mHardwareUi)
	{
		SendPresetColorsToMonome();
		mHardwareUi->Subscribe(this);
		mHardwareUi->Subscribe(mEngine.get());

		bool anyMidiOutOpen = false;
		for (auto & mMidiOut : mMidiOuts)
		{
			IMidiOutPtr curOut = mMidiOut.second;
			if (curOut && curOut->IsMidiOutOpen())
			{
				anyMidiOutOpen = true;
				break;
			}
		}

		if (anyMidiOutOpen)
			mSystemPowerOverride = new KeepDisplayOn;
	}
}

void
ControlUi::LoadUi(const std::string & uiSettingsFile)
{
	UiLoader ldr(this, uiSettingsFile);

	if (mGrid)
		setLayout(mGrid);

	Trace("Midi Output Devices:\n");
	IMidiOutPtr midiOut = CreateMidiOut(0, -1, 0);
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

	IMidiInPtr midiIn = CreateMidiIn(0);
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
	IMonome40h * monome = nullptr;
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
					monome = nullptr;
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
	if (mEngine)
	{
		_ASSERTE(!"unexpected engine instance");
		mEngine->Shutdown();
		mEngine = nullptr;
	}

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
			curTime < (mLastUiButtonEventTime + 120))
		{
			// ignore press due to Qt emitting pressed twice when using touch
			mLastUiButtonEventTime = curTime;
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

	virtual void exec() override
	{
		const QString prevTxt(mLabel->toPlainText());
		// if RestoreMainTextEvent is being used, leave a blank line
		// at the top of the display to prevent repeated shifting of
		// display text which can occur when expression pedals are 
		// in use.
		const QString newTxt(QString("\n") + mUi->mMainText);
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

	virtual void exec() override
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

	virtual void exec() override
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

		virtual void exec() override
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
class UpdateSwitchDisplayEvent : public ControlUiEvent
{
	ControlUi::SwitchLed * mLed;
	DWORD mColor;
	DWORD mFrameHighlightColor;

public:
	UpdateSwitchDisplayEvent(ControlUi::SwitchLed * led, DWORD color, DWORD frameHighlightColor, int offset) : 
		ControlUiEvent(User),
		mLed(led),
		mColor(color),
		mFrameHighlightColor(frameHighlightColor)
	{
		if (mColor && offset)
			ScaleLedColorForSwitchDisplay(offset);
	}

	virtual void exec() override
	{
		QPalette pal;
		pal.setColor(QPalette::Window, mColor);
		pal.setColor(QPalette::Light, mColor);
		pal.setColor(QPalette::Dark, mFrameHighlightColor);
		mLed->setPalette(pal);
	}

	void ScaleLedColorForSwitchDisplay(int offset)
	{
		const BYTE r1 = GetRValue(mColor);
		const int kReducedOffsetCutoffVal = 10;
		const int kReducedOffset = offset / (offset > 100 ? 4 : 2);
		BYTE r2 = r1;
		if (r2)
		{
			r2 = r2 + (r2 < kReducedOffsetCutoffVal ? kReducedOffset : offset);
			if (r1 > r2)
				r2 = 0xff;
		}

		const BYTE g1 = GetGValue(mColor);
		BYTE g2 = g1;
		if (g2)
		{
			g2 = g2 + (g2 < kReducedOffsetCutoffVal ? kReducedOffset : offset);
			if (g1 > g2)
				g2 = 0xff;
		}

		const BYTE b1 = GetBValue(mColor);
		BYTE b2 = b1;
		if (b2)
		{
			b2 = b2 + (b2 < kReducedOffsetCutoffVal ? kReducedOffset : offset);
			if (b1 > b2)
				b2 = 0xff;
		}

		mColor = RGB(r2, g2, b2);
	}
};

void
ControlUi::SetSwitchDisplay(int switchNumber, 
							unsigned int color)
{
	if (!mSwitchLedUpdateEnabled)
		return;

	ForceSwitchDisplay(switchNumber, color);
}

void
ControlUi::TurnOffSwitchDisplay(int switchNumber)
{
	if (!mSwitchLedUpdateEnabled)
		return;

	ForceSwitchDisplay(switchNumber, 0);
}

void
ControlUi::ForceSwitchDisplay(int switchNumber, 
							  unsigned int color)
{
	_ASSERTE(switchNumber < kMaxButtons);
	const bool kColorIsPreset = color & 0x80000000;

	if (mHardwareUi)
	{
		byte row, col;
		if (RowColFromSwitchNumber(switchNumber, row, col))
		{
			if (kColorIsPreset)
				mHardwareUi->EnableLedPreset(row, col, color & 0xff);
			else
				mHardwareUi->EnableLed(row, col, color);
		}
	}

	if (!mLeds[switchNumber] || !mLeds[switchNumber]->isEnabled())
		return;

	if (kColorIsPreset)
	{
		color &= 0xff;
		if (color > 31)
			color = 0;
		color = mLedConfig.mPresetColors[color];
	}

	QCoreApplication::postEvent(this, 
		new UpdateSwitchDisplayEvent(mLeds[switchNumber], 
			color ? color : mLedConfig.mOffColor,
			mFrameHighlightColor, 
			color ? mLedConfig.mLedColorOffset : 0));
}

void
ControlUi::DimSwitchDisplay(int switchNumber, 
							unsigned int ledColor)
{
	if (!mSwitchLedUpdateEnabled)
		return;

	_ASSERTE(switchNumber < kMaxButtons);
	_ASSERTE(ledColor);
	const bool kColorIsPreset = ledColor & 0x80000000;

	if (mHardwareUi)
	{
		byte row, col;
		if (RowColFromSwitchNumber(switchNumber, row, col))
		{
			if (kColorIsPreset)
				mHardwareUi->EnableLedPreset(row, col, ledColor & 0xff);
			else
				mHardwareUi->EnableLed(row, col, ledColor);
		}
	}

	if (!mLeds[switchNumber] || !mLeds[switchNumber]->isEnabled())
		return;

	if (kColorIsPreset)
	{
		ledColor &= 0xff;
		if (ledColor > 31)
			ledColor = 0;
		ledColor = mLedConfig.mPresetColors[ledColor];
	}

	QCoreApplication::postEvent(this, 
		new UpdateSwitchDisplayEvent(mLeds[switchNumber], 
			ledColor, 
			mFrameHighlightColor, 
			ledColor ? mLedConfig.mLedColorOffset : 0));
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

	virtual void exec() override
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
ControlUi::SetIndicatorThreadSafe(bool isOn, PatchPtr patch, int time)
{
	// ClearIndicatorTimer
	// --------------------------------------------------------------------
	// class used to handle LED state change on UI thread (via postEvent)
	//
	class ClearIndicatorTimer : public QEvent
	{
		int mTime;
		ISwitchDisplay * mSwitchDisplay;
		PatchPtr mPatch;
		bool mOn;

	public:
		ClearIndicatorTimer(bool isOn, ISwitchDisplay * switchDisplay, PatchPtr patch, int time) :
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
				QTimer::singleShot(mTime, clr, &SetIndicatorTimerCallback::TimerFired);
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

void
ControlUi::UpdatePresetColors(std::array<unsigned int, 32> &presetColors)
{
	mLedConfig.mPresetColors.swap(presetColors);
}

void
ControlUi::SendPresetColorsToMonome()
{
	if (!mHardwareUi)
		return;

	int idx = 0;
	for (const auto& it : mLedConfig.mPresetColors)
		mHardwareUi->UpdatePreset(idx++, it);
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
							  int vOffset, 
							  int hOffset,
							  unsigned int offColor,
							  int uiColorOffset)
{
	mLedConfig.mWidth = width;
	mLedConfig.mHeight = height;
	mLedConfig.mOffColor = offColor;
	mLedConfig.mVoffset = vOffset;
	mLedConfig.mHoffset = hOffset;
	mLedConfig.mLedColorOffset = uiColorOffset;

	// old auto-dim logic
// 	float onOpacity = 0.25f;
// 	float offOpacity = 1.0f - onOpacity;
// 	BYTE R = (BYTE)((float)GetRValue(offColor) * offOpacity + (float)GetRValue(onColor) * onOpacity);
// 	BYTE G = (BYTE)((float)GetGValue(offColor) * offOpacity + (float)GetGValue(onColor) * onOpacity);
// 	BYTE B = (BYTE)((float)GetBValue(offColor) * offOpacity + (float)GetBValue(onColor) * onOpacity);
// 	mLedConfig.mDimColor = RGB(R, G, B);
}

void
ControlUi::CreateSwitchLed(int id, 
						   int top, 
						   int left)
{
	if (mGrid)
		return;

	CreateSwitchLed(id);

	mLeds[id]->move(left + mLedConfig.mHoffset, top + mLedConfig.mVoffset);
}

void
ControlUi::CreateSwitchLed(int id)
{
	SwitchLed * curSwitchLed = new SwitchLed(this);
//	curSwitchLed->SetShape(ID_SHAPE_SQUARE);
//	curSwitchLed->SetColor(mLedConfig.mOnColor, mLedConfig.mOffColor);

	curSwitchLed->setFrameShape(QFrame::Panel);
	curSwitchLed->setFrameShadow(QFrame::Sunken);
	curSwitchLed->setAutoFillBackground(true);
	curSwitchLed->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	curSwitchLed->setMinimumSize(mLedConfig.mWidth, mLedConfig.mHeight);
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
						   int vOffset, 
						   int hOffset,
						   const std::string & fontName, 
						   int fontHeight, 
						   bool bold, 
						   unsigned int fgColor)
{
	mSwitchConfig.mWidth = width ? width : 75;
	mSwitchConfig.mHeight = height ? height : 25;
	mSwitchConfig.mVoffset = vOffset;
	mSwitchConfig.mHoffset = hOffset;
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
	if (mGrid)
		return;

	CreateSwitch(id, label);

	mSwitches[id]->move(left + mSwitchConfig.mHoffset, top + mSwitchConfig.mVoffset);
}

void
ControlUi::CreateSwitch(int id, 
						const std::string & label)
{
	Switch * curSwitch = new Switch(label.c_str(), this);
	curSwitch->setFont(mSwitchButtonFont);

#if defined(Q_OS_WIN)
	GESTURECONFIG config;
	config.dwID = 0;
	config.dwWant = 0;
	config.dwBlock = GC_ALLGESTURES;
	SetGestureConfig((HWND)curSwitch->winId(), 0, 1, &config, sizeof(config));
#endif

	curSwitch->setFlat(true);
	QPalette pal;
	pal.setColor(QPalette::Button, mFrameHighlightColor);
	pal.setColor(QPalette::Dark, mFrameHighlightColor);
	pal.setColor(QPalette::Light, mFrameHighlightColor);
	if (mSwitchConfig.mFgColor != -1)
		pal.setColor(QPalette::ButtonText, mSwitchConfig.mFgColor);
	curSwitch->setPalette(pal);
	curSwitch->setAutoFillBackground(true);
	curSwitch->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	curSwitch->setMinimumSize(mSwitchConfig.mWidth, mSwitchConfig.mHeight);
	curSwitch->resize(mSwitchConfig.mWidth, mSwitchConfig.mHeight);

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

	connect(curSwitch, &Switch::pressed, this, GetUiButtonPressedMember(id));
	connect(curSwitch, &Switch::released, this, GetUiButtonReleasedMember(id));

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
									  int vOffset, 
									  int hOffset,
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
	mSwitchTextDisplayConfig.mVoffset = vOffset;
	mSwitchTextDisplayConfig.mHoffset = hOffset;
}

void
ControlUi::CreateSwitchTextDisplay(int id, 
								   int top, 
								   int left)
{
	if (mGrid)
		return;

	CreateSwitchTextDisplay(id, top, left, mSwitchTextDisplayConfig.mWidth, mSwitchTextDisplayConfig.mHeight);
}

void
ControlUi::CreateSwitchTextDisplay(int id, 
								   int top, 
								   int left,
								   int width)
{
	if (mGrid)
		return;

	CreateSwitchTextDisplay(id, top, left, width, mSwitchTextDisplayConfig.mHeight);
}


void
ControlUi::CreateSwitchTextDisplay(int id, 
								   int top, 
								   int left,
								   int width,
								   int height)
{
	if (mGrid)
		return;

	CreateSwitchTextDisplay(id);

	mSwitchTextDisplays[id]->move(left + mSwitchTextDisplayConfig.mHoffset, top + mSwitchTextDisplayConfig.mVoffset);
	mSwitchTextDisplays[id]->resize(width, height);
}

void
ControlUi::CreateSwitchTextDisplay(int id)
{
	SwitchTextDisplay * curSwitchDisplay = new SwitchTextDisplay(this);
	curSwitchDisplay->setIndent(2);
	curSwitchDisplay->setMargin(0);
	curSwitchDisplay->setFrameShape(QFrame::Panel);
	curSwitchDisplay->setFrameShadow(QFrame::Sunken);
	curSwitchDisplay->setAutoFillBackground(true);
	curSwitchDisplay->setAlignment(Qt::AlignTop | Qt::AlignLeft);

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

// https://doc.qt.io/qtforpython/overviews/stylesheet-examples.html#customizing-qscrollbar
QString sHorizontalScrollStyle = "\
	QScrollBar:horizontal { \
		border: 1px #1a1a1a; \
		background: #202020; \
		height: 10px; \
		margin: 0px 10px 0 10px; \
	} \
	QScrollBar::handle:horizontal { \
		background: #505050; \
		min-width: 10px; \
	} \
	QScrollBar::add-line:horizontal { \
		border: 1px #1a1a1a; \
		background: #3a3a3a; \
		width: 10px; \
		subcontrol-position: right; \
		subcontrol-origin: margin; \
	} \
	QScrollBar::sub-line:horizontal { \
		border: 1px #1a1a1a; \
		background: #3a3a3a; \
		width: 10px; \
		subcontrol-position: left; \
		subcontrol-origin: margin; \
	} \
	QScrollBar:left-arrow:horizontal, QScrollBar::right-arrow:horizontal { \
		border: 1px #1a1a1a; \
		width: 3px; \
		height: 3px; \
		background: #808080; \
	} \
	QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { \
		background: none; \
	}";

QString sVerticalScrollStyle = "\
	QScrollBar:vertical { \
		border: 1px #1a1a1a; \
		background: #202020; \
		width: 10px; \
		margin: 10px 0 10px 0; \
	} \
	QScrollBar::handle:vertical { \
		background: #505050; \
		min-height: 10px; \
	} \
	QScrollBar::add-line:vertical { \
		border: 1px #1a1a1a; \
		background: #3a3a3a; \
		height: 10px; \
		subcontrol-position: bottom; \
		subcontrol-origin: margin; \
	} \
	QScrollBar::sub-line:vertical { \
		border: 1px #1a1a1a; \
		background: #3a3a3a; \
		height: 10px; \
		subcontrol-position: top; \
		subcontrol-origin: margin; \
	} \
	QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical { \
		border: 1px #1a1a1a; \
		width: 3px; \
		height: 3px; \
		background: #808080; \
	} \
	QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { \
		background: none; \
	}";

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
	if (mGrid)
		return;

	CreateMainDisplay(fontName, fontHeight, bold, bgColor, fgColor);

	mMainDisplayRc.setTopLeft(QPoint(left, top));
	mMainDisplayRc.setBottomRight(QPoint(left + width, top + height));

	mMainDisplay->move(left, top);
	mMainDisplay->resize(width, height);
}

void
ControlUi::CreateMainDisplay(const std::string &fontName, 
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

	// trying to get touch to perform scroll instead of selection
	mMainDisplay->setTextInteractionFlags(Qt::NoTextInteraction);
	mMainDisplay->setAttribute(Qt::WA_AcceptTouchEvents, false);
	if (mMainDisplay->viewport())
		mMainDisplay->viewport()->setAttribute(Qt::WA_AcceptTouchEvents, false);

	QFont font(fontName.c_str(), fontHeight, bold ? QFont::Bold : QFont::Normal);
	font.setLetterSpacing(QFont::PercentageSpacing, 90);
	mMainDisplay->setFont(font);

	QPalette pal;
	pal.setColor(QPalette::Window, bgColor);
	pal.setColor(QPalette::WindowText, fgColor);
	pal.setColor(QPalette::Text, fgColor);
	pal.setColor(QPalette::Base, bgColor);
	pal.setColor(QPalette::Light, mFrameHighlightColor);
	pal.setColor(QPalette::Dark, mFrameHighlightColor);
	mMainDisplay->setPalette(pal);

	mMainDisplay->horizontalScrollBar()->setStyleSheet(sHorizontalScrollStyle);
	mMainDisplay->verticalScrollBar()->setStyleSheet(sVerticalScrollStyle);
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
	if (mGrid)
		return;

	CreateTraceDisplay(fontName, fontHeight, bold);

	mTraceDiplayRc.setTopLeft(QPoint(left, top));
	mTraceDiplayRc.setBottomRight(QPoint(left + width, top + height));
	mTraceDisplay->move(left, top);
	mTraceDisplay->resize(width, height);
}

void
ControlUi::CreateTraceDisplay(const std::string &fontName, 
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

	mTraceFont.setFamily(fontName.c_str());
	mTraceFont.setPointSize(fontHeight);
	mTraceFont.setBold(bold);
	mTraceDisplay->setFont(mTraceFont);

	QPalette pal;
	pal.setColor(QPalette::Light, mFrameHighlightColor);
	pal.setColor(QPalette::Dark, mFrameHighlightColor);
	pal.setColor(QPalette::Text, 0x00ff00);
	pal.setColor(QPalette::Base, 0x1a1a1a);
	mTraceDisplay->setPalette(pal);

	mTraceDisplay->horizontalScrollBar()->setStyleSheet(sHorizontalScrollStyle);
	mTraceDisplay->verticalScrollBar()->setStyleSheet(sVerticalScrollStyle);
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
ControlUi::EnableAutoGrid()
{
	mGrid = new QGridLayout();
	mGrid->setHorizontalSpacing(4);
	mGrid->setVerticalSpacing(4);
	mGrid->setContentsMargins(2, 2, 2, 2);
}

void
ControlUi::CreateAssemblyInGrid(int id, 
								int row, 
								int col, 
								int colSpan, 
								const std::string & label,
								bool createTextDisplay, 
								bool createSwitch, 
								bool createLed)
{
	if (!mGrid)
		return;

	if (createSwitch || createTextDisplay)
	{
		QHBoxLayout *horizLayout = new QHBoxLayout();
		if (createSwitch)
		{
			CreateSwitch(id, label);
			horizLayout->addWidget(mSwitches[id], 0, Qt::AlignLeft | Qt::AlignBottom);
		}

		if (createLed)
		{
			CreateSwitchLed(id);
			horizLayout->addSpacerItem(new QSpacerItem(20, 0, QSizePolicy::Expanding));
			horizLayout->addWidget(mLeds[id], 0, Qt::AlignLeft | Qt::AlignVCenter);
			horizLayout->addSpacerItem(new QSpacerItem(20, 0, QSizePolicy::Expanding));
		}

		QVBoxLayout *vertLayout = new QVBoxLayout();
		if (createTextDisplay)
		{
			CreateSwitchTextDisplay(id);
			// Fixed height; width ignores content size so that columns are fixed 
			// size regardless of content (but allow resize when resizing the parent window)
			mSwitchTextDisplays[id]->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
			vertLayout->addWidget(mSwitchTextDisplays[id]);
		}

		vertLayout->addLayout(horizLayout);
		mGrid->addLayout(vertLayout, row, col, 1, colSpan);
	}
	else if (createLed)
	{
		CreateSwitchLed(id);
		mGrid->addWidget(mLeds[id], row, col, 1, colSpan, Qt::AlignVCenter | Qt::AlignHCenter);
	}
}

void
ControlUi::CreateMainDisplayInGrid(int row, 
								   int col, 
								   int colSpan, 
								   const std::string & fontName, 
								   int fontHeight, 
								   bool bold, 
								   unsigned int bgColor, 
								   unsigned int fgColor,
								   int minHeight)
{
	if (!mGrid)
		return;

	CreateMainDisplay(fontName, fontHeight, bold, bgColor, fgColor);

	mMainDisplay->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	mMainDisplay->setMinimumHeight(minHeight ? minHeight : 100);

	mDisplaysGridInfo[0] = row;
	mDisplaysGridInfo[1] = col;
	mDisplaysGridInfo[2] = colSpan;
	mGrid->addWidget(mMainDisplay, row, col, 1, colSpan);
}

void
ControlUi::CreateTraceDisplayInGrid(int row, 
									int col, 
									int colSpan, 
									const std::string & fontName, 
									int fontHeight, 
									bool bold)
{
	if (!mGrid)
		return;

	CreateTraceDisplay(fontName, fontHeight, bold);

	mTraceDisplay->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	mDisplaysGridInfo[3] = row;
	mDisplaysGridInfo[4] = col;
	mDisplaysGridInfo[5] = colSpan;
	mGrid->addWidget(mTraceDisplay, row, col, 1, colSpan);
}

void
ControlUi::SetMainSize(int width, 
					   int height)
{
	mPreferredHeight = height;
	mPreferredWidth = width;
}

void
ControlUi::IncreaseMainDisplayHeight()
{
	if (!mMainDisplay)
		return;

	// when increasing, start from current actual height,
	// since user doesn't want to increment from min ht to 
	// actual ht before seeing a change.
	// starting from current height guarantees a visible change.
	auto curHt = mMainDisplay->height();
	mMainDisplay->setMinimumHeight(curHt + 10);
}

void
ControlUi::DecreaseMainDisplayHeight()
{
	if (!mMainDisplay)
		return;

	// when decreasing, start from minimum height even
	// though actual height may be larger since grid 
	// may not cause a visible change anyway.
	// a decrease of height is not guaranteed to make a visible change 
	// due to layout work.
	auto curHt = mMainDisplay->minimumHeight();
	mMainDisplay->setMinimumHeight(curHt - 10);
}

IMidiOutPtr
ControlUi::CreateMidiOut(unsigned int deviceIdx,
						 int activityIndicatorIdx, 
						 unsigned int ledColor)
{
	if (!mMidiOuts[deviceIdx])
		mMidiOuts[deviceIdx] = std::make_shared<XMidiOut>(this);

	if (activityIndicatorIdx > 0)
		mMidiOuts[deviceIdx]->SetActivityIndicator(this, activityIndicatorIdx, ledColor);

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


IMidiOutPtr
ControlUi::GetMidiOut(unsigned int deviceIdx)
{
	return mMidiOuts[deviceIdx];
}

void
ControlUi::OpenMidiOuts()
{
	for (auto & mMidiOut : mMidiOuts)
	{
		IMidiOutPtr curOut = mMidiOut.second;
		if (!curOut || curOut->IsMidiOutOpen())
			continue;

		std::strstream traceMsg;
		const unsigned int kDeviceIdx = mMidiOut.first;

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
	for (auto & mMidiOut : mMidiOuts)
	{
		IMidiOutPtr curOut = mMidiOut.second;
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
}

decltype(&ControlUi::UiButtonPressed_0)
ControlUi::GetUiButtonPressedMember(int id)
{
	if (id > 64)
	{
		_ASSERTE(!"out of range switch id");
		return nullptr;
	}

	decltype(&ControlUi::UiButtonPressed_0) members[65]
	{
		&ControlUi::UiButtonPressed_0,
		&ControlUi::UiButtonPressed_1,
		&ControlUi::UiButtonPressed_2,
		&ControlUi::UiButtonPressed_3,
		&ControlUi::UiButtonPressed_4,
		&ControlUi::UiButtonPressed_5,
		&ControlUi::UiButtonPressed_6,
		&ControlUi::UiButtonPressed_7,
		&ControlUi::UiButtonPressed_8,
		&ControlUi::UiButtonPressed_9,
		&ControlUi::UiButtonPressed_10,
		&ControlUi::UiButtonPressed_11,
		&ControlUi::UiButtonPressed_12,
		&ControlUi::UiButtonPressed_13,
		&ControlUi::UiButtonPressed_14,
		&ControlUi::UiButtonPressed_15,
		&ControlUi::UiButtonPressed_16,
		&ControlUi::UiButtonPressed_17,
		&ControlUi::UiButtonPressed_18,
		&ControlUi::UiButtonPressed_19,
		&ControlUi::UiButtonPressed_20,
		&ControlUi::UiButtonPressed_21,
		&ControlUi::UiButtonPressed_22,
		&ControlUi::UiButtonPressed_23,
		&ControlUi::UiButtonPressed_24,
		&ControlUi::UiButtonPressed_25,
		&ControlUi::UiButtonPressed_26,
		&ControlUi::UiButtonPressed_27,
		&ControlUi::UiButtonPressed_28,
		&ControlUi::UiButtonPressed_29,
		&ControlUi::UiButtonPressed_30,
		&ControlUi::UiButtonPressed_31,
		&ControlUi::UiButtonPressed_32,
		&ControlUi::UiButtonPressed_33,
		&ControlUi::UiButtonPressed_34,
		&ControlUi::UiButtonPressed_35,
		&ControlUi::UiButtonPressed_36,
		&ControlUi::UiButtonPressed_37,
		&ControlUi::UiButtonPressed_38,
		&ControlUi::UiButtonPressed_39,
		&ControlUi::UiButtonPressed_40,
		&ControlUi::UiButtonPressed_41,
		&ControlUi::UiButtonPressed_42,
		&ControlUi::UiButtonPressed_43,
		&ControlUi::UiButtonPressed_44,
		&ControlUi::UiButtonPressed_45,
		&ControlUi::UiButtonPressed_46,
		&ControlUi::UiButtonPressed_47,
		&ControlUi::UiButtonPressed_48,
		&ControlUi::UiButtonPressed_49,
		&ControlUi::UiButtonPressed_50,
		&ControlUi::UiButtonPressed_51,
		&ControlUi::UiButtonPressed_52,
		&ControlUi::UiButtonPressed_53,
		&ControlUi::UiButtonPressed_54,
		&ControlUi::UiButtonPressed_55,
		&ControlUi::UiButtonPressed_56,
		&ControlUi::UiButtonPressed_57,
		&ControlUi::UiButtonPressed_58,
		&ControlUi::UiButtonPressed_59,
		&ControlUi::UiButtonPressed_60,
		&ControlUi::UiButtonPressed_61,
		&ControlUi::UiButtonPressed_62,
		&ControlUi::UiButtonPressed_63,
		&ControlUi::UiButtonPressed_64
	};

	return members[id];
}

decltype(&ControlUi::UiButtonReleased_0)
ControlUi::GetUiButtonReleasedMember(int id)
{
	if (id > 64)
	{
		_ASSERTE(!"out of range switch id");
		return nullptr;
	}

	decltype(&ControlUi::UiButtonReleased_0) members[65]
	{
		&ControlUi::UiButtonReleased_0,
		&ControlUi::UiButtonReleased_1,
		&ControlUi::UiButtonReleased_2,
		&ControlUi::UiButtonReleased_3,
		&ControlUi::UiButtonReleased_4,
		&ControlUi::UiButtonReleased_5,
		&ControlUi::UiButtonReleased_6,
		&ControlUi::UiButtonReleased_7,
		&ControlUi::UiButtonReleased_8,
		&ControlUi::UiButtonReleased_9,
		&ControlUi::UiButtonReleased_10,
		&ControlUi::UiButtonReleased_11,
		&ControlUi::UiButtonReleased_12,
		&ControlUi::UiButtonReleased_13,
		&ControlUi::UiButtonReleased_14,
		&ControlUi::UiButtonReleased_15,
		&ControlUi::UiButtonReleased_16,
		&ControlUi::UiButtonReleased_17,
		&ControlUi::UiButtonReleased_18,
		&ControlUi::UiButtonReleased_19,
		&ControlUi::UiButtonReleased_20,
		&ControlUi::UiButtonReleased_21,
		&ControlUi::UiButtonReleased_22,
		&ControlUi::UiButtonReleased_23,
		&ControlUi::UiButtonReleased_24,
		&ControlUi::UiButtonReleased_25,
		&ControlUi::UiButtonReleased_26,
		&ControlUi::UiButtonReleased_27,
		&ControlUi::UiButtonReleased_28,
		&ControlUi::UiButtonReleased_29,
		&ControlUi::UiButtonReleased_30,
		&ControlUi::UiButtonReleased_31,
		&ControlUi::UiButtonReleased_32,
		&ControlUi::UiButtonReleased_33,
		&ControlUi::UiButtonReleased_34,
		&ControlUi::UiButtonReleased_35,
		&ControlUi::UiButtonReleased_36,
		&ControlUi::UiButtonReleased_37,
		&ControlUi::UiButtonReleased_38,
		&ControlUi::UiButtonReleased_39,
		&ControlUi::UiButtonReleased_40,
		&ControlUi::UiButtonReleased_41,
		&ControlUi::UiButtonReleased_42,
		&ControlUi::UiButtonReleased_43,
		&ControlUi::UiButtonReleased_44,
		&ControlUi::UiButtonReleased_45,
		&ControlUi::UiButtonReleased_46,
		&ControlUi::UiButtonReleased_47,
		&ControlUi::UiButtonReleased_48,
		&ControlUi::UiButtonReleased_49,
		&ControlUi::UiButtonReleased_50,
		&ControlUi::UiButtonReleased_51,
		&ControlUi::UiButtonReleased_52,
		&ControlUi::UiButtonReleased_53,
		&ControlUi::UiButtonReleased_54,
		&ControlUi::UiButtonReleased_55,
		&ControlUi::UiButtonReleased_56,
		&ControlUi::UiButtonReleased_57,
		&ControlUi::UiButtonReleased_58,
		&ControlUi::UiButtonReleased_59,
		&ControlUi::UiButtonReleased_60,
		&ControlUi::UiButtonReleased_61,
		&ControlUi::UiButtonReleased_62,
		&ControlUi::UiButtonReleased_63,
		&ControlUi::UiButtonReleased_64
	};

	return members[id];
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
		mHardwareUi->Unsubscribe(mEngine.get());
		mHardwareUi->Unsubscribe(this);
		for (int idx = 0; idx < kPorts; ++idx)
			adcEnables[idx] = mHardwareUi->IsAdcEnabled(idx);
		delete mHardwareUi;
		mHardwareUi = nullptr;
	}

	LoadMonome(false);

	if (mHardwareUi)
	{
		SendPresetColorsToMonome();
		for (int idx = 0; idx < kPorts; ++idx)
			mHardwareUi->EnableAdc(idx, adcEnables[idx]);

		mHardwareUi->Subscribe(this);
		mHardwareUi->Subscribe(mEngine.get());
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
	if (mGrid)
	{
		// with auto grid layout, toggle of trace is dependent upon trace and main being in same row
		if (mDisplaysGridInfo[0] == mDisplaysGridInfo[3])
		{
			if (mTraceDisplay->isVisible())
			{
				mGrid->removeWidget(mMainDisplay);
				mTraceDisplay->hide();
				mGrid->addWidget(mMainDisplay, mDisplaysGridInfo[0], mDisplaysGridInfo[1], 1, mDisplaysGridInfo[2] + mDisplaysGridInfo[5]);
			}
			else
			{
				mGrid->removeWidget(mMainDisplay);
				mGrid->addWidget(mMainDisplay, mDisplaysGridInfo[0], mDisplaysGridInfo[1], 1, mDisplaysGridInfo[2]);
				mTraceDisplay->show();
			}
		}
	}
	else
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
ControlUi::TestLeds(int testPattern)
{
	if (0 == testPattern)
	{
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		int row, col, switchNumber;

		// turn all on
		for (row = 0; row < kMaxRows; ++row)
		{
			for (col = 0; col < kMaxCols; ++col)
				SetSwitchDisplay((row * kMaxCols) + col, 0x80000000);
		}

		QApplication::processEvents();
		QApplication::processEvents();
		SLEEP(750);

		// turn all off
		for (row = 0; row < kMaxRows; ++row)
		{
			for (col = 0; col < kMaxCols; ++col)
				TurnOffSwitchDisplay((row * kMaxCols) + col);
		}

		// walk rows
		for (row = 0; row < kMaxRows; ++row)
		{
			for (col = 0; col < kMaxCols; ++col)
			{
				switchNumber = mRowColToSwitchNumber[(row << 16) | col];
				if (switchNumber || (!row && !col)) // assume only row0, col0 can be switchNumber 0
					SetSwitchDisplay(switchNumber, 0x80000000);
			}
			QApplication::processEvents();
			QApplication::processEvents();
			SLEEP(200);
			for (col = 0; col < kMaxCols; ++col)
			{
				switchNumber = mRowColToSwitchNumber[(row << 16) | col];
				if (switchNumber || (!row && !col))
					TurnOffSwitchDisplay(switchNumber);
			}
		}

		// walk columns
		for (col = 0; col < kMaxCols; ++col)
		{
			for (row = 0; row < kMaxRows; ++row)
			{
				switchNumber = mRowColToSwitchNumber[(row << 16) | col];
				if (switchNumber || (!row && !col))
					SetSwitchDisplay(switchNumber, 0x80000000);
			}
			QApplication::processEvents();
			QApplication::processEvents();
			SLEEP(200);
			for (row = 0; row < kMaxRows; ++row)
			{
				switchNumber = mRowColToSwitchNumber[(row << 16) | col];
				if (switchNumber || (!row && !col))
					TurnOffSwitchDisplay(switchNumber);
			}
		}

		// turn all on
		for (row = 0; row < kMaxRows; ++row)
		{
			for (col = 0; col < kMaxCols; ++col)
				SetSwitchDisplay((row * kMaxCols) + col, 0x80000000);
		}

		QApplication::processEvents();
		QApplication::processEvents();
		SLEEP(750);

		// turn all off
		for (row = 0; row < kMaxRows; ++row)
		{
			for (col = 0; col < kMaxCols; ++col)
				TurnOffSwitchDisplay((row * kMaxCols) + col);
		}

		QApplication::restoreOverrideCursor();
	}
	else if (1 == testPattern)
	{
		// show color presets, 15 at a time
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		const int kPresets = mLedConfig.mPresetColors.size();
		for (int preset = 0; preset < kPresets; )
		{
			int displayed = 0;
			for (int switchNumber = 0; switchNumber < 15 && preset < kPresets; ++switchNumber)
			{
				byte row, col;
				if (RowColFromSwitchNumber(switchNumber, row, col))
				{
					if (mLedConfig.mPresetColors[preset++])
					{
						SetSwitchDisplay(switchNumber, 0x80000000 | (preset - 1));
						++displayed;
					}
				}
			}

			QApplication::processEvents();
			QApplication::processEvents();
			if (displayed)
				SLEEP(displayed < 5 ? 2000 : 5000);

			// can't use row commands on IMonome since we need to update GUI also
			for (int row = 0; row < kMaxRows; ++row)
			{
				for (int col = 0; col < kMaxCols; ++col)
					TurnOffSwitchDisplay((row * kMaxCols) + col);
			}

			QApplication::processEvents();
			QApplication::processEvents();
			SLEEP(100);
		}

		QApplication::restoreOverrideCursor();
	}
	else if (mHardwareUi)
		mHardwareUi->TestLed(testPattern);
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
	connect(mTimeDisplayTimer, &QTimer::timeout, this, &ControlUi::DisplayTime);
	mTimeDisplayTimer->start(100);
}

void
ControlUi::DisplayTime()
{
	if (mDisplayTime)
	{
		if (mMainDisplay)
		{
			const std::string msg(mApp->GetElapsedTimeStr());
			TextOut(msg);
		}
	}
	else
	{
		mTimeDisplayTimer->stop();
		disconnect(mTimeDisplayTimer, &QTimer::timeout, this, &ControlUi::DisplayTime);
	}
}

void
ControlUi::StopTimer()
{
	mDisplayTime = false;
}

void
ControlUi::ExitEventFired()
{
	Unload();

	// exit application
	MainTrollWindow *wnd = dynamic_cast<MainTrollWindow *>(mParent);
	if (nullptr != wnd)
		wnd->close();
}

IMidiInPtr
ControlUi::GetMidiIn(unsigned int deviceIdx)
{
	return mMidiIns[deviceIdx];
}

void
ControlUi::CloseMidiIns()
{
	for (auto & mMidiIn : mMidiIns)
	{
		IMidiInPtr curIn = mMidiIn.second;
		if (curIn && curIn->IsMidiInOpen())
			curIn->CloseMidiIn();
	}
}

void
ControlUi::OpenMidiIns()
{
	for (auto & mMidiIn : mMidiIns)
	{
		IMidiInPtr curIn = mMidiIn.second;
		if (!curIn || curIn->IsMidiInOpen())
			continue;

		std::strstream traceMsg;
		const unsigned int kDeviceIdx = mMidiIn.first;

		if (curIn->OpenMidiIn(kDeviceIdx))
			traceMsg << "Opened MIDI in " << kDeviceIdx << " " << curIn->GetMidiInDeviceName(kDeviceIdx) << std::endl << std::ends;
		else
			traceMsg << "Failed to open MIDI in " << kDeviceIdx << std::endl << std::ends;

		Trace(traceMsg.str());
	}
}

IMidiInPtr
ControlUi::CreateMidiIn(unsigned int deviceIdx)
{
#ifdef USE_MIDI_IN
	if (!mMidiIns[deviceIdx])
		mMidiIns[deviceIdx] = std::make_shared<XMidiIn>(this);

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
	for (auto & curOut : mMidiOuts)
		if (curOut.second->SuspendMidiOut())
			anySuspended = true;

	for (auto & curIn : mMidiIns)
		if (curIn.second->SuspendMidiIn())
			anySuspended = true;

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
	for (auto & curOut : mMidiOuts)
		if (!curOut.second->ResumeMidiOut())
			allResumed = false;

	for (auto & curIn : mMidiIns)
		if (!curIn.second->ResumeMidiIn())
			allResumed = false;

	if (allResumed)
	{
		std::strstream msg;
		msg << "Resumed MIDI connections" << std::endl << std::ends;
		Trace(msg.str());
	}

	return allResumed;
}
