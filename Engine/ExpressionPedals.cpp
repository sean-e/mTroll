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

	if (mainDisplay && showStatus)
	{
		std::strstream displayMsg;
		if (newCcVal == mMinCcVal)
			displayMsg << "___ min ___   ";
		else if (newCcVal == mMaxCcVal)
			displayMsg << "||| MAX |||   ";

		displayMsg << "adc ch(" << (int) mChannel << "), ctrl(" << (int) mControlNumber << "): " << newAdcVal << " -> " << (int) mMidiData[2] << std::endl << std::ends;
		mainDisplay->TextOut(displayMsg.str());
	}
}
