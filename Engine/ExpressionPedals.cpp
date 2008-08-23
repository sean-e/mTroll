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

#include <string>
#include <strstream>
#include "ExpressionPedals.h"
#include "IMainDisplay.h"
#include "IMidiOut.h"


void
ExpressionControl::Init(bool invert, 
						byte channel, 
						byte controlNumber, 
						byte minVal, 
						byte maxVal)
{
	mEnabled = true;
	mInverted = invert;
	mChannel = channel;
	mControlNumber = controlNumber;
	mMinCcVal = minVal < maxVal ? minVal : maxVal;
	if (mMinCcVal < 0)
		mMinCcVal = 0;
	mMaxCcVal = maxVal > minVal ? maxVal : minVal;
	if (mMaxCcVal > 127)
		mMaxCcVal = 127;
	mCcValRange = mMaxCcVal - mMinCcVal;

	mMidiData[0] = (0xb0 | mChannel);
	mMidiData[1] = mControlNumber;
	mMidiData[2] = 0;
}

void
ExpressionControl::Calibrate(const PedalCalibration & calibrationSetting)
{
	mMinAdcVal = calibrationSetting.mMinAdcVal;
	mMaxAdcVal = calibrationSetting.mMaxAdcVal;
	mAdcValRange = mMaxAdcVal - mMinAdcVal;
}

void
ExpressionControl::AdcValueChange(IMainDisplay * mainDisplay, 
								  IMidiOut * midiOut, 
								  int newAdcVal)
{
	if (!mEnabled)
		return;

	const int cappedAdcVal = newAdcVal < mMinAdcVal ? 
			mMinAdcVal : 
			(newAdcVal > mMaxAdcVal) ? mMaxAdcVal : newAdcVal;

	// normal 127 range is 1023
	byte newCcVal = ((cappedAdcVal - mMinAdcVal) * mCcValRange) / mAdcValRange;
	if (mMinCcVal)
		newCcVal += mMinCcVal;

	if (newCcVal > mMaxCcVal)
		newCcVal = mMaxCcVal;
	else if (newCcVal < mMinCcVal)
		newCcVal = mMinCcVal;

	if (mInverted)
		newCcVal = 127 - newCcVal;

	if (mMidiData[2] == newCcVal)
		return;

	mMidiData[2] = newCcVal;

	// only fire midi indicator at top and bottom of range -
	// easier to see that top and bottom hit on controller than on pc
	const bool showStatus = newCcVal == mMinCcVal || newCcVal == mMaxCcVal;
	midiOut->MidiOut(mMidiData[0], mMidiData[1], mMidiData[2], showStatus);

	if (mainDisplay)
	{
		static bool sHadStatus = false;
		if (showStatus || sHadStatus)
		{
			std::strstream displayMsg;
			if (newCcVal == mMinCcVal)
				displayMsg << "______ min ______" << std::endl;
			else if (newCcVal == mMaxCcVal)
				displayMsg << "|||||| MAX ||||||" << std::endl;

			sHadStatus = showStatus;
			if (showStatus)
				displayMsg << "adc ch(" << (int) mChannel << "), ctrl(" << (int) mControlNumber << "): " << newAdcVal << " -> " << (int) mMidiData[2] << std::endl << std::ends;
			else
				displayMsg << std::endl << std::ends; // clear display

			mainDisplay->TextOut(displayMsg.str());
		}
	}
}
