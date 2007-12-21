#ifndef Patch_h__
#define Patch_h__

#include <string>
#include <vector>
#include "IMidiOut.h"
#include "ExpressionPedals.h"

class IMainDisplay;
class ISwitchDisplay;


class Patch
{
public:
	virtual ~Patch();

	ExpressionPedals & GetPedals() {return mPedals;}

	void AssignSwitch(int switchNumber, ISwitchDisplay * switchDisplay);
	void ClearSwitch(ISwitchDisplay * switchDisplay);

	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) = 0;
	virtual void SwitchReleased(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) { }

	void UpdateDisplays(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	const std::string & GetName() const {return mName;}
	int GetNumber() const {return mNumber;}
	bool IsActive() const {return mPatchIsActive;}
	void Reset(ISwitchDisplay * switchDisplay) {mPatchIsActive = false; UpdateDisplays(NULL, switchDisplay);}
	virtual bool UpdateMainDisplayOnPress() const {return true;}

	virtual void Activate(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	virtual void Deactivate(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);

	virtual std::string GetPatchTypeStr() const = 0;
	virtual bool IsPatchVolatile() const {return false;} // load of one volatile patch affects loaded volatile patch (typically normal mode patches)
	virtual void DeactivateVolatilePatch() { }

	virtual void BankTransitionActivation() = 0;
	virtual void BankTransitionDeactivation() = 0;

protected:
	Patch(int number, const std::string & name, int midiOutPortNumber = -1, IMidiOut * midiOut = NULL);

private:
	Patch();
	Patch(const Patch &);

protected:
	ExpressionPedals		mPedals;
	IMidiOut				* mMidiOut;
	bool					mPatchIsActive;

private:
	const int				mNumber;	// unique across patches
	const std::string		mName;
	const int				mMidiOutPort;
	std::vector<int>		mSwitchNumbers;
};

extern ExpressionPedals * gActivePatchPedals;

#endif // Patch_h__
