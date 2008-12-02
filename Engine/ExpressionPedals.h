/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2008 Sean Echevarria
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

#include "IMidiOut.h"

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


class ExpressionControl
{
public:
	ExpressionControl() : 
		mEnabled(false), 
		mMinAdcVal(0), 
		mMaxAdcVal(PedalCalibration::MaxAdcVal), 
		mAdcValRange(PedalCalibration::MaxAdcVal) 
	{ }

	void Init(bool invert, 
			  byte channel, 
			  byte controlNumber, 
			  int minVal, 
			  int maxVal,
			  bool doubleByte);

	void Calibrate(const PedalCalibration & calibrationSetting);
	void AdcValueChange(IMainDisplay * mainDisplay, IMidiOut * midiOut, int newVal);

private:
	bool				mEnabled;
	bool				mInverted;
	bool				mIsDoubleByte;
	byte				mChannel;
	byte				mControlNumber;
	int					mMinCcVal;
	int					mMaxCcVal;
	int					mCcValRange;
	byte				mMidiData[4];

	int					mMinAdcVal;
	int					mMaxAdcVal;
	int					mAdcValRange;
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

private:
	bool					mHasAnyNondefault;
	IMidiOut				* mMidiOut;
	bool					mGlobalEnables[PedalCount];
	bool					mPedalEnables[PedalCount];	// true if either cc is enabled for a pedal
	ExpressionPedal			mPedals[PedalCount];
};

#endif // ExpressionPedals_h__
