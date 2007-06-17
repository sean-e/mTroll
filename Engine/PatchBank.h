#ifndef PatchBank_h__
#define PatchBank_h__

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

	void Load(IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void Unload(IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void DisplayInfo(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay, bool showPatchInfo);
	void DisplayDetailedPatchInfo(int switchNumber, IMainDisplay * mainDisplay);

	void PatchSwitchPressed(int switchNumber, IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void PatchSwitchReleased(int switchNumber, IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);

	int GetBankNumber() const {return mNumber;}
	const std::string & GetBankName() const {return mName;}

private:
	void PatchSwitchAction(bool pressed, int switchNumber, IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);

	struct PatchMap
	{
		int					mPatchNumber;	// needed for load of document
		PatchState			mPatchStateAtLoad;
		PatchState			mPatchStateAtUnload;
		Patch				*mPatch;		// non-retained runtime state; weak ref

		PatchMap(int patchNumber, PatchState loadState, PatchState unloadState) :
			mPatchNumber(patchNumber),
			mPatchStateAtLoad(loadState),
			mPatchStateAtUnload(unloadState),
			mPatch(NULL)
		{
		}
	};
	typedef std::vector<PatchMap*> PatchVect;

	struct DeletePatchMaps
	{
		void operator()(const std::pair<int, PatchVect> & pr);
	};

	const int					mNumber;	// unique across all patchBanks
	const std::string			mName;
	// a switch in a bank can have multiple patches
	// only the first patch associated with a switch gets control of the switch
	// only the name of the first patch will be displayed
	typedef std::map<int, PatchVect> PatchMaps;
	PatchMaps					mPatches;	// switchNumber is key
};

#endif // PatchBank_h__
