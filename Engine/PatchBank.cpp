/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2008,2010-2016,2018 Sean Echevarria
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

#include <algorithm>
#include <strstream>
#include <iomanip>
#include "PatchBank.h"
#include "Patch.h"

#include "IMainDisplay.h"
#include "ISwitchDisplay.h"
#include "ITraceDisplay.h"
#include "DeletePtr.h"


static std::list<Patch *>	sActiveVolatilePatches;


PatchBank::PatchBank(int number, 
					 const std::string & name) :
	mNumber(number),
	mName(name),
	mLoaded(false)
{
}

PatchBank::~PatchBank()
{
	mGroupFromSwitch.clear();
	std::for_each(mGroups.begin(), mGroups.end(), DeletePtr<GroupSwitches>());
	mGroups.clear();

	std::for_each(mPatches.begin(), mPatches.end(), DeletePatchMaps());
	mPatches.clear();

	sActiveVolatilePatches.clear();
}

void PatchBank::DeletePatchMaps::operator()(const std::pair<int, DualPatchVect> & pr)
{
	std::for_each(pr.second.mPrimaryPatches.begin(), pr.second.mPrimaryPatches.end(), DeletePtr<BankPatchState>());
	std::for_each(pr.second.mSecondaryPatches.begin(), pr.second.mSecondaryPatches.end(), DeletePtr<BankPatchState>());
}

void
PatchBank::AddSwitchAssignment(int switchNumber, 
						   int patchNumber, 
						   const std::string &overrideName,
						   SwitchFunctionAssignment st, 
						   SecondFunctionOperation sfoOp, 
						   PatchState patchLoadState, 
						   PatchState patchUnloadState, 
						   PatchState patchStateOverride, 
						   PatchSyncState patchSyncState)
{
	PatchVect & curPatches = mPatches[switchNumber].GetPatchVect(st);
	curPatches.push_back(new BankPatchState(patchNumber, overrideName, patchLoadState, patchUnloadState, patchStateOverride, patchSyncState, sfoOp));
	if (ssSecondary == st)
	{
		if (!SwitchHasSecondaryLogic(switchNumber))
			mPatches[switchNumber].mSfOp = sfoOp; // first secondary patch sets the op for all secondary patches

		_ASSERTE(mPatches[switchNumber].mSfOp == sfoOp);
	}
}

void
PatchBank::SetDefaultMappings(const PatchBank & defaultMapping)
{
	// propagate switch groups from default bank
	if (&defaultMapping != this && !defaultMapping.mGroups.empty())
	{
		std::for_each(defaultMapping.mGroups.begin(), defaultMapping.mGroups.end(), 
			[this](GroupSwitches * grp)
		{
			bool useThisMapping = true;
			PatchMaps & pm2 = mPatches;
			SwitchToGroupMap & nonDefaultMappings = mGroupFromSwitch;
			PatchBank::GroupSwitches * grpCpy = new PatchBank::GroupSwitches;
			std::for_each(grp->begin(), grp->end(), [&pm2, &grpCpy, &nonDefaultMappings, &useThisMapping](int num)
			{
				// only insert if num is not already assigned (num of switch has been assigned a patch).
				// this means that a group might be smaller in the current bank than the default bank.
				for (int idx2 = PatchBank::ssPrimary; idx2 < PatchBank::ssCount; ++idx2)
				{
					PatchBank::SwitchFunctionAssignment ss = static_cast<PatchBank::SwitchFunctionAssignment>(idx2);
					PatchBank::PatchVect & curPatches = pm2[num].GetPatchVect(ss);
					if (!curPatches.empty())
						return;
				}

				if (nonDefaultMappings.find(num) != nonDefaultMappings.end())
				{
					// this default group has buttons that are already assigned a group, so 
					// reject this entire default group
					useThisMapping = false;
				}
				else
					grpCpy->insert(num);
			});

			if (!grpCpy->empty() && useThisMapping)
				CreateExclusiveGroup(grpCpy);
			else
				delete grpCpy;
		});
	}

	for (int idx = ssPrimary; idx < ssCount; ++idx)
	{
		SwitchFunctionAssignment ss = static_cast<SwitchFunctionAssignment>(idx);

		for (PatchMaps::const_iterator it = defaultMapping.mPatches.begin();
			it != defaultMapping.mPatches.end();
			++it)
		{
			const int switchNumber = (*it).first;
			PatchVect & curPatches = mPatches[switchNumber].GetPatchVect(ss);
			if (!curPatches.empty())
				continue;

			if (ssSecondary == ss)
				mPatches[switchNumber].mSfOp = (*it).second.mSfOp;

			const PatchVect & patches = (*it).second.GetPatchVectConst(ss);
			for (PatchVect::const_iterator it2 = patches.begin();
				it2 != patches.end();
				++it2)
			{
				BankPatchState * curItem = *it2;
				curPatches.push_back(new BankPatchState(*curItem));
			}
		}
	}
}

