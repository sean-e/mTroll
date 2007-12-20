#ifndef MetaPatch_ResetBankPatches_h__
#define MetaPatch_ResetBankPatches_h__

#include "Patch.h"
#include "MidiControlEngine.h"


class MetaPatch_ResetBankPatches : public Patch
{
public:
	MetaPatch_ResetBankPatches(MidiControlEngine * engine, int number, const std::string & name) : 
		Patch(number, name),
		mEngine(engine)
	{
		_ASSERTE(mEngine);
	}

	virtual std::string GetPatchTypeStr() const {return "meta: resetBankPatches";}

	virtual void SwitchPressed(IMainDisplay *, ISwitchDisplay *)
	{
		mEngine->ResetBankPatches();
	}

	virtual void BankTransitionActivation() {SwitchPressed(NULL, NULL);}
	virtual void BankTransitionDeactivation() {SwitchPressed(NULL, NULL);}

private:
	MidiControlEngine	* mEngine;
};

#endif // MetaPatch_ResetBankPatches_h__
