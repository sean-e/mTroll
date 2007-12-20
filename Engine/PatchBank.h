#ifndef PatchBank_h__
#define PatchBank_h__

#include <map>
#include <list>
#include <string>
#include <set>
#include <list>
#include "MidiControlEngine.h"

class Patch;
class IMainDisplay;
class ISwitchDisplay;


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
	void CalibrateExprSettings(const PedalCalibration * pedalCalibration);
	void SetDefaultMappings(const PatchBank & defaultMapping);

	void Load(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void Unload(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void DisplayInfo(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay, bool showPatchInfo, bool temporaryDisplay);
	void DisplayDetailedPatchInfo(int switchNumber, IMainDisplay * mainDisplay);

	void PatchSwitchPressed(int switchNumber, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void PatchSwitchReleased(int switchNumber, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void ResetPatches(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);

	int GetBankNumber() const {return mNumber;}
	const std::string & GetBankName() const {return mName;}

private:
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

		PatchMap(const PatchMap & rhs) :
			mPatchNumber(rhs.mPatchNumber),
			mPatchStateAtLoad(rhs.mPatchStateAtLoad),
			mPatchStateAtUnload(rhs.mPatchStateAtUnload),
			mPatch(rhs.mPatch)
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

	// support for exclusive switch groups
	typedef std::set<int>			GroupSwitches;			// contains the switches for an exclusive group
	struct Group
	{
		int				mActiveSwitch;
		GroupSwitches	mSwitches;

		Group() : mActiveSwitch(-1) { }
		Group(int sw) : mActiveSwitch(sw) { }
	};
	typedef std::list<Group>		Groups;	// contains all of the GroupSwitches
	typedef std::map<int, Group *>	SwitchToGroupMap;	// weak ref to GroupSwitches

	Groups				mGroups;					// this is just a store
	SwitchToGroupMap	mGroupFromSwitch;
};

#endif // PatchBank_h__