void
PatchBank::InitPatches(const MidiControlEngine::Patches & enginePatches,
					   ITraceDisplay * traceDisp)
{
	for (PatchMaps::iterator it = mPatches.begin();
		it != mPatches.end();
		++it)
	{
		for (int idx = ssPrimary; idx < ssCount; ++idx)
		{
			SwitchFunctionAssignment ss = static_cast<SwitchFunctionAssignment>(idx);
			PatchVect & patches = (*it).second.GetPatchVect(ss);
			for (PatchVect::iterator it2 = patches.begin();
				 it2 != patches.end();
				 )
			{
				bool inc = true;
				BankPatchState * curItem = *it2;
				if (curItem)
				{
					MidiControlEngine::Patches::const_iterator patchIt = enginePatches.find(curItem->mPatchNumber);
					if (patchIt == enginePatches.end())
					{
						if (curItem->mPatchNumber != -1)
						{
							if (traceDisp)
							{
								std::strstream traceMsg;
								traceMsg << "Patch " << curItem->mPatchNumber << " referenced in bank " << mName << " (" << mNumber << ") does not exist!" << std::endl << std::ends;
								traceDisp->Trace(std::string(traceMsg.str()));
							}

							inc = false;
							it2 = patches.erase(it2);
						}
					}
					else
					{
						Patch * thePatch = (*patchIt).second;
						curItem->mPatch = thePatch;
					}
				}

				if (inc)
					++it2;
			}
		}
	}
}

void
PatchBank::CalibrateExprSettings(const PedalCalibration * pedalCalibration, MidiControlEngine * eng, ITraceDisplay * traceDisp)
{
	for (PatchMaps::iterator it = mPatches.begin();
		it != mPatches.end();
		++it)
	{
		for (int idx = ssPrimary; idx < ssCount; ++idx)
		{
			SwitchFunctionAssignment ss = static_cast<SwitchFunctionAssignment>(idx);
			PatchVect & patches = (*it).second.GetPatchVect(ss);
			for (PatchVect::iterator it2 = patches.begin();
				it2 != patches.end();
				++it2)
			{
				BankPatchState * curItem = *it2;
				if (curItem && curItem->mPatch)
				{
					ExpressionPedals & pedals = curItem->mPatch->GetPedals();
					pedals.Calibrate(pedalCalibration, eng, traceDisp);
				}
			}
		}
	}
}

void
PatchBank::Load(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	mLoaded = true;
	for (PatchMaps::iterator it = mPatches.begin();
		it != mPatches.end();
		++it)
	{
		for (int idx = ssPrimary; idx < ssCount; ++idx)
		{
			bool once = true;
			SwitchFunctionAssignment ss = static_cast<SwitchFunctionAssignment>(idx);
			PatchVect & patches = (*it).second.GetPatchVect(ss);
			for (PatchVect::iterator it2 = patches.begin();
				it2 != patches.end();
				++it2)
			{
				BankPatchState * curItem = *it2;
				if (!curItem || !curItem->mPatch)
					continue;

				if (!once)
					curItem->mPatch->OverridePedals(true); // expression pedals only apply to first patch

				if (stA == curItem->mPatchStateAtBankLoad)
					curItem->mPatch->BankTransitionActivation();
				else if (stB == curItem->mPatchStateAtBankLoad)
					curItem->mPatch->BankTransitionDeactivation();

				// only assign a switch to the first patch (if multiple 
				// patches are assigned to the same switch)
				if (once && ssPrimary == ss)
				{
					once = false;
					curItem->mPatch->AssignSwitch((*it).first, switchDisplay);
					if (curItem->mOverrideSwitchName.length() && switchDisplay)
						switchDisplay->SetSwitchText((*it).first, curItem->mOverrideSwitchName);
				}
				else
					curItem->mPatch->OverridePedals(false);
// 				else
// 					curItem->mPatch->AssignSwitch(-1, NULL);
			}
		}
	}

	DisplayInfo(mainDisplay, switchDisplay, true, false);
}

