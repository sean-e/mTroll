#pragma once

class IMainDisplay;
class ISwitchDisplay;
class IMidiOut;


class Patch
{
public:
	Patch(int number, std::string name, PatchType patchType, byte * stringA, byte * stringB);
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
	int			mNumber;	// unique across patches
	std::string	mName;
	PatchType	mPatchType;
	byte		*mByteStringA;
	byte		*mByteStringB;
// 	byte		*mMetaStringA;
// 	byte		*mMetaStringB;

	// runtime only state
	bool		mPatchIsOn;
	int			mSwitchNumber;
	static Patch * sCurrentNormalPatch;
};
