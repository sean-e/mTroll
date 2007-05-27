#pragma once

#include <map>
#include <string>
#include "MidiControlEngine.h"

class Patch;
class IMainDisplay;
class ISwitchDisplay;
class IMidiOut;


class PatchBank
{
public:
	PatchBank(int number, const std::string & name);
	~PatchBank();

	enum PatchState 
	{
		stIgnore,		// default; only state valid for ptMomentary
		stA,			// only valid for ptNormal and ptToggle
		stB				// only valid for ptNormal and ptToggle
	};

	// creation/init
	void AddPatchMapping(int switchNumber, int patchNumber, PatchState patchLoadState, PatchState patchUnloadState);
	void InitPatches(const MidiControlEngine::Patches & patches);

	bool Load(IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void Unload(IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);

	void PatchSwitchPressed(int switchNumber, IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void PatchSwitchReleased(int switchNumber, IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);

private:
	struct PatchMap
	{
		int					mPatchNumber;	// needed for load of document
		PatchState			mPatchStateAtLoad;
		PatchState			mPatchStateAtUnload;
		Patch				*mPatch;		// non-retained runtime state; weak ref
	};

	struct DeletePatchMap
	{
		void operator()(const std::pair<int, PatchMap *> & pr)
		{
			delete pr.second;
		}
	};

	const int					mNumber;	// unique across all patchBanks
	const std::string			mName;
	typedef std::map<int, PatchMap*> PatchMaps;
	PatchMaps					mPatches;	// switchNumber is key
};
