/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2010,2014-2015,2018,2020,2023 Sean Echevarria
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

#include <memory>
#include "..\Monome40h\IMonome40hInputSubscriber.h"

class Patch;
class IMainDisplay;
class IMidiOut;
class ISwitchDisplay;
class MidiControlEngine;
class ITraceDisplay;

using PatchPtr = std::shared_ptr<Patch>;
using IMidiOutPtr = std::shared_ptr<IMidiOut>;


struct PedalCalibration
{
	enum {MaxAdcVal = 1023};

	PedalCalibration() = default;

	int		mMinAdcVal = 0;
	int		mMaxAdcVal = MaxAdcVal;
	int		mBottomToggleZoneSize = 0;
	int		mBottomToggleDeadzoneSize = 0;
	int		mTopToggleZoneSize = 0;
	int		mTopToggleDeadzoneSize = 0;
};


struct PedalToggle
{
	bool				mToggleIsEnabled = false;
	int					mTogglePatchNumber = -1;

	// runtime values calculated during pedal calibration
	int					mMinActivateAdcVal = 0;			// bottom of active zone that sends ccs and may result in exec of ON command
	int					mMaxActivateAdcVal = 0;			// top of active zone that sends ccs and may result in exec of ON command
	int					mMinDeactivateAdcVal = 0;		// bottom of zone that execs OFF command
	int					mMaxDeactivateAdcVal = 0;		// top of zone that execs OFF command

	// runtime state
	PatchPtr			mPatch;
	ISwitchDisplay		* mSwitchDisplay = nullptr;

	PedalToggle() = default;

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
		InitParams() = default;

		bool mInvert = false;
		byte mChannel = 1;
		byte mControlNumber = 1;
		int mMinVal = 0;
		int mMaxVal = 127;
		bool mDoubleByte = false;
		SweepCurve mCurve = scLinear;
		int mBottomTogglePatchNumber = -1;
		int mTopTogglePatchNumber = -1;
	};

	ExpressionControl() = default;

	void Init(int pedal, const InitParams & params);
	void InitMidiOut(IMidiOutPtr midiOut) { mMidiOut = midiOut; }

	void Calibrate(const PedalCalibration & calibrationSetting, MidiControlEngine * eng, ITraceDisplay * traceDisp);
	void AdcValueChange(IMainDisplay * mainDisplay, int newVal);
	void Refire(IMainDisplay * mainDisplay);

private:
	IMidiOutPtr			mMidiOut;
	int					mPedalNumber = 1;
	bool				mEnabled = false;
	bool				mInverted = false;
	bool				mIsDoubleByte = false;
	byte				mChannel = 1;
	byte				mControlNumber = 1;
	int					mMinCcVal = 0;
	int					mMaxCcVal = 127;
	int					mCcValRange = 0;
	SweepCurve			mSweepCurve = scLinear;
	// 4 bytes used for single byte controllers
	// 5 bytes used for double byte controllers
	// each get one extra byte to reduce adc jitter
	byte				mMidiData[5] = { 0 };

	int					mMinAdcVal = 0;
	int					mMaxAdcVal = PedalCalibration::MaxAdcVal;
	int					mAdcValRange = PedalCalibration::MaxAdcVal;
	int					mActiveAdcRangeStart = 0;
	int					mActiveAdcRangeEnd = PedalCalibration::MaxAdcVal;
	TopToggle			mTopToggle;
	BottomToggle		mBottomToggle;
};


class ExpressionPedal
{
	enum {ccsPerPedals = 2};

public:
	ExpressionPedal() { }

	void Init(int pedal,
			  int idx, 
			  const ExpressionControl::InitParams & params)
	{
		_ASSERTE(idx < ccsPerPedals);
		if (idx < ccsPerPedals)
			mPedalControlData[idx].Init(pedal, params);
	}

	void InitMidiOut(int idx, IMidiOutPtr midiOut) 
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

	ExpressionPedals(IMidiOutPtr midiOut = nullptr)
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

	void InitMidiOut(IMidiOutPtr midiOut) 
	{
		for (auto & pedal : mPedals)
		{
			pedal.InitMidiOut(0, midiOut);
			pedal.InitMidiOut(1, midiOut);
		}
	}

	void InitMidiOut(IMidiOutPtr midiOut, int pedal, int ccIdx) 
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
			mPedals[pedal].Init(pedal + 1, idx, params);
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
	bool					mHasAnyNondefault = false;
	bool					mGlobalEnables[PedalCount];
	bool					mPedalEnables[PedalCount];	// true if either cc is enabled for a pedal
	ExpressionPedal			mPedals[PedalCount];
};

#endif // ExpressionPedals_h__
