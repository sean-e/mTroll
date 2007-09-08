#ifndef MetaPatch_ResetBankPatches_h__
#define MetaPatch_ResetBankPatches_h__

#include "Patch.h"
#include "MidiControlEngine.h"


class MetaPatch_ResetBankPatches : public Patch
{
public:
	MetaPatch_ResetBankPatches(MidiControlEngine * engine, int number, const std::string & name) : 
		Patch(number, name, ptToggle), // ptToggle so that expression pedals aren't accessed
		mEngine(engine)
	{
		_ASSERTE(mEngine);
	}

	virtual void SwitchPressed(IMainDisplay *, ISwitchDisplay *)
	{
		mEngine->ResetBankPatches();
	}

private:
	MidiControlEngine	* mEngine;
};

#endif // MetaPatch_ResetBankPatches_h__
