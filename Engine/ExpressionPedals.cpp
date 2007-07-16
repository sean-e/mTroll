#include <string>
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

	byte newCcVal;
	// do stuff
	newCcVal = 0;
	mCurCcVal = newCcVal;
}