void
PatchBank::Unload(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	mLoaded = false;
	for (PatchMaps::iterator it = mPatches.begin();
		it != mPatches.end();
		++it)
	{
		for (int idx = ssPrimary; idx < ssCount; ++idx)
		{
			bool once = true;
			SwitchFunctionAssignment ss = static_cast<SwitchFunctionAssignment>(idx);
			PatchVect & patches = (*it).second.GetPatchVect(ss);
			for (PatchVect::iterator it2 = patches.begin();
				it2 != patches.end();
				++it2)
			{
				BankPatchState * curItem = *it2;
				if (!curItem || !curItem->mPatch)
					continue;

				if (!once)
					curItem->mPatch->OverridePedals(true);

				if (stA == curItem->mPatchStateAtBankUnload)
					curItem->mPatch->BankTransitionActivation();
				else if (stB == curItem->mPatchStateAtBankUnload)
					curItem->mPatch->BankTransitionDeactivation();

				if (once)
					once = false;
				else
					curItem->mPatch->OverridePedals(false);

				curItem->mPatch->ClearSwitch(switchDisplay);
			}
		}
	}

//	mainDisplay->ClearDisplay();
}

void
PatchBank::PatchSwitchPressed(int switchNumber, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	if (SwitchHasSecondaryLogic(switchNumber))
	{
		// handle in PatchSwitchReleased after check for long-press.
		// this means that momentary patches will not work as expected when
		// secondary patches exist.
		return;
	}

	// switch doesn't have a secondary function, handle as discrete press and release
	PatchSwitchPressed(ssPrimary, switchNumber, mainDisplay, switchDisplay);
}

