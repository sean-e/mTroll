#ifndef MetaPatch_BankHistory_h__
#define MetaPatch_BankHistory_h__

#include "Patch.h"
#include "MidiControlEngine.h"


class MetaPatch_BankHistoryBackward : public Patch
{
public:
	MetaPatch_BankHistoryBackward(MidiControlEngine * engine, int number, const std::string & name) : 
		Patch(number, name),
		mEngine(engine)
	{
		_ASSERTE(mEngine);
	}

	virtual std::string GetPatchTypeStr() const {return "meta: BankHistoryBackward";}

	virtual void SwitchPressed(IMainDisplay *, ISwitchDisplay *)
	{
		mEngine->HistoryBackward();
	}

	virtual bool UpdateMainDisplayOnPress() const {return false;}

	virtual void BankTransitionActivation() {SwitchPressed(NULL, NULL);}
	virtual void BankTransitionDeactivation() {SwitchPressed(NULL, NULL);}

private:
	MidiControlEngine	* mEngine;
};


class MetaPatch_BankHistoryForward : public Patch
{
public:
	MetaPatch_BankHistoryForward(MidiControlEngine * engine, int number, const std::string & name) : 
		Patch(number, name),
		mEngine(engine)
	{
		_ASSERTE(mEngine);
	}

	virtual std::string GetPatchTypeStr() const {return "meta: BankHistoryForward";}

	virtual void SwitchPressed(IMainDisplay *, ISwitchDisplay *)
	{
		mEngine->HistoryForward();
	}

	virtual bool UpdateMainDisplayOnPress() const {return false;}

	virtual void BankTransitionActivation() {SwitchPressed(NULL, NULL);}
	virtual void BankTransitionDeactivation() {SwitchPressed(NULL, NULL);}

private:
	MidiControlEngine	* mEngine;
};


class MetaPatch_BankHistoryRecall : public Patch
{
public:
	MetaPatch_BankHistoryRecall(MidiControlEngine * engine, int number, const std::string & name) : 
		Patch(number, name),
		mEngine(engine)
	{
		_ASSERTE(mEngine);
	}

	virtual std::string GetPatchTypeStr() const {return "meta: BankHistoryRecall";}

	virtual void SwitchPressed(IMainDisplay *, ISwitchDisplay *)
	{
		mEngine->HistoryRecall();
	}

	virtual bool UpdateMainDisplayOnPress() const {return false;}

	virtual void BankTransitionActivation() {SwitchPressed(NULL, NULL);}
	virtual void BankTransitionDeactivation() {SwitchPressed(NULL, NULL);}

private:
	MidiControlEngine	* mEngine;
};


#endif // MetaPatch_BankHistory_h__
