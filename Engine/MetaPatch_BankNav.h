#ifndef MetaPatch_BankNav_h__
#define MetaPatch_BankNav_h__

#include "Patch.h"
#include "MidiControlEngine.h"


class MetaPatch_BankNavNext : public Patch
{
public:
	MetaPatch_BankNavNext(MidiControlEngine * engine, int number, const std::string & name) : 
		Patch(number, name),
		mEngine(engine)
	{
		_ASSERTE(mEngine);
	}

	virtual std::string GetPatchTypeStr() const {return "meta: BankNavNext";}

	virtual void SwitchPressed(IMainDisplay *, ISwitchDisplay *)
	{
		mEngine->EnterNavMode();
	}

	virtual bool UpdateMainDisplayOnPress() const {return false;}

	virtual void BankTransitionActivation() {}
	virtual void BankTransitionDeactivation() {}

private:
	MidiControlEngine	* mEngine;
};


class MetaPatch_BankNavPrevious : public Patch
{
public:
	MetaPatch_BankNavPrevious(MidiControlEngine * engine, int number, const std::string & name) : 
		Patch(number, name),
		mEngine(engine)
	{
		_ASSERTE(mEngine);
	}

	virtual std::string GetPatchTypeStr() const {return "meta: BankNavPrevious";}

	virtual void SwitchPressed(IMainDisplay *, ISwitchDisplay *)
	{
		mEngine->EnterNavMode();
	}

	virtual bool UpdateMainDisplayOnPress() const {return false;}

	virtual void BankTransitionActivation() {}
	virtual void BankTransitionDeactivation() {}

private:
	MidiControlEngine	* mEngine;
};

#endif // MetaPatch_BankNav_h__
