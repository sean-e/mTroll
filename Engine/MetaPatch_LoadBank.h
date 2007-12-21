#ifndef MetaPatch_LoadBank_h__
#define MetaPatch_LoadBank_h__

#include "Patch.h"
#include "MidiControlEngine.h"


class MetaPatch_LoadBank : public Patch
{
public:
	MetaPatch_LoadBank(MidiControlEngine * engine, int number, const std::string & name, int bankNumber) : 
		Patch(number, name),
		mEngine(engine),
		mBankNumber(bankNumber)
	{
		_ASSERTE(mEngine);
	}

	virtual std::string GetPatchTypeStr() const {return "meta: LoadBank";}

	virtual void SwitchPressed(IMainDisplay *, ISwitchDisplay *)
	{
		mEngine->LoadBankByNumber(mBankNumber);
	}

	virtual bool UpdateMainDisplayOnPress() const {return false;}

	virtual void BankTransitionActivation() {SwitchPressed(NULL, NULL);}
	virtual void BankTransitionDeactivation() {SwitchPressed(NULL, NULL);}

private:
	MidiControlEngine	* mEngine;
	int			mBankNumber;
};

#endif // MetaPatch_LoadBank_h__
