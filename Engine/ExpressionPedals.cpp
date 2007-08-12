#include <string>
#include <strstream>
#include "ExpressionPedals.h"
#include "IMainDisplay.h"
#include "IMidiOut.h"


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

	mPrevCcVal = mCurCcVal;
	mCurCcVal = (newVal * 127) / 1023;

	Bytes data;
	data.push_back(0xb0 | mChannel);
	data.push_back(mControlNumber);
	data.push_back(mCurCcVal);
	midiOut->MidiOut(data);

	if (mainDisplay)
	{
		std::strstream displayMsg;
		displayMsg << "Adc " << (int) mChannel << ", " << (int) mControlNumber << ": " << newVal << ", " << (int) mCurCcVal << std::endl << std::ends;
		mainDisplay->TextOut(displayMsg.str());
	}
}
