#ifndef Patch_h__
#define Patch_h__

#include <string>
#include <vector>
#include "IMidiOut.h"

class IMainDisplay;
class ISwitchDisplay;


class Patch
{
public:
	Patch(int number, const std::string & name, PatchType patchType, const Bytes & stringA, const Bytes & stringB);
	~Patch();

	enum PatchType 
	{
		ptNormal,			// responds to SwitchPressed; SwitchReleased does not affect patch state
		ptToggle,			// responds to SwitchPressed; SwitchReleased does not affect patch state
		ptMomentary			// responds to SwitchPressed and SwitchReleased
	};

	void AssignSwitch(int switchNumber, ISwitchDisplay * switchDisplay);
	void ClearSwitch(ISwitchDisplay * switchDisplay);

	void SwitchPressed(IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void SwitchReleased(IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);

	void UpdateDisplays(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);

private:
	void SendStringA(IMidiOut * midiOut);
	void SendStringB(IMidiOut * midiOut);

private:
	const int				mNumber;	// unique across patches
	const std::string		mName;
	const PatchType			mPatchType;
	const Bytes				mByteStringA;
	const Bytes				mByteStringB;
// 	const Bytes				mMetaStringA;
// 	const Bytes				mMetaStringB;

	// runtime only state
	bool					mPatchIsOn;
	int						mSwitchNumber;
	static Patch			* sCurrentNormalPatch;
};

#endif // Patch_h__
