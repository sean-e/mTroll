/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2009 Sean Echevarria
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

#ifndef ExpressionPedals_h__
#define ExpressionPedals_h__

#include <algorithm>
#include <functional>
#include "IMidiOut.h"
#include "IPatchCommand.h"

class IMainDisplay;


struct PedalCalibration
{
	enum {MaxAdcVal = 1023};

	PedalCalibration() : 
		mMinAdcVal(0),
		mMaxAdcVal(MaxAdcVal)
	{ }

	void Init(int minVal, int maxVal)
	{
		mMinAdcVal = minVal;
		mMaxAdcVal = maxVal;
	}

	int		mMinAdcVal;
	int		mMaxAdcVal;
};


struct PedalToggle
{
	// used for setup
	bool				mToggleIsEnabled;
	int					mActiveZoneSize;	
	int					mDeadzoneSize;

	// runtime values calculated during pedal calibration
	int					mMinActivateAdcVal;			// bottom of active zone that sends ccs and may result in exec of ON command
	int					mMaxActivateAdcVal;			// top of active zone that sends ccs and may result in exec of ON command
	int					mMinDeactivateAdcVal;		// bottom of zone that execs OFF command
	int					mMaxDeactivateAdcVal;		// top of zone that execs OFF command

	// runtime state
	PatchCommands		mToggleOn, mToggleOff;
	bool				mToggleIsOn;

	PedalToggle() :
		mToggleIsEnabled(false),
		mToggleIsOn(false),
		mActiveZoneSize(0),
		mDeadzoneSize(0),
		mMinActivateAdcVal(0),
		mMinDeactivateAdcVal(0),
		mMaxActivateAdcVal(0),
		mMaxDeactivateAdcVal(0)
	{
	}

	~PedalToggle()
	{
		ClearCommands();
	}

	void Init(int zoneSize, int deadzoneSize, PatchCommands & onCmds, PatchCommands & offCmds)
	{
		mToggleIsEnabled = true;
		ClearCommands();
		mToggleIsOn = false;
		mActiveZoneSize = zoneSize;
		mDeadzoneSize = deadzoneSize;
		mToggleOn.swap(onCmds);
		mToggleOff.swap(offCmds);
	}

	bool IsInActivationZone(int adcVal) const
	{
		if (adcVal >= mMinActivateAdcVal && adcVal <= mMaxActivateAdcVal)
			return true;
		return false;
	}

	bool IsInDeactivationZone(int adcVal) const
	{
		if (adcVal >= mMinDeactivateAdcVal && adcVal <= mMaxDeactivateAdcVal)
			return true;
		return false;
	}

	bool Activate()
	{
		if (/*!mToggleIsEnabled ||*/ mToggleIsOn)
			return false;

		std::for_each(mToggleOn.begin(), mToggleOn.end(), std::mem_fun(&IPatchCommand::Exec));
		mToggleIsOn = true;
		return true;
	}

	bool Deactivate()
	{
		if (/*!mToggleIsEnabled ||*/ !mToggleIsOn)
			return false;

		std::for_each(mToggleOff.begin(), mToggleOff.end(), std::mem_fun(&IPatchCommand::Exec));
		mToggleIsOn = false;
		return true;
	}

private:
	void ClearCommands();
};


struct BottomToggle : public PedalToggle
{
	bool IsInDeadzone(int adcVal) const 
	{ 
		if (adcVal < mMinActivateAdcVal && adcVal > mMaxDeactivateAdcVal)
			return true;
		return false; 
	}
};


struct TopToggle : public PedalToggle
{
	bool IsInDeadzone(int adcVal) const 
	{ 
		if (adcVal > mMinActivateAdcVal && adcVal < mMaxDeactivateAdcVal)
			return true;
		return false; 
	}
};


class ExpressionControl
{
public:
	ExpressionControl() : 
		mEnabled(false), 
		mMinAdcVal(0), 
		mMaxAdcVal(PedalCalibration::MaxAdcVal), 
		mActiveAdcRangeStart(0),
		mActiveAdcRangeEnd(PedalCalibration::MaxAdcVal),
		mAdcValRange(PedalCalibration::MaxAdcVal)
	{ }

	void Init(bool invert, 
			  byte channel, 
			  byte controlNumber, 
			  int minVal, 
			  int maxVal,
			  bool doubleByte);

	void InitToggle(int toggle, 
					int zoneSize, 
					int deadzoneSize, 
					PatchCommands & onCmds, 
					PatchCommands & offCmds)
	{
		if (toggle)
			mTopToggle.Init(zoneSize, deadzoneSize, onCmds, offCmds);
		else
			mBottomToggle.Init(zoneSize, deadzoneSize, onCmds, offCmds);
	}

	void Calibrate(const PedalCalibration & calibrationSetting);
	void AdcValueChange(IMainDisplay * mainDisplay, IMidiOut * midiOut, int newVal);
	void Refire(IMainDisplay * mainDisplay, IMidiOut * midiOut);

private:
	bool				mEnabled;
	bool				mInverted;
	bool				mIsDoubleByte;
	byte				mChannel;
	byte				mControlNumber;
	int					mMinCcVal;
	int					mMaxCcVal;
	int					mCcValRange;
	// 4 bytes used for single byte controllers
	// 5 bytes used for double byte controllers
	// each get one extra byte to reduce adc jitter
	byte				mMidiData[5];

