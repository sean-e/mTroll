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
	mPrevAdcVals[0] = mPrevAdcVals[1] = -1;

	mMidiData.push_back(0xb0 | mChannel);
	mMidiData.push_back(mControlNumber);
	mMidiData.push_back(0);
}

void
ExpressionControl::Calibrate(PedalCalibration & calibrationSetting)
{
	if (!mEnabled)
		return;

	// do stuff
}

void
ExpressionControl::AdcValueChange(IMainDisplay * mainDisplay, 
								  IMidiOut * midiOut, 
								  int newAdcVal)
{
	if (!mEnabled)
		return;

	if (mPrevAdcVals[0] == newAdcVal)
		return;
	if (mPrevAdcVals[1] == newAdcVal)
	{
		if (mainDisplay)
		{
			std::strstream displayMsg;
			displayMsg << "adc val repeat: " << newAdcVal << std::endl << std::ends;
			mainDisplay->TextOut(displayMsg.str());
		}
		return;
	}

	mPrevAdcVals[1] = mPrevAdcVals[0];
	mPrevAdcVals[0] = newAdcVal;

	const int kMinAdcVal = 60; // cap min 0
	const int kMaxAdcVal = 305; // 1023; // cap max 1023
	const int kAdcValRange = kMaxAdcVal - kMinAdcVal;
	const int cappedAdcVal = newAdcVal < kMinAdcVal ? 
			kMinAdcVal : 
			(newAdcVal > kMaxAdcVal) ? kMaxAdcVal : newAdcVal;

	// normal 127 range is 1023
	byte newCcVal = ((cappedAdcVal - kMinAdcVal) * mCcValRange) / kAdcValRange;
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
	midiOut->MidiOut(mMidiData);

	if (mainDisplay)
	{
		std::strstream displayMsg;
		displayMsg << "Adc " << (int) mChannel << ", " << (int) mControlNumber << ": " << newAdcVal << ", " << (int) mMidiData[2] << std::endl << std::ends;
		mainDisplay->TextOut(displayMsg.str());
	}
}
