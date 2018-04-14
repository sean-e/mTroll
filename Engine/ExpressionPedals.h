/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2010,2014-2015,2018 Sean Echevarria
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

#include "..\Monome40h\IMonome40hInputSubscriber.h"

class Patch;
class IMainDisplay;
class IMidiOut;
class ISwitchDisplay;
class MidiControlEngine;
class ITraceDisplay;


struct PedalCalibration
{
	enum {MaxAdcVal = 1023};

	PedalCalibration() : 
		mMinAdcVal(0),
		mMaxAdcVal(MaxAdcVal),
		mBottomToggleZoneSize(0),
		mBottomToggleDeadzoneSize(0),
		mTopToggleZoneSize(0),
		mTopToggleDeadzoneSize(0)
	{ }

	int		mMinAdcVal;
	int		mMaxAdcVal;
	int		mBottomToggleZoneSize;
	int		mBottomToggleDeadzoneSize;
	int		mTopToggleZoneSize;
	int		mTopToggleDeadzoneSize;
};


struct PedalToggle
{
	bool				mToggleIsEnabled;
	int					mTogglePatchNumber;

	// runtime values calculated during pedal calibration
	int					mMinActivateAdcVal;			// bottom of active zone that sends ccs and may result in exec of ON command
	int					mMaxActivateAdcVal;			// top of active zone that sends ccs and may result in exec of ON command
	int					mMinDeactivateAdcVal;		// bottom of zone that execs OFF command
	int					mMaxDeactivateAdcVal;		// top of zone that execs OFF command

	// runtime state
	Patch				* mPatch;
	ISwitchDisplay		* mSwitchDisplay;

	PedalToggle() :
		mToggleIsEnabled(false),
		mMinActivateAdcVal(0),
		mMinDeactivateAdcVal(0),
		mMaxActivateAdcVal(0),
		mMaxDeactivateAdcVal(0),
		mTogglePatchNumber(-1),
		mPatch(nullptr),
		mSwitchDisplay(nullptr)
	{
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

	bool Activate();
	bool Deactivate();
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
	enum SweepCurve { scLinear, scAudioLog, scShallowLog, scPseudoAudioLog, scReverseAudioLog, scReverseShallowLog, scReversePseudoAudioLog };

	struct InitParams
	{
		InitParams() :
		  mInvert(false),
		  mChannel(1),
		  mControlNumber(1),
		  mMinVal(0),
		  mMaxVal(127),
		  mDoubleByte(false),
		  mCurve(scLinear),
		  mBottomTogglePatchNumber(-1),
		  mTopTogglePatchNumber(-1)
		{ }

		bool mInvert;
		byte mChannel;
		byte mControlNumber;
		int mMinVal;
		int mMaxVal;
		bool mDoubleByte;
		SweepCurve mCurve;
		int mBottomTogglePatchNumber;
		int mTopTogglePatchNumber;
	};

	ExpressionControl() : 
		mEnabled(false), 
		mSweepCurve(scLinear),
		mMinAdcVal(0), 
		mMaxAdcVal(PedalCalibration::MaxAdcVal), 
		mActiveAdcRangeStart(0),
		mActiveAdcRangeEnd(PedalCalibration::MaxAdcVal),
		mAdcValRange(PedalCalibration::MaxAdcVal),
		mMidiOut(nullptr)
	{ }

	void Init(const InitParams & params);
	void InitMidiOut(IMidiOut * midiOut) { mMidiOut = midiOut; }

	void Calibrate(const PedalCalibration & calibrationSetting, MidiControlEngine * eng, ITraceDisplay * traceDisp);
	void AdcValueChange(IMainDisplay * mainDisplay, int newVal);
	void Refire(IMainDisplay * mainDisplay);

private:
	IMidiOut			*mMidiOut;
	bool				mEnabled;
	bool				mInverted;
	bool				mIsDoubleByte;
	byte				mChannel;
	byte				mControlNumber;
	int					mMinCcVal;
	int					mMaxCcVal;
	int					mCcValRange;
	SweepCurve			mSweepCurve;
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
			  const ExpressionControl::InitParams & params)
	{
		_ASSERTE(idx < ccsPerPedals);
		if (idx < ccsPerPedals)
			mPedalControlData[idx].Init(params);
	}

