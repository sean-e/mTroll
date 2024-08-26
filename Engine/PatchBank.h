/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2008,2010,2012-2015,2018,2024 Sean Echevarria
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
#include <memory>
#include "MidiControlEngine.h"

class Patch;
class IMainDisplay;
class ISwitchDisplay;


class PatchBank
{
public:
	PatchBank(int number, const std::string & name, const std::string & additionalText);
	~PatchBank();

	enum SwitchFunctionAssignment
	{ 
		ssPrimary,		// default
		ssSecondary,	// long-press secondary function
		ssCount
	};

	enum PatchState 
	{
		stIgnore,		// default; only state valid for ptMomentary
		stA,			// only valid for ptNormal and ptToggle
		stB				// only valid for ptNormal and ptToggle
	};

	enum PatchSyncState
	{
		syncIgnore,		// default
		syncInPhase,	// PatchState of sibling patches sync identical to master patch
		syncOutOfPhase	// PatchState of sibling patches sync opposite of master patch
	};

	enum SwitchPressDuration
	{
		spdShort,		// normal
		spdLong,		// long-press: invoke second function using defined SecondFunctionOperation mode
		spdExtended		// extra-long: invoke second function in sfoManual mode (ignore SecondFunctionOperation mode)
	};

	enum SecondFunctionOperation
	{
		sfoNone,			// no secondary function

		// secondFunction="manual|auto|autoOn|autoOff|immediateToggle"
		sfoManual,			// long-press simply changes switch display; default
		sfoAutoEnable,		// long-press changes switch display and presses switch to enable
		sfoAutoDisable,		// long-press changes switch display; user must press again to enable; press to disable also does 'long-press' to toggle back
		sfoAuto,			// long-press changes switch display and presses switch to enable; press to disable also does 'long-press' to toggle back
		sfoStatelessToggle	// long-press activates second (presses switch of second), reverts back to primary
	};

	// creation/init
	void AddSwitchAssignment(int switchNumber, int patchNumber, const std::string &overrideName, SwitchFunctionAssignment st, SecondFunctionOperation sfoOp, PatchState patchLoadState, PatchState patchUnloadState, PatchState patchStateOverride, PatchSyncState patchSyncState);
	void InitPatches(const MidiControlEngine::Patches & patches, ITraceDisplay * traceDisp);
	void CalibrateExprSettings(const PedalCalibration * pedalCalibration, MidiControlEngine * eng, ITraceDisplay * traceDisp);
	void SetDefaultMappings(const PatchBankPtr defaultMapping);

	void Load(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void Unload(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void DisplayInfo(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay, bool showPatchInfo, bool temporaryDisplay);
	void DisplayDetailedPatchInfo(int switchNumber, IMainDisplay * mainDisplay);

	void PatchSwitchPressed(int switchNumber, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void PatchSwitchReleased(int switchNumber, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay, SwitchPressDuration dur);
	void ResetPatches(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void ResetExclusiveGroup(ISwitchDisplay * switchDisplay, int switchNumberToSet);

	int GetBankNumber() const {return mNumber;}
	const std::string & GetBankName() const {return mName;}

	// support for exclusive switch groups
	using GroupSwitches = std::set<int>;			// contains the switches for an exclusive group
	using GroupSwitchesPtr = std::shared_ptr<GroupSwitches>;
	void CreateExclusiveGroup(GroupSwitchesPtr switches);

private:
	bool SwitchHasSecondaryLogic(int switchNumber) { return mPatches[switchNumber].mSfOp != sfoNone; }
	void ToggleDualFunctionState(int switchNumber, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void PatchSwitchPressed(SwitchFunctionAssignment st, int switchNumber, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void PatchSwitchReleased(SwitchFunctionAssignment st, int switchNumber, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);

private:
	struct BankPatchState
	{
		int					mPatchNumber;	// needed for load of document
		std::string			mOverrideSwitchName;
		PatchState			mPatchStateAtBankLoad;
		PatchState			mPatchStateAtBankUnload;
		PatchState			mPatchStateOverride; // press of switch prevents toggle from changing
		PatchSyncState		mPatchSyncState;
		SecondFunctionOperation mSfOp;
		PatchPtr			mPatch;		// non-retained runtime state

		BankPatchState(int patchNumber, std::string overrideName, PatchState loadState, PatchState unloadState, PatchState stateOverride, PatchSyncState patchSyncState, SecondFunctionOperation sfOp) :
			mPatchNumber(patchNumber),
			mOverrideSwitchName(overrideName),
			mPatchStateAtBankLoad(loadState),
			mPatchStateAtBankUnload(unloadState),
			mPatchStateOverride(stateOverride),
			mPatchSyncState(patchSyncState),
			mSfOp(sfOp),
			mPatch(nullptr)
		{
		}

		BankPatchState(const BankPatchState & rhs) :
			mPatchNumber(rhs.mPatchNumber),
			mOverrideSwitchName(rhs.mOverrideSwitchName),
			mPatchStateAtBankLoad(rhs.mPatchStateAtBankLoad),
			mPatchStateAtBankUnload(rhs.mPatchStateAtBankUnload),
			mPatchStateOverride(rhs.mPatchStateOverride),
			mPatchSyncState(rhs.mPatchSyncState),
			mSfOp(rhs.mSfOp),
			mPatch(rhs.mPatch)
		{
		}
	};
	using BankPatchStatePtr = std::shared_ptr<BankPatchState>;
	using PatchVect = std::vector<BankPatchStatePtr>;

	void CheckForDeviceProgramChange(BankPatchState *curSwitchItem, ISwitchDisplay *switchDisplay, IMainDisplay *mainDisplay);

	struct DualPatchVect
	{
		SwitchFunctionAssignment	mCurrentSwitchState;	// events affect primary or secondary patches
		SecondFunctionOperation		mSfOp;					// assigned from first secondary patch
		PatchVect					mPrimaryPatches;
		PatchVect					mSecondaryPatches;

		DualPatchVect() : mCurrentSwitchState(ssPrimary), mSfOp(sfoNone) { }

		PatchVect &					GetPatchVect()
		{
			return GetPatchVect(mCurrentSwitchState);
		}

		PatchVect &					GetPatchVect(SwitchFunctionAssignment ss)
		{
			if (ssPrimary == ss)
				return mPrimaryPatches;
			else // any other value is considered secondary
				return mSecondaryPatches;
		}

		const PatchVect &			GetPatchVectConst(SwitchFunctionAssignment ss) const
		{
			if (ssPrimary == ss)
				return mPrimaryPatches;
			else // any other value is considered secondary
				return mSecondaryPatches;
		}
	};

	const int					mNumber;	// unique across all patchBanks
	const std::string			mName;
	const std::string			mAdditionalText;
	// a switch in a bank can have multiple patches
	// only the first patch associated with a switch gets control of the switch
	// only the name of the first patch will be displayed
	using PatchMaps = std::map<int, DualPatchVect>;
	PatchMaps					mPatches;	// switchNumber is key

	// support for exclusive switch groups
	using Groups = std::list<GroupSwitchesPtr>;					// contains all of the GroupSwitches for the current bank
	using SwitchToGroupMap = std::map<int, GroupSwitchesPtr>;	// lookup group from switch number - many to one

	Groups				mGroups;
	SwitchToGroupMap	mGroupFromSwitch;
	bool				mLoaded;
};

using PatchBankPtr = std::shared_ptr<PatchBank>;

#endif // PatchBank_h__
