#ifndef NormalPatch_h__
#define NormalPatch_h__

#include "TwoStatePatch.h"


// NormalPatch
// -----------------------------------------------------------------------------
// responds to SwitchPressed; SwitchReleased does not affect patch state
// 
class NormalPatch : public TwoStatePatch
{
public:
	NormalPatch(int number, 
				const std::string & name, 
				int midiOutPortNumber, 
				IMidiOut * midiOut, 
				const Bytes & midiStringA, 
				const Bytes & midiStringB) :
		TwoStatePatch(number, name, midiOutPortNumber, midiOut, midiStringA, midiStringB, psAllow)
	{
	}

	virtual std::string GetPatchTypeStr() const { return "normal"; }
	virtual bool IsPatchVolatile() const { return true; }
	virtual void DeactivateVolatilePatch() { SendStringB(); }

	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
	{
		if (mPatchIsActive)
		{
			SendStringB();
			SendStringA();
		}
		else
			SendStringA();

		UpdateDisplays(mainDisplay, switchDisplay);
	}

	virtual void SwitchReleased(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
	{
		return;
	}
};

#endif // NormalPatch_h__
