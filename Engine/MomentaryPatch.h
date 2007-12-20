#ifndef MomentaryPatch_h__
#define MomentaryPatch_h__

#include "TwoStatePatch.h"


// MomentaryPatch
// -----------------------------------------------------------------------------
// responds to SwitchPressed and SwitchReleased
// No expression pedal support
//
class MomentaryPatch : public TwoStatePatch
{
public:
	MomentaryPatch(int number, 
					const std::string & name, 
					int midiOutPortNumber, 
					IMidiOut * midiOut, 
					const Bytes & midiStringA, 
					const Bytes & midiStringB) :
		TwoStatePatch(number, name, midiOutPortNumber, midiOut, midiStringA, midiStringB, psDisallow)
	{
	}

	virtual std::string GetPatchTypeStr() const { return "momentary"; }
	
	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
	{
		SendStringA();
		UpdateDisplays(mainDisplay, switchDisplay);
	}

	virtual void SwitchReleased(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
	{
		SendStringB();
		UpdateDisplays(mainDisplay, switchDisplay);
	}

	virtual void Activate(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
	{
		if (IsActive())
			return;

		SwitchPressed(mainDisplay, switchDisplay);
	}

	virtual void Deactivate(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
	{
		if (!IsActive())
			return;

		SwitchReleased(mainDisplay, switchDisplay);
	}
};

#endif // MomentaryPatch_h__
