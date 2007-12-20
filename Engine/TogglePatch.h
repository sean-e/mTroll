#ifndef TogglePatch_h__
#define TogglePatch_h__

#include "TwoStatePatch.h"


// TogglePatch
// -----------------------------------------------------------------------------
// responds to SwitchPressed; SwitchReleased does not affect patch state
// supports expression pedals (psAllowOnlyActive) - but should it?
//
class TogglePatch : public TwoStatePatch
{
public:
	TogglePatch(int number, 
				const std::string & name, 
				int midiOutPortNumber, 
				IMidiOut * midiOut, 
				const Bytes & midiStringA, 
				const Bytes & midiStringB) :
		TwoStatePatch(number, name, midiOutPortNumber, midiOut, midiStringA, midiStringB, psAllowOnlyActive)
	{
	}

	virtual std::string GetPatchTypeStr() const { return "toggle"; }

	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
	{
		if (mPatchIsActive)
			SendStringB();
		else
			SendStringA();

		UpdateDisplays(mainDisplay, switchDisplay);
	}
};

#endif // TogglePatch_h__
