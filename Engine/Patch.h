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
	enum PatchType 
	{
		ptNormal,			// responds to SwitchPressed; SwitchReleased does not affect patch state
		ptToggle,			// responds to SwitchPressed; SwitchReleased does not affect patch state
		ptMomentary			// responds to SwitchPressed and SwitchReleased
	};

	Patch(int number, const std::string & name, PatchType patchType, int midiOutPortNumber, IMidiOut * midiOut, const Bytes & midiStringA, const Bytes & midiStringB);
	~Patch();

	ExpressionPedals & GetPedals() {return mPedals;}

	void AssignSwitch(int switchNumber, ISwitchDisplay * switchDisplay);
	void ClearSwitch(ISwitchDisplay * switchDisplay);

	void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void SwitchReleased(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);

	void UpdateDisplays(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	const std::string & GetName() const {return mName;}
	int GetNumber() const {return mNumber;}
	bool IsOn() const {return mPatchIsOn;}
	std::string GetPatchTypeStr() const;
	PatchType GetPatchType() const {return mPatchType;}

	void SendStringA();
	void SendStringB();

private:
	const int				mNumber;	// unique across patches
	const std::string		mName;
	const PatchType			mPatchType;
	const int				mMidiOutPort;
	const Bytes				mMidiByteStringA;
	const Bytes				mMidiByteStringB;
// 	const Bytes				mMetaStringA;
// 	const Bytes				mMetaStringB;
	ExpressionPedals		mPedals;

	// runtime only state
	IMidiOut				* mMidiOut;
	bool					mPatchIsOn;
	int						mSwitchNumber;
};

extern ExpressionPedals * gActivePatchPedals;

#endif // Patch_h__
