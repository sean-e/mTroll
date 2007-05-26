#pragma once


class Patch;
class IMainDisplay;
class ISwitchDisplay;
class IMidiOut;


class PatchBank
{
public:
	PatchBank(int number, std::string name);
	~PatchBank();

	enum PatchState 
	{
		stIgnore,		// default; only state valid for ptMomentary
		stA,			// only valid for ptNormal and ptToggle
		stB				// only valid for ptNormal and ptToggle
	};

	// creation/init
	void AddPatch(int switchNumber, int patchNumber, PatchState patchLoadState, PatchState patchUnloadState);
	void InitPatches(const std::map<int, Patch*> & patches);

	bool Load(IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void Unload(IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);

	void PatchSwitchPressed(int switchNumber, IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void PatchSwitchReleased(int switchNumber, IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);

private:
	int			mNumber;					// unique across all patchBanks
	std::string	mName;
	struct PatchMap
	{
		int					mSwitchNumber;	// unique per bank
		int					mPatchNumber;	// needed for load of document
		PatchState			mPatchStateAtLoad;
		PatchState			mPatchStateAtUnload;
		Patch				*mPatch;		// non-retained runtime state
	};
	std::map<int, PatchMap*>	mPatches;	// switchNumber is key
};