	void InitMidiOut(int idx, IMidiOut * midiOut) 
	{ 
		_ASSERTE(idx < ccsPerPedals);
		if (idx < ccsPerPedals)
			mPedalControlData[idx].InitMidiOut(midiOut); 
	}

	void Calibrate(const PedalCalibration & calibrationSetting, MidiControlEngine * eng, ITraceDisplay * traceDisp)
	{
		mPedalControlData[0].Calibrate(calibrationSetting, eng, traceDisp);
		mPedalControlData[1].Calibrate(calibrationSetting, eng, traceDisp);
	}

	void AdcValueChange(IMainDisplay * mainDisplay, int newVal)
	{
		mPedalControlData[0].AdcValueChange(mainDisplay, newVal);
		mPedalControlData[1].AdcValueChange(mainDisplay, newVal);
	}

	void Refire(IMainDisplay * mainDisplay)
	{
		mPedalControlData[0].Refire(mainDisplay);
		mPedalControlData[1].Refire(mainDisplay);
	}

private:
	ExpressionControl	mPedalControlData[ccsPerPedals];
};


class ExpressionPedals
{
public:
	enum {PedalCount = 4};

	ExpressionPedals(IMidiOut * midiOut = NULL) : mHasAnyNondefault(false)
	{
		for (auto & globalEnable : mGlobalEnables)
			globalEnable = true;
		for (auto & pedalEnable : mPedalEnables)
			pedalEnable = false;

		InitMidiOut(midiOut);
	}

	bool HasAnySettings() const { return mHasAnyNondefault; }

	void EnableGlobal(int pedal, bool enable)
	{
		_ASSERTE(pedal < PedalCount);
		if (pedal < PedalCount)
		{
			mGlobalEnables[pedal] = enable;
			mHasAnyNondefault = true;
		}
	}

	void InitMidiOut(IMidiOut * midiOut) 
	{
		for (auto & pedal : mPedals)
		{
			pedal.InitMidiOut(0, midiOut);
			pedal.InitMidiOut(1, midiOut);
		}
	}

	void InitMidiOut(IMidiOut * midiOut, int pedal, int ccIdx) 
	{
		_ASSERTE(pedal < PedalCount);
		if (pedal < PedalCount)
			mPedals[pedal].InitMidiOut(ccIdx, midiOut);
	}

	void Init(int pedal, 
			  int idx, 
			  const ExpressionControl::InitParams & params)
	{
		_ASSERTE(pedal < PedalCount);
		if (pedal < PedalCount)
		{
			mPedals[pedal].Init(idx, params);
			mPedalEnables[pedal] = true;
			mHasAnyNondefault = true;
		}
	}

	void Calibrate(const PedalCalibration * calibrationSetting, MidiControlEngine * eng, ITraceDisplay * traceDisp)
	{
		for (int idx = 0; idx < PedalCount; ++idx)
			mPedals[idx].Calibrate(calibrationSetting[idx], eng, traceDisp);
	}

	bool AdcValueChange(IMainDisplay * mainDisplay, 
						int pedal, 
						int newVal)
	{
		_ASSERTE(pedal < PedalCount);
		if (pedal < PedalCount)
		{
			if (mPedalEnables[pedal])
				mPedals[pedal].AdcValueChange(mainDisplay, newVal);
			return mGlobalEnables[pedal];
		}

		return false;
	}

	bool Refire(IMainDisplay * mainDisplay, 
				int pedal)
	{
		_ASSERTE(pedal < PedalCount);
		if (pedal < PedalCount)
		{
			if (mPedalEnables[pedal])
				mPedals[pedal].Refire(mainDisplay);
			return mGlobalEnables[pedal];
		}

		return false;
	}

private:
	bool					mHasAnyNondefault;
	bool					mGlobalEnables[PedalCount];
	bool					mPedalEnables[PedalCount];	// true if either cc is enabled for a pedal
	ExpressionPedal			mPedals[PedalCount];
};

#endif // ExpressionPedals_h__
