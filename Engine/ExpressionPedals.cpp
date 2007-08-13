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
	mMinCcVal = minVal;
	mMaxCcVal = maxVal;
	mPrevCcVal = 255;
	mCcRange = mMaxCcVal - mMinCcVal;

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
								  int newVal)
{
	if (!mEnabled)
		return;

	mPrevCcVal = mMidiData[2];

	// TODO: support min, max and invert
	// normal 127 range is 1023
	byte newCcVal = (newVal * 127) / 1023;

	mMidiData[2] = newCcVal;
	midiOut->MidiOut(mMidiData);

	if (mainDisplay)
	{
		std::strstream displayMsg;
		displayMsg << "Adc " << (int) mChannel << ", " << (int) mControlNumber << ": " << newVal << ", " << (int) mMidiData[2] << std::endl << std::ends;
		mainDisplay->TextOut(displayMsg.str());
	}
}