void
PatchBank::PatchSwitchPressed(SwitchFunctionAssignment st, 
							  int switchNumber, 
							  IMainDisplay * mainDisplay, 
							  ISwitchDisplay * switchDisplay)
{
	PatchVect & curPatches = mPatches[switchNumber].GetPatchVect(st);
	PatchVect::iterator it;
	bool curSwitchHasNormalPatch = false;

	// if any patch for the switch is normal, then need to do normal processing
	// on current normal patches.
	for (it = curPatches.begin();
		 it != curPatches.end();
		 ++it)
	{
		BankPatchState * curSwitchItem = *it;
		if (!curSwitchItem || !curSwitchItem->mPatch)
			continue;

		if (curSwitchItem->mPatch->IsPatchVolatile())
		{
			curSwitchHasNormalPatch = true;
			break;
		}
	}

	if (curSwitchHasNormalPatch)
	{
		// do B processing
		for (std::list<Patch*>::iterator it2 = sActiveVolatilePatches.begin();
			it2 != sActiveVolatilePatches.end();
			it2 = sActiveVolatilePatches.begin())
		{
			Patch * curPatchItem = *it2;
			if (curPatchItem && curPatchItem->IsActive())
			{
				curPatchItem->DeactivateVolatilePatch();
				curPatchItem->UpdateDisplays(mainDisplay, switchDisplay);
			}
			sActiveVolatilePatches.erase(it2);
		}
	}

	GroupSwitches * grp = mGroupFromSwitch[switchNumber];
	if (grp)
	{
		bool isRepress = false;
		bool isAnyPatchInGroupActiveThatIsNotCurrentSwitch = false;
		for (GroupSwitches::iterator switchIt = grp->begin();
			switchIt != grp->end();
			++switchIt)
		{
			const int kCurSwitchNumber = *switchIt;
			PatchVect & prevPatches = mPatches[kCurSwitchNumber].GetPatchVect(st);
			for (it = prevPatches.begin();
				it != prevPatches.end();
				++it)
			{
				BankPatchState * curSwitchItem = *it;
				if (!curSwitchItem || !curSwitchItem->mPatch)
					continue;

				if (kCurSwitchNumber == switchNumber)
					isRepress = true;
				else if (curSwitchItem->mPatch->IsActive())
					isAnyPatchInGroupActiveThatIsNotCurrentSwitch = true;

				// only check the first patch
				break;
			}
		}

		if (!isRepress || isAnyPatchInGroupActiveThatIsNotCurrentSwitch)
		{
			// turn off any in group that are active
			for (GroupSwitches::iterator switchIt = grp->begin();
				switchIt != grp->end();
				++switchIt)
			{
				const int kCurSwitchNumber = *switchIt;
				if (kCurSwitchNumber == switchNumber)
					continue; // don't deactivate if pressing the same switch multiple times

				// exclusive groups don't really support secondary functions.
				// keep secondary function groups separate from primary function groups.
				// basically unsupported for now.
				PatchVect & prevPatches = mPatches[kCurSwitchNumber].GetPatchVect(st);
				for (it = prevPatches.begin();
					it != prevPatches.end();
					++it)
				{
					BankPatchState * curSwitchItem = *it;
					if (!curSwitchItem || !curSwitchItem->mPatch)
						continue;

					curSwitchItem->mPatch->Deactivate(mainDisplay, switchDisplay);

					// force displays off for all other patches in group
					curSwitchItem->mPatch->UpdateDisplays(mainDisplay, switchDisplay);
				}
			}
		}
	}

	bool doDisplayUpdate = true;

	it = curPatches.begin();
	bool masterPatchIsActive = false;
	if (it != curPatches.end())
	{
		BankPatchState * curSwitchItem = *it;
		if (curSwitchItem && curSwitchItem->mPatch && curSwitchItem->mPatch->IsActive())
			masterPatchIsActive = true; // master patch is just the first
	}

	// do standard pressed processing (send A)
	bool once = (st == ssPrimary);
	std::strstream msgstr;
	for (it = curPatches.begin();
		 it != curPatches.end();
		 ++it)
	{
		BankPatchState * curSwitchItem = *it;
		if (!curSwitchItem || !curSwitchItem->mPatch)
			continue;

		if (!once)
			curSwitchItem->mPatch->OverridePedals(true); // expression pedals only apply to first patch

		if (syncIgnore != curSwitchItem->mPatchSyncState)
		{
			// sync mode
			// sync options work relative to first mapping
			// sync options not applicable to first mapping
			// 	 syncIgnore		// siblings are toggled as is - might be same as master, might not - depends on active state, not sync'd up first
			// 	 syncInPhase,	// PatchState of sibling patches sync identical to master patch
			// 	 syncOutOfPhase	// PatchState of sibling patches sync opposite of master patch
			// Note: works on the assumption that IsActive can just be toggled by issuing a SwitchPressed call

			// Consider: option for sync on activate only - override no change after load?
			// Consider: option for override on deactivate only - override no change on load?

			if (syncInPhase == curSwitchItem->mPatchSyncState)
			{
				if (curSwitchItem->mPatch->IsActive() != masterPatchIsActive)
					curSwitchItem->mPatch->SwitchPressed(nullptr, switchDisplay);
			}
			else
			{
				if (curSwitchItem->mPatch->IsActive() == masterPatchIsActive)
					curSwitchItem->mPatch->SwitchPressed(nullptr, switchDisplay);
			}
		}

		if (stIgnore == curSwitchItem->mPatchStateOverride ||
			(stA == curSwitchItem->mPatchStateOverride && !curSwitchItem->mPatch->IsActive()) ||
			(stB == curSwitchItem->mPatchStateOverride && curSwitchItem->mPatch->IsActive()))
			curSwitchItem->mPatch->SwitchPressed(nullptr, switchDisplay);
		else
			curSwitchItem->mPatch->UpdateDisplays(nullptr, switchDisplay);

		if (once)
			once = false;
		else
			curSwitchItem->mPatch->OverridePedals(false);

		if (curSwitchItem->mPatch->IsPatchVolatile())
		{
			_ASSERTE(std::find(sActiveVolatilePatches.begin(), sActiveVolatilePatches.end(), curSwitchItem->mPatch) == sActiveVolatilePatches.end());
			sActiveVolatilePatches.push_back(curSwitchItem->mPatch);
		}

		if (curSwitchItem->mPatch->UpdateMainDisplayOnPress())
		{
			const int patchNum = curSwitchItem->mPatch->GetNumber();
			if (patchNum > 0)
				msgstr << patchNum << " ";
			msgstr << curSwitchItem->mPatch->GetDisplayText(true) << std::endl;
		}
		else
			doDisplayUpdate = false;
	}

	if (mainDisplay && doDisplayUpdate)
	{
		msgstr << std::ends;
		mainDisplay->TextOut(msgstr.str());
	}
}

