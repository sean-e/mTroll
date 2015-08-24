/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2015 Sean Echevarria
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

#ifndef ControlUi_h__
#define ControlUi_h__

#include <time.h>
#include <map>
#include <QWidget>
#include <QFont>

#include "../Engine/IMainDisplay.h"
#include "../Engine/ISwitchDisplay.h"
#include "../Engine/ITraceDisplay.h"
#include "../Engine/MidiControlEngine.h"
#include "../Engine/IMidiControlUi.h"
#include "../Engine/IMidiOutGenerator.h"
#include "../Engine/IMidiInGenerator.h"
#include "../Monome40h/IMonome40hInputSubscriber.h"

#ifdef _WINDOWS
	#include "../winUtil/KeepDisplayOn.h"
	#undef TextOut		// Win A/W preprocessing hoses IMainDisplay::TextOut impl
#else
	struct KeepDisplayOn {};
#endif


class QLabel;
class QPushButton;
class QPlainTextEdit;
class QFrame;
class QTimer;
class IMonome40h;
class ITrollApplication;


class ControlUi : public QWidget, 
						 IMainDisplay, 
						 ISwitchDisplay, 
						 ITraceDisplay,
						 IMidiControlUi,
						 IMidiOutGenerator,
						 IMidiInGenerator,
						 IMonome40hSwitchSubscriber
{
	Q_OBJECT;
	friend class CreateDisplayTimeTimer;
	friend class EditTextOutEvent;
	friend class EditAppendEvent;
	friend class RestoreMainTextEvent;
public:
	ControlUi(QWidget * parent, ITrollApplication * app);
	virtual ~ControlUi();

	typedef QFrame				SwitchLed;
	typedef QLabel				SwitchTextDisplay;
	typedef QPushButton			Switch;

			void Load(const std::string & uiSettingsFile, const std::string & configSettingsFile, const bool adcOverrides[ExpressionPedals::PedalCount]);
			void Unload();
			void ToggleTraceWindow();
			void UpdateAdcs(const bool adcOverrides[ExpressionPedals::PedalCount]);
			bool EnableTimeDisplay(bool enable);
			bool SuspendMidi();
			bool ResumeMidi();

			void GetPreferredSize(int & width, int & height) const {width = mPreferredWidth; height = mPreferredHeight;}

public: // IMidiOutGenerator
	virtual IMidiOut *	CreateMidiOut(unsigned int deviceIdx, int activityIndicatorIdx);
	virtual IMidiOut *	GetMidiOut(unsigned int deviceIdx);
	virtual unsigned int GetMidiOutDeviceIndex(const std::string &deviceName);
	virtual void		OpenMidiOuts();
	virtual void		CloseMidiOuts();

public: // IMidiInGenerator
	virtual IMidiIn *	GetMidiIn(unsigned int deviceIdx);
	virtual void		CloseMidiIns();
	virtual void		OpenMidiIns();
	virtual IMidiIn *	CreateMidiIn(unsigned int deviceIdx);
	virtual unsigned int GetMidiInDeviceIndex(const std::string &deviceName);

public: // IMainDisplay
	virtual void		TextOut(const std::string & txt);
	virtual void		AppendText(const std::string & text);
	virtual void		ClearDisplay();
	virtual void		ClearTransientText();
	virtual void		TransientTextOut(const std::string & txt);
	virtual std::string GetCurrentText();

public: // ITraceDisplay
	virtual void		Trace(const std::string & txt);

public: // ISwitchDisplay
	virtual void		SetSwitchDisplay(int switchNumber, bool isOn);
	virtual void		ForceSwitchDisplay(int switchNumber, bool isOn);
	virtual void		DimSwitchDisplay(int switchNumber);
	virtual void		SetSwitchText(int switchNumber, const std::string & txt);
	virtual void		ClearSwitchText(int switchNumber);
	virtual void		InvertLeds(bool invert);
	virtual bool		IsInverted() const { return mInvertLeds; }
	virtual	void		Reconnect();
	virtual void		TestLeds();
	virtual void		SetIndicatorThreadSafe(bool isOn, Patch * patch, int time);
	virtual void		EnableDisplayUpdate(bool enable) { mSwitchLedUpdateEnabled = enable; }

public: // IMonome40hSwitchSubscriber
	virtual void		SwitchPressed(byte row, byte column);
	virtual void		SwitchReleased(byte row, byte column);

private: // IMidiControlUi
	virtual void		AddSwitchMapping(int switchNumber, int row, int col);
	virtual void		SetSwitchLedConfig(int width, int height, int vOffset, int hOffset, unsigned int onColor, unsigned int offColor);
	virtual void		CreateSwitchLed(int id, int top, int left);
	virtual void		SetSwitchConfig(int width, int height, int vOffset, int hOffset, const std::string & fontName, int fontHeight, bool bold, unsigned int fgColor);
	virtual void		CreateSwitch(int id, const std::string & label, int top, int left);
	virtual void		SetSwitchTextDisplayConfig(int width, int height, int vOffset, int hOffset, const std::string & fontName, int fontHeight, bool bold, unsigned int bgColor, unsigned int fgColor);
	virtual void		CreateSwitchTextDisplay(int id, int top, int left);
	virtual void		CreateSwitchTextDisplay(int id, int top, int left, int width);
	virtual void		CreateSwitchTextDisplay(int id, int top, int left, int width, int height);
	virtual void		CreateMainDisplay(int top, int left, int width, int height, const std::string & fontName, int fontHeight, bool bold, unsigned int bgColor, unsigned int fgColor);
	virtual void		CreateTraceDisplay(int top, int left, int width, int height, const std::string & fontName, int fontHeight, bool bold);
	virtual void		CreateStaticLabel(const std::string & label, int top, int left, int width, int height, const std::string & fontName, int fontHeight, bool bold, unsigned int bgColor, unsigned int fgColor);
	virtual void		SetMainSize(int width, int height);
	virtual void		SetHardwareLedIntensity(short brightness) { mLedIntensity = brightness; }
	virtual void		SetLedDisplayState(bool invert) { mInvertLeds = invert; }
	virtual void		SetColors(unsigned int backgroundColor, unsigned int frameHighlightColor) { mFrameHighlightColor = frameHighlightColor; mBackgroundColor = backgroundColor; }

private slots:
	void DisplayTime();

	// sigh... the one time that I would use a macro but the Qt MOC doesn't support it (or the use of tokenization)!!
	void UiButtonPressed_0() { ButtonPressed(0); }
	void UiButtonPressed_1() { ButtonPressed(1); }
	void UiButtonPressed_2() { ButtonPressed(2); }
	void UiButtonPressed_3() { ButtonPressed(3); }
	void UiButtonPressed_4() { ButtonPressed(4); }
	void UiButtonPressed_5() { ButtonPressed(5); }
	void UiButtonPressed_6() { ButtonPressed(6); }
	void UiButtonPressed_7() { ButtonPressed(7); }
	void UiButtonPressed_8() { ButtonPressed(8); }
	void UiButtonPressed_9() { ButtonPressed(9); }
	void UiButtonPressed_10() { ButtonPressed(10); }
	void UiButtonPressed_11() { ButtonPressed(11); }
	void UiButtonPressed_12() { ButtonPressed(12); }
	void UiButtonPressed_13() { ButtonPressed(13); }
	void UiButtonPressed_14() { ButtonPressed(14); }
	void UiButtonPressed_15() { ButtonPressed(15); }
	void UiButtonPressed_16() { ButtonPressed(16); }
	void UiButtonPressed_17() { ButtonPressed(17); }
	void UiButtonPressed_18() { ButtonPressed(18); }
	void UiButtonPressed_19() { ButtonPressed(19); }
	void UiButtonPressed_20() { ButtonPressed(20); }
	void UiButtonPressed_21() { ButtonPressed(21); }
	void UiButtonPressed_22() { ButtonPressed(22); }
	void UiButtonPressed_23() { ButtonPressed(23); }
	void UiButtonPressed_24() { ButtonPressed(24); }
	void UiButtonPressed_25() { ButtonPressed(25); }
	void UiButtonPressed_26() { ButtonPressed(26); }
	void UiButtonPressed_27() { ButtonPressed(27); }
	void UiButtonPressed_28() { ButtonPressed(28); }
	void UiButtonPressed_29() { ButtonPressed(29); }
	void UiButtonPressed_30() { ButtonPressed(30); }
	void UiButtonPressed_31() { ButtonPressed(31); }
	void UiButtonPressed_32() { ButtonPressed(32); }
	void UiButtonPressed_33() { ButtonPressed(33); }
	void UiButtonPressed_34() { ButtonPressed(34); }
	void UiButtonPressed_35() { ButtonPressed(35); }
	void UiButtonPressed_36() { ButtonPressed(36); }
	void UiButtonPressed_37() { ButtonPressed(37); }
	void UiButtonPressed_38() { ButtonPressed(38); }
	void UiButtonPressed_39() { ButtonPressed(39); }
	void UiButtonPressed_40() { ButtonPressed(40); }
	void UiButtonPressed_41() { ButtonPressed(41); }
	void UiButtonPressed_42() { ButtonPressed(42); }
	void UiButtonPressed_43() { ButtonPressed(43); }
	void UiButtonPressed_44() { ButtonPressed(44); }
	void UiButtonPressed_45() { ButtonPressed(45); }
	void UiButtonPressed_46() { ButtonPressed(46); }
	void UiButtonPressed_47() { ButtonPressed(47); }
	void UiButtonPressed_48() { ButtonPressed(48); }
	void UiButtonPressed_49() { ButtonPressed(49); }
	void UiButtonPressed_50() { ButtonPressed(50); }
	void UiButtonPressed_51() { ButtonPressed(51); }
	void UiButtonPressed_52() { ButtonPressed(52); }
	void UiButtonPressed_53() { ButtonPressed(53); }
	void UiButtonPressed_54() { ButtonPressed(54); }
	void UiButtonPressed_55() { ButtonPressed(55); }
	void UiButtonPressed_56() { ButtonPressed(56); }
	void UiButtonPressed_57() { ButtonPressed(57); }
	void UiButtonPressed_58() { ButtonPressed(58); }
	void UiButtonPressed_59() { ButtonPressed(59); }
	void UiButtonPressed_60() { ButtonPressed(60); }
	void UiButtonPressed_61() { ButtonPressed(61); }
	void UiButtonPressed_62() { ButtonPressed(62); }
	void UiButtonPressed_63() { ButtonPressed(63); }
	void UiButtonPressed_64() { ButtonPressed(64); }

	void UiButtonReleased_0() { ButtonReleased(0); }
	void UiButtonReleased_1() { ButtonReleased(1); }
	void UiButtonReleased_2() { ButtonReleased(2); }
	void UiButtonReleased_3() { ButtonReleased(3); }
	void UiButtonReleased_4() { ButtonReleased(4); }
	void UiButtonReleased_5() { ButtonReleased(5); }
	void UiButtonReleased_6() { ButtonReleased(6); }
	void UiButtonReleased_7() { ButtonReleased(7); }
	void UiButtonReleased_8() { ButtonReleased(8); }
	void UiButtonReleased_9() { ButtonReleased(9); }
	void UiButtonReleased_10() { ButtonReleased(10); }
	void UiButtonReleased_11() { ButtonReleased(11); }
	void UiButtonReleased_12() { ButtonReleased(12); }
	void UiButtonReleased_13() { ButtonReleased(13); }
	void UiButtonReleased_14() { ButtonReleased(14); }
	void UiButtonReleased_15() { ButtonReleased(15); }
	void UiButtonReleased_16() { ButtonReleased(16); }
	void UiButtonReleased_17() { ButtonReleased(17); }
	void UiButtonReleased_18() { ButtonReleased(18); }
	void UiButtonReleased_19() { ButtonReleased(19); }
	void UiButtonReleased_20() { ButtonReleased(20); }
	void UiButtonReleased_21() { ButtonReleased(21); }
	void UiButtonReleased_22() { ButtonReleased(22); }
	void UiButtonReleased_23() { ButtonReleased(23); }
	void UiButtonReleased_24() { ButtonReleased(24); }
	void UiButtonReleased_25() { ButtonReleased(25); }
	void UiButtonReleased_26() { ButtonReleased(26); }
	void UiButtonReleased_27() { ButtonReleased(27); }
	void UiButtonReleased_28() { ButtonReleased(28); }
	void UiButtonReleased_29() { ButtonReleased(29); }
	void UiButtonReleased_30() { ButtonReleased(30); }
	void UiButtonReleased_31() { ButtonReleased(31); }
	void UiButtonReleased_32() { ButtonReleased(32); }
	void UiButtonReleased_33() { ButtonReleased(33); }
	void UiButtonReleased_34() { ButtonReleased(34); }
	void UiButtonReleased_35() { ButtonReleased(35); }
	void UiButtonReleased_36() { ButtonReleased(36); }
	void UiButtonReleased_37() { ButtonReleased(37); }
	void UiButtonReleased_38() { ButtonReleased(38); }
	void UiButtonReleased_39() { ButtonReleased(39); }
	void UiButtonReleased_40() { ButtonReleased(40); }
	void UiButtonReleased_41() { ButtonReleased(41); }
	void UiButtonReleased_42() { ButtonReleased(42); }
	void UiButtonReleased_43() { ButtonReleased(43); }
	void UiButtonReleased_44() { ButtonReleased(44); }
	void UiButtonReleased_45() { ButtonReleased(45); }
	void UiButtonReleased_46() { ButtonReleased(46); }
	void UiButtonReleased_47() { ButtonReleased(47); }
	void UiButtonReleased_48() { ButtonReleased(48); }
	void UiButtonReleased_49() { ButtonReleased(49); }
	void UiButtonReleased_50() { ButtonReleased(50); }
	void UiButtonReleased_51() { ButtonReleased(51); }
	void UiButtonReleased_52() { ButtonReleased(52); }
	void UiButtonReleased_53() { ButtonReleased(53); }
	void UiButtonReleased_54() { ButtonReleased(54); }
	void UiButtonReleased_55() { ButtonReleased(55); }
	void UiButtonReleased_56() { ButtonReleased(56); }
	void UiButtonReleased_57() { ButtonReleased(57); }
	void UiButtonReleased_58() { ButtonReleased(58); }
	void UiButtonReleased_59() { ButtonReleased(59); }
	void UiButtonReleased_60() { ButtonReleased(60); }
	void UiButtonReleased_61() { ButtonReleased(61); }
	void UiButtonReleased_62() { ButtonReleased(62); }
	void UiButtonReleased_63() { ButtonReleased(63); }
	void UiButtonReleased_64() { ButtonReleased(64); }

private:
    virtual bool		event(QEvent *);
	void LoadUi(const std::string & uiSettingsFile);
	void LoadMonome(bool displayStartSequence);
	void LoadMidiSettings(const std::string & file, const bool adcOverrides[ExpressionPedals::PedalCount]);
	void StopTimer();
	void CreateTimeDisplayTimer();
	void ToggleTraceWindowCallback();

	void ButtonReleased(const int idx);
	void ButtonPressed(const int idx);

	void MonomeStartupSequence();
	inline bool RowColFromSwitchNumber(int ord, byte & row, byte & col)
	{
		std::map<int, int>::const_iterator it = mSwitchNumberToRowCol.find(ord);
		if (it == mSwitchNumberToRowCol.end())
			return false;
		const int rc = (*it).second;
		col = (byte)(rc & 0xff);
		row = (byte)((rc >> 16) & 0xff);
		return true;
	}
	inline int SwitchNumberFromRowCol(byte row, byte col)
	{
		return mRowColToSwitchNumber[(row << 16) | col];
	}

private:
	QWidget						* mParent;
	ITrollApplication			* mApp;
	IMonome40h					* mHardwareUi;
	MidiControlEngine			* mEngine;
	QPlainTextEdit				* mMainDisplay;
	QPlainTextEdit				* mTraceDisplay;
	std::map<int, SwitchLed*>	mLeds;
	std::map<int, SwitchTextDisplay *>		mSwitchTextDisplays;
	std::map<int, Switch *>		mSwitches;
	std::map<int, bool>			mStupidSwitchStates;
	std::map<int, int>			mSwitchNumberToRowCol;
	std::map<int, int>			mRowColToSwitchNumber;
	QFont						mSwitchButtonFont;
	QFont						mTraceFont;
	int							mPreferredHeight, mPreferredWidth;
	int							mMaxSwitchId;
	typedef std::map<unsigned int, IMidiOut*> MidiOuts;
	MidiOuts					mMidiOuts;
	typedef std::map<unsigned int, IMidiIn*> MidiIns;
	MidiIns						mMidiIns;
	int							mLedIntensity;
	byte						mLedIntensityDimmed;
	bool						mInvertLeds;
	KeepDisplayOn				* mSystemPowerOverride;
	QRect						mMainDisplayRc;
	QRect						mTraceDiplayRc;
	bool						mUserAdcSettings[ExpressionPedals::PedalCount];
	bool						mDisplayTime;
	QTimer						* mTimeDisplayTimer;
	DWORD						mBackgroundColor;
	DWORD						mFrameHighlightColor;
	QString						mMainText;
	bool						mSwitchLedUpdateEnabled;

	// workaround for double fire of pressed signal when using touch
	int							mLastUiButtonPressed;
	clock_t						mLastUiButtonEventTime;

	struct SwitchTextDisplayConfig
	{
		int						mHeight;
		int						mWidth;
		int						mVoffset;
		int						mHoffset;
		int						mFontHeight;
		QString					mFontname;
		DWORD					mFgColor;
		DWORD					mBgColor;
		bool					mBold;
	};
	SwitchTextDisplayConfig		mSwitchTextDisplayConfig;

	struct SwitchConfig
	{
		int						mHeight;
		int						mWidth;
		int						mVoffset;
		int						mHoffset;
		QString					mFontname;
		int						mFontHeight;
		bool					mBold;
		DWORD					mFgColor;
	};
	SwitchConfig				mSwitchConfig;

	struct LedConfig
	{
		int						mHeight;
		int						mWidth;
		int						mVoffset;
		int						mHoffset;
		DWORD					mOnColor;
		DWORD					mOffColor;
		DWORD					mDimColor;
	};
	LedConfig					mLedConfig;
};



// SetIndicatorTimerCallback
// ----------------------------------------------------------------------------
// Timer callback for delayed modification of LED display for a patch
//
class SetIndicatorTimerCallback : public QObject
{
	Q_OBJECT;
	ISwitchDisplay * mSwitchDisplay;
	Patch * mPatch;
	bool mOn;

public:
	SetIndicatorTimerCallback(bool on, ISwitchDisplay * switchDisplay, Patch * p) :
	  mSwitchDisplay(switchDisplay),
	  mPatch(p),
	  mOn(on)
	{
	}

private slots:
	void TimerFired();
};


#endif // ControlUi_h__