	int					mMinAdcVal;
	int					mMaxAdcVal;
	int					mAdcValRange;
	int					mActiveAdcRangeStart;
	int					mActiveAdcRangeEnd;
	TopToggle			mTopToggle;
	BottomToggle		mBottomToggle;
};


class ExpressionPedal
{
	enum {ccsPerPedals = 2};

public:
	ExpressionPedal() { }

	void Init(int idx, 
			  bool invert, 
			  byte channel, 
			  byte controlNumber, 
			  int minVal, 
			  int maxVal,
			  bool doubleByte)
	{
		_ASSERTE(idx < ccsPerPedals);
		mPedalControlData[idx].Init(invert, channel, controlNumber, minVal, maxVal, doubleByte);
	}

	void InitToggle(int idx,
					int toggle, 
					int zoneSize, 
					int deadzoneSize, 
					PatchCommands & onCmds, 
					PatchCommands & offCmds)
	{
		_ASSERTE(idx < ccsPerPedals);
		mPedalControlData[idx].InitToggle(toggle, zoneSize, deadzoneSize, onCmds, offCmds);
	}

	void Calibrate(const PedalCalibration & calibrationSetting)
	{
		mPedalControlData[0].Calibrate(calibrationSetting);
		mPedalControlData[1].Calibrate(calibrationSetting);
	}

	void AdcValueChange(IMainDisplay * mainDisplay, IMidiOut * midiOut, int newVal)
	{
		mPedalControlData[0].AdcValueChange(mainDisplay, midiOut, newVal);
		mPedalControlData[1].AdcValueChange(mainDisplay, midiOut, newVal);
	}

	void Refire(IMainDisplay * mainDisplay, IMidiOut * midiOut)
	{
		mPedalControlData[0].Refire(mainDisplay, midiOut);
		mPedalControlData[1].Refire(mainDisplay, midiOut);
	}

private:
	ExpressionControl	mPedalControlData[ccsPerPedals];
};


class ExpressionPedals
{
public:
	enum {PedalCount = 4};

	ExpressionPedals(IMidiOut * midiOut = NULL) : mHasAnyNondefault(false), mMidiOut(midiOut)
	{
		int idx;
		for (idx = 0; idx < PedalCount; ++idx)
			mGlobalEnables[idx] = true;
		for (idx = 0; idx < PedalCount; ++idx)
			mPedalEnables[idx] = false;
	}

	bool HasAnySettings() const { return mHasAnyNondefault; }

	void EnableGlobal(int pedal, bool enable)
	{
		_ASSERTE(pedal < PedalCount);
		mGlobalEnables[pedal] = enable;
		mHasAnyNondefault = true;
	}

	void InitMidiOut(IMidiOut * midiOut) {mMidiOut = midiOut;}

	void Init(int pedal, 
			  int idx, 
			  bool invert, 
			  byte channel, 
			  byte controlNumber, 
			  int minVal, 
			  int maxVal,
			  bool doubleByte)
	{
		_ASSERTE(pedal < PedalCount);
		mPedals[pedal].Init(idx, invert, channel, controlNumber, minVal, maxVal, doubleByte);
		mPedalEnables[pedal] = true;
		mHasAnyNondefault = true;
	}

	void InitToggle(int pedal,
					int idx,
					int toggle, 
					int zoneSize, 
					int deadzoneSize, 
					PatchCommands & onCmds, 
					PatchCommands & offCmds)
	{
		_ASSERTE(pedal < PedalCount);
		mPedals[pedal].InitToggle(idx, toggle, zoneSize, deadzoneSize, onCmds, offCmds);
	}

	void Calibrate(const PedalCalibration * calibrationSetting)
	{
		for (int idx = 0; idx < PedalCount; ++idx)
			mPedals[idx].Calibrate(calibrationSetting[idx]);
	}

	bool AdcValueChange(IMainDisplay * mainDisplay, 
						int pedal, 
						int newVal)
	{
		_ASSERTE(pedal < PedalCount);
		_ASSERTE(mMidiOut);
		if (mPedalEnables[pedal])
			mPedals[pedal].AdcValueChange(mainDisplay, mMidiOut, newVal);
		return mGlobalEnables[pedal];
	}

	bool Refire(IMainDisplay * mainDisplay, 
				int pedal)
	{
		_ASSERTE(pedal < PedalCount);
		_ASSERTE(mMidiOut);
		if (mPedalEnables[pedal])
			mPedals[pedal].Refire(mainDisplay, mMidiOut);
		return mGlobalEnables[pedal];
	}

private:
	bool					mHasAnyNondefault;
	IMidiOut				* mMidiOut;
	bool					mGlobalEnables[PedalCount];
	bool					mPedalEnables[PedalCount];	// true if either cc is enabled for a pedal
	ExpressionPedal			mPedals[PedalCount];
};

#endif // ExpressionPedals_h__