void
PatchBank::PatchSwitchReleased(int switchNumber, 
							   IMainDisplay * mainDisplay, 
							   ISwitchDisplay * switchDisplay, 
							   SwitchPressDuration dur)
{
	const bool kHasSecondaryLogic = SwitchHasSecondaryLogic(switchNumber);
	if (kHasSecondaryLogic)
	{
		if (spdShort != dur)
		{
			// long press changes switch mode; handle transition
			ToggleDualFunctionState(switchNumber, mainDisplay, switchDisplay);
			if (ssPrimary == mPatches[switchNumber].mCurrentSwitchState)
			{
				// back in primary mode, no more work to do, except update main display
				if (mLoaded)
				{
					PatchVect & patches = mPatches[switchNumber].GetPatchVect();
					for (PatchVect::iterator it2 = patches.begin();
						it2 != patches.end();
						++it2)
					{
						BankPatchState * curItem = *it2;
						if (!curItem || !curItem->mPatch)
							continue;

						// update main display to note new function state registered
						curItem->mPatch->UpdateDisplays(mainDisplay, nullptr);
						break;
					}
				}
				return;
			}

			// now in secondary mode

			if (spdExtended == dur)
			{
				// spdExtended is treated as sfoManual regardless of mSfOp for 
				// manual transition into and out of secondary function mode.
				// 
				// Note that spdExtended does not prevent short-press release 
				// behavior of sfoAuto*.  If op is Auto and spdExtended causes
				// transition to secondary function, a short-press will still
				// automatically transition back to primary function.
				return;
			}

			switch (mPatches[switchNumber].mSfOp)
			{
			case sfoAuto:
			case sfoAutoEnable:
				// activate on press if not already active
				if (mPatches[switchNumber].mSecondaryPatches[0]->mPatch->IsActive())
					return; 
				break;
			case sfoStatelessToggle:
				// toggle regardless of state and immediately revert back to primary mode
				break;
			default:
				return;
			}
		}

		// since PatchSwitchPressed was ignored, handle now
		PatchSwitchPressed(mPatches[switchNumber].mCurrentSwitchState, switchNumber, mainDisplay, switchDisplay);
	}

	// this only happens during short-press or during sfoAuto* long-press
	PatchSwitchReleased(mPatches[switchNumber].mCurrentSwitchState, switchNumber, mainDisplay, switchDisplay);

	if (kHasSecondaryLogic && ssSecondary == mPatches[switchNumber].mCurrentSwitchState)
	{
		// in secondary mode, see if we need to go back to primary mode
		switch (dur)
		{
		case spdShort:
			switch (mPatches[switchNumber].mSfOp)
			{
			case sfoAuto:
			case sfoAutoDisable:
				// toggle switch function on release if no longer active
				if (mPatches[switchNumber].mSecondaryPatches[0]->mPatch->IsActive())
					return; 
				break;
			case sfoStatelessToggle:
				// toggle regardless of state and immediately revert back to primary mode
				break;
			default:
				return;
			}

			// handle transition from secondary mode back to standard/primary mode for sfoAuto and sfoAutoDisable
			ToggleDualFunctionState(switchNumber, mainDisplay, switchDisplay);
			break;

		case spdLong:
			if (sfoStatelessToggle == mPatches[switchNumber].mSfOp)
			{
				// handle transition from secondary mode back to standard/primary mode for sfoStatelessToggle
				ToggleDualFunctionState(switchNumber, mainDisplay, switchDisplay);
			}
			break;
		}
	}
}

void
PatchBank::PatchSwitchReleased(SwitchFunctionAssignment st,
							   int switchNumber, 
							   IMainDisplay * mainDisplay, 
							   ISwitchDisplay * switchDisplay)
{
	PatchVect & curPatches = mPatches[switchNumber].GetPatchVect(st);

	bool once = (st == ssPrimary);
	for (PatchVect::iterator it = curPatches.begin();
		 it != curPatches.end();
		 ++it)
	{
		BankPatchState * curSwitchItem = *it;
		if (!curSwitchItem || !curSwitchItem->mPatch)
			continue;

		if (!once)
			curSwitchItem->mPatch->OverridePedals(true); // expression pedals only apply to first patch

		curSwitchItem->mPatch->SwitchReleased(mainDisplay, switchDisplay);

		if (once)
			once = false;
		else
			curSwitchItem->mPatch->OverridePedals(false);
	}
}

