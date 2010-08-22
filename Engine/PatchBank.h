/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2008 Sean Echevarria
 *
 * This file is part of mTroll.
 *
 * mTroll is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mTroll is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Let me know if you modify, extend or use mTroll.
 * Original project site: http://www.creepingfog.com/mTroll/
 * Contact Sean: "fester" at the domain of the original project site
 */

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
	void InitPatches(const MidiControlEngine::Patches & patches, ITraceDisplay * traceDisp);
	void CalibrateExprSettings(const PedalCalibration * pedalCalibration, MidiControlEngine * eng, ITraceDisplay * traceDisp);
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

	// support for exclusive switch groups
	typedef std::set<int> GroupSwitches;			// contains the switches for an exclusive group
	void CreateExclusiveGroup(GroupSwitches * switches);

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
	typedef std::list<GroupSwitches*>	Groups;					// contains all of the GroupSwitches for the current bank
	typedef std::map<int, GroupSwitches *> SwitchToGroupMap;	// weak ref to GroupSwitches - lookup group from switchnumber - many to one

	Groups				mGroups;					// this is just a store - for freeing GroupSwitches
	SwitchToGroupMap	mGroupFromSwitch;
};

#endif // PatchBank_h__