void
PatchBank::ToggleDualFunctionState(int switchNumber, 
								   IMainDisplay * mainDisplay, 
								   ISwitchDisplay * switchDisplay)
{
	if (mLoaded)
	{
		// clear switch assignment of current functions (extract of Unload).
		// only do if loaded, since unload should have already handled
		PatchVect & patches = mPatches[switchNumber].GetPatchVect();
		for (PatchVect::iterator it2 = patches.begin();
			it2 != patches.end();
			++it2)
		{
			BankPatchState * curItem = *it2;
			if (!curItem || !curItem->mPatch)
				continue;

			curItem->mPatch->RemoveSwitch(switchNumber, switchDisplay);
		}
	}

	// toggle the function
	mPatches[switchNumber].mCurrentSwitchState = 
		(mPatches[switchNumber].mCurrentSwitchState == ssPrimary) ? ssSecondary : ssPrimary;

	if (mLoaded)
	{
		// assign switch to first current function (extract of Load).
		// only do if loaded, since we don't want to modify switches under control
		// of another now loaded bank
		PatchVect & patches = mPatches[switchNumber].GetPatchVect();
		for (PatchVect::iterator it2 = patches.begin();
			it2 != patches.end();
			++it2)
		{
			BankPatchState * curItem = *it2;
			if (!curItem || !curItem->mPatch)
				continue;

			curItem->mPatch->AssignSwitch(switchNumber, switchDisplay);
			if (curItem->mOverrideSwitchName.length() && switchDisplay)
				switchDisplay->SetSwitchText(switchNumber, curItem->mOverrideSwitchName);

			// update main display to note new function state registered
			if (mPatches[switchNumber].mCurrentSwitchState == ssSecondary)
				curItem->mPatch->UpdateDisplays(mainDisplay, nullptr);
			break;
		}
	}
}

void
PatchBank::DisplayInfo(IMainDisplay * mainDisplay, 
					   ISwitchDisplay * switchDisplay,
					   bool showPatchInfo,
					   bool temporaryDisplay)
{
	std::ostrstream info;
	info << mNumber << " " << mName;

	if (showPatchInfo)
	{
		info << std::endl;
		for (PatchMaps::iterator it = mPatches.begin();
			it != mPatches.end();
			++it)
		{
			for (int idx = ssPrimary; idx < ssCount; ++idx)
			{
				SwitchFunctionAssignment ss = static_cast<SwitchFunctionAssignment>(idx);
				PatchVect & patches = (*it).second.GetPatchVect(ss);
				bool once = true;
				for (PatchVect::iterator it2 = patches.begin();
					it2 != patches.end();
					++it2)
				{
					BankPatchState * curItem = *it2;
					if (!curItem || !curItem->mPatch)
						continue;

					const int patchNum = curItem->mPatch->GetNumber();
					if (once)
					{
						// primary patch
						once = false;
						if (temporaryDisplay)
							curItem->mPatch->ClearSwitch(nullptr);
						if (ssPrimary == ss && ssPrimary == (*it).second.mCurrentSwitchState)
						{
							curItem->mPatch->AssignSwitch((*it).first, switchDisplay);
							if (curItem->mOverrideSwitchName.length() && switchDisplay)
								switchDisplay->SetSwitchText((*it).first, curItem->mOverrideSwitchName);
						}
						else if (ssSecondary == ss && ssSecondary == (*it).second.mCurrentSwitchState)
						{
							curItem->mPatch->AssignSwitch((*it).first, switchDisplay);
							if (curItem->mOverrideSwitchName.length() && switchDisplay)
								switchDisplay->SetSwitchText((*it).first, curItem->mOverrideSwitchName);
						}

						info << "sw" << std::setw(2) << ((*it).first + 1);
						if (ssSecondary == idx)
							info << "-2:";
						else
							info << ":  ";
						if (patchNum > 0)
							info << std::setw(3) << patchNum << " ";
		
						if (temporaryDisplay)
						{
//							curItem->mPatch->AssignSwitch(-1, NULL);
							curItem->mPatch->ClearSwitch(nullptr);
						}
					}
					else
					{
						// secondary patches
						info << "       ";
						if (patchNum > 0)
							info << std::setw(3) << patchNum << " ";
					}

					info << curItem->mPatch->GetName() << std::endl;
				}
			}
		}
	}

	info << std::ends;
	if (mainDisplay)
		mainDisplay->TextOut(info.str());
}

void
PatchBank::DisplayDetailedPatchInfo(int switchNumber, IMainDisplay * mainDisplay)
{
	std::ostrstream info;
	info << "Status for bank " << std::setw(2) << mNumber << " '" << mName << "'" 
		<< std::endl << "switch " << std::setw(2) << (switchNumber + 1) << std::endl;

	for (int idx = ssPrimary; idx < ssCount; ++idx)
	{
		for (PatchMaps::iterator it = mPatches.begin();
			it != mPatches.end();
			++it)
		{
			if ((*it).first != switchNumber)
				continue;

			int cnt = 0;
			SwitchFunctionAssignment ss = static_cast<SwitchFunctionAssignment>(idx);
			PatchVect & patches = (*it).second.GetPatchVect(ss);
			if (ssSecondary == ss && !patches.empty())
				info << "switch " << std::setw(2) << (switchNumber + 1) << " secondary" << std::endl;

			for (PatchVect::iterator it2 = patches.begin();
				it2 != patches.end();
				++it2)
			{
				BankPatchState * curItem = *it2;
				if (!curItem || !curItem->mPatch)
					continue;

				if (cnt == 0)
					info << "Num On/Off Type      Name" << std::endl;

// 				if (cnt == 1)
// 					info << "(Hidden patches)" << std::endl;

				const int patchNum = curItem->mPatch->GetNumber();
				if (patchNum > 0)
					info << std::setw(3) << patchNum << " ";

				info << (curItem->mPatch->IsActive() ? "ON     " : "off    ") 
					<< std::setiosflags(std::ios::left) << std::setw(10) << curItem->mPatch->GetPatchTypeStr() 
					<< curItem->mPatch->GetName() << std::endl;

				++cnt;
			}

			break;
		}
	}

	info << std::ends;
	if (mainDisplay)
		mainDisplay->TextOut(info.str());
}

void
PatchBank::ResetPatches(IMainDisplay * mainDisplay, 
						ISwitchDisplay * switchDisplay)
{
	for (PatchMaps::iterator it = mPatches.begin();
		it != mPatches.end();
		++it)
	{
		for (int idx = ssPrimary; idx < ssCount; ++idx)
		{
			SwitchFunctionAssignment ss = static_cast<SwitchFunctionAssignment>(idx);
			PatchVect & patches = (*it).second.GetPatchVect(ss);
			for (PatchVect::iterator it2 = patches.begin();
				it2 != patches.end();
				++it2)
			{
				BankPatchState * curItem = *it2;
				if (!curItem || !curItem->mPatch)
					continue;

				curItem->mPatch->UpdateState(switchDisplay, false);
			}
		}
	}

	if (mainDisplay)
	{
		std::ostrstream info;
		info << "Bank patches reset" << std::endl << std::ends;
		mainDisplay->TextOut(info.str());
	}
}

void
PatchBank::CreateExclusiveGroup(GroupSwitches * switchNumbers)
{
	mGroups.push_front(switchNumbers);

	for (GroupSwitches::iterator switchIt = switchNumbers->begin();
		switchIt != switchNumbers->end();
		++switchIt)
	{
		const int kCurSwitchNumber = *switchIt;
		mGroupFromSwitch[kCurSwitchNumber] = switchNumbers;
	}
}

void
PatchBank::ResetExclusiveGroup(ISwitchDisplay * switchDisplay, 
							   int switchNumberToSet)
{
	GroupSwitches * grp = mGroupFromSwitch[switchNumberToSet];
	if (!grp)
		return;

	for (GroupSwitches::iterator switchIt = grp->begin();
		switchIt != grp->end();
		++switchIt)
	{
		const int kCurSwitchNumber = *switchIt;
		const bool enabled = kCurSwitchNumber == switchNumberToSet;

		// exclusive groups don't really support secondary functions.
		// keep secondary function groups separate from primary function groups.
		// basically unsupported for now.
		PatchVect & prevPatches = mPatches[kCurSwitchNumber].GetPatchVect(ssPrimary);
		for (PatchVect::iterator it = prevPatches.begin();
			it != prevPatches.end();
			++it)
		{
			BankPatchState * curSwitchItem = *it;
			if (!curSwitchItem || !curSwitchItem->mPatch)
				continue;

			curSwitchItem->mPatch->UpdateState(switchDisplay, enabled);
		}
	}
}
