/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2008,2010-2016,2018,2021,2024-2025 Sean Echevarria
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
#include <format>
#include <iomanip>
#include <atomic>
#include "PatchBank.h"
#include "Patch.h"

#include "IMainDisplay.h"
#include "ISwitchDisplay.h"
#include "ITraceDisplay.h"


#ifdef ITEM_COUNTING
std::atomic<int> gPatchBankCnt;
#endif

PatchBank::PatchBank(MidiControlEnginePtr eng,
					 int number, 
					 const std::string & name, 
					 const std::string & additionalText) :
	mEngine(eng),
	mNumber(number),
	mName(name),
	mAdditionalText(additionalText),
	mLoaded(false)
{
#ifdef ITEM_COUNTING
	++gPatchBankCnt;
#endif
}

PatchBank::~PatchBank()
{
#ifdef ITEM_COUNTING
	--gPatchBankCnt;
#endif
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
	curPatches.push_back(std::make_shared<BankPatchState>(patchNumber, overrideName, patchLoadState, patchUnloadState, patchStateOverride, patchSyncState, sfoOp));
	if (ssSecondary == st)
	{
		if (!SwitchHasSecondaryLogic(switchNumber))
			mPatches[switchNumber].mSfOp = sfoOp; // first secondary patch sets the op for all secondary patches

		_ASSERTE(mPatches[switchNumber].mSfOp == sfoOp);
	}
}

void
PatchBank::SetDefaultMappings(const PatchBankPtr defaultMapping)
{
	_ASSERTE(defaultMapping);
	// propagate switch groups from default bank
	if (defaultMapping.get() != this && !defaultMapping->mGroups.empty())
	{
		std::for_each(defaultMapping->mGroups.begin(), defaultMapping->mGroups.end(),
			[this](GroupSwitchesPtr grp)
		{
			bool useThisMapping = true;
			PatchMaps & pm2 = mPatches;
			SwitchToGroupMap & nonDefaultMappings = mGroupFromSwitch;
			GroupSwitchesPtr grpCpy = std::make_shared<GroupSwitches>();
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
		});
	}

	std::set<int> skippedPrimarySwitches;
	for (int idx = ssPrimary; idx < ssCount; ++idx)
	{
		SwitchFunctionAssignment ss = static_cast<SwitchFunctionAssignment>(idx);

		for (PatchMaps::const_iterator it = defaultMapping->mPatches.begin();
			it != defaultMapping->mPatches.end();
			++it)
		{
			const int switchNumber = (*it).first;
			PatchVect & curPatches = mPatches[switchNumber].GetPatchVect(ss);
			if (!curPatches.empty())
			{
				if (ssPrimary == ss)
				{
					// save cur switchNumber so that we know not to modify ssSecondary behavior
					skippedPrimarySwitches.insert(switchNumber);
				}

				continue;
			}

			if (ssSecondary == ss)
			{
				if (skippedPrimarySwitches.find(switchNumber) != skippedPrimarySwitches.end())
				{
					// switchNumber was skipped in ssPrimary loop, so skip for ssSecondary also
					// (otherwise secondary assignments in default bank could be added even
					// though switch is overridden, primary only, in current bank definition)
					continue;
				}

				mPatches[switchNumber].mSfOp = (*it).second.mSfOp;
			}

			const PatchVect & patches = (*it).second.GetPatchVectConst(ss);
			for (const BankPatchStatePtr& curItem : patches)
				curPatches.push_back(std::make_shared<BankPatchState>(*curItem));
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
				BankPatchStatePtr curItem = *it2;
				if (curItem)
				{
					MidiControlEngine::Patches::const_iterator patchIt = enginePatches.find(curItem->mPatchNumber);
					if (patchIt == enginePatches.end())
					{
						if (curItem->mPatchNumber != -1)
						{
							if (traceDisp)
								traceDisp->Trace(std::format("Patch {} referenced in bank {} ({}) does not exist!\n", curItem->mPatchNumber, mName, mNumber));

							inc = false;
							it2 = patches.erase(it2);
						}
					}
					else
					{
						PatchPtr thePatch = (*patchIt).second;
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
PatchBank::CalibrateExprSettings(const PedalCalibration * pedalCalibration, ITraceDisplay * traceDisp)
{
	for (auto & curPatch : mPatches)
	{
		for (int idx = ssPrimary; idx < ssCount; ++idx)
		{
			SwitchFunctionAssignment ss = static_cast<SwitchFunctionAssignment>(idx);
			PatchVect & patches = curPatch.second.GetPatchVect(ss);
			for (const BankPatchStatePtr& curItem : patches)
			{
				if (curItem && curItem->mPatch)
				{
					ExpressionPedals & pedals = curItem->mPatch->GetPedals();
					pedals.Calibrate(pedalCalibration, mEngine.get(), traceDisp);
				}
			}
		}
	}
}

void
PatchBank::Load(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	mLoaded = true;
	for (auto & curPatch : mPatches)
	{
		for (int idx = ssPrimary; idx < ssCount; ++idx)
		{
			bool once = true;
			SwitchFunctionAssignment ss = static_cast<SwitchFunctionAssignment>(idx);
			PatchVect & patches = curPatch.second.GetPatchVect(ss);
			for (const BankPatchStatePtr& curItem : patches)
			{
				if (!curItem || !curItem->mPatch)
					continue;

				if (!once)
					curItem->mPatch->OverridePedals(true); // expression pedals only apply to first patch

				if (stA == curItem->mPatchStateAtBankLoad)
				{
					curItem->mPatch->BankTransitionActivation();
					mEngine->CheckForDeviceProgramChange(curItem.get()->mPatch, switchDisplay, mainDisplay);
				}
				else if (stB == curItem->mPatchStateAtBankLoad)
					curItem->mPatch->BankTransitionDeactivation();

				// only assign a switch to the first patch (if multiple 
				// patches are assigned to the same switch)
				if (once && ssPrimary == ss)
				{
					once = false;
					curItem->mPatch->AssignSwitch(curPatch.first, switchDisplay);
					if (curItem->mOverrideSwitchName.length() && switchDisplay)
						switchDisplay->SetSwitchText(curPatch.first, curItem->mOverrideSwitchName);
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
	for (auto & curPatch : mPatches)
	{
		for (int idx = ssPrimary; idx < ssCount; ++idx)
		{
			bool once = true;
			SwitchFunctionAssignment ss = static_cast<SwitchFunctionAssignment>(idx);
			PatchVect & patches = curPatch.second.GetPatchVect(ss);
			for (const BankPatchStatePtr& curItem : patches)
			{
				if (!curItem || !curItem->mPatch)
					continue;

				if (!once)
					curItem->mPatch->OverridePedals(true);

				if (stA == curItem->mPatchStateAtBankUnload)
				{
					curItem->mPatch->BankTransitionActivation();
					mEngine->CheckForDeviceProgramChange(curItem.get()->mPatch, switchDisplay, mainDisplay);
				}
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
PatchBank::ReengageSwitchesWithoutLoad(ISwitchDisplay * switchDisplay)
{
	_ASSERTE(mLoaded);
	if (!mLoaded)
		return;

	for (auto & curPatch : mPatches)
	{
		for (int idx = ssPrimary; idx < ssCount; ++idx)
		{
			bool once = true;
			SwitchFunctionAssignment ss = static_cast<SwitchFunctionAssignment>(idx);
			PatchVect & patches = curPatch.second.GetPatchVect(ss);
			for (const BankPatchStatePtr& curItem : patches)
			{
				if (!curItem || !curItem->mPatch)
					continue;

				// only assign a switch to the first patch (if multiple 
				// patches are assigned to the same switch)
				if (once && ssPrimary == ss)
				{
					once = false;
					curItem->mPatch->AssignSwitch(curPatch.first, switchDisplay);
					if (curItem->mOverrideSwitchName.length() && switchDisplay)
						switchDisplay->SetSwitchText(curPatch.first, curItem->mOverrideSwitchName);
				}
			}
		}
	}
}

void
PatchBank::DisengageSwitchesWithoutUnload(ISwitchDisplay * switchDisplay)
{
	_ASSERTE(mLoaded);
	if (!mLoaded)
		return;

	for (auto & curPatch : mPatches)
	{
		for (int idx = ssPrimary; idx < ssCount; ++idx)
		{
			bool once = true;
			SwitchFunctionAssignment ss = static_cast<SwitchFunctionAssignment>(idx);
			PatchVect & patches = curPatch.second.GetPatchVect(ss);
			for (const BankPatchStatePtr& curItem : patches)
			{
				if (!curItem || !curItem->mPatch)
					continue;

				curItem->mPatch->ClearSwitch(switchDisplay);
			}
		}
	}
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

	// if any patch for the switch is normal, then need to do normal processing
	// on current normal patches.
	for (it = curPatches.begin();
		 it != curPatches.end();
		 ++it)
	{
		BankPatchStatePtr curSwitchItem = *it;
		if (!curSwitchItem || !curSwitchItem->mPatch)
			continue;

		if (curSwitchItem->mPatch->IsPatchVolatile())
		{
			// do B processing
			mEngine->DeactivateVolatilePatches(mainDisplay, switchDisplay);
			break;
		}
	}

	GroupSwitchesPtr grp = mGroupFromSwitch[switchNumber];
	if (grp)
	{
		bool isRepress = false;
		bool isAnyPatchInGroupActiveThatIsNotCurrentSwitch = false;
		for (const int kCurSwitchNumber : *grp)
		{
			PatchVect & prevPatches = mPatches[kCurSwitchNumber].GetPatchVect(st);
			for (it = prevPatches.begin();
				it != prevPatches.end();
				++it)
			{
				BankPatchStatePtr curSwitchItem = *it;
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
			for (const int kCurSwitchNumber : *grp)
			{
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
					BankPatchStatePtr curSwitchItem = *it;
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
		BankPatchStatePtr curSwitchItem = *it;
		if (curSwitchItem && curSwitchItem->mPatch && curSwitchItem->mPatch->IsActive())
			masterPatchIsActive = true; // master patch is just the first
	}

	// do standard pressed processing (send A)
	bool once = (st == ssPrimary);
	std::string msgstr;
	for (it = curPatches.begin();
		 it != curPatches.end();
		 ++it)
	{
		BankPatchStatePtr curSwitchItem = *it;
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
			mEngine->VolatilePatchIsActive(curSwitchItem->mPatch);

		mEngine->CheckForDeviceProgramChange(curSwitchItem.get()->mPatch, switchDisplay, mainDisplay);

		if (curSwitchItem->mPatch->UpdateMainDisplayOnPress())
		{
			const std::string txt(curSwitchItem->mPatch->GetDisplayText(true));
			msgstr += txt;
			const int patchNum = curSwitchItem->mPatch->GetNumber();
			if (patchNum > 0)
			{
#ifdef _DEBUG
				if (txt.empty())
					std::format_to(std::back_inserter(msgstr), "(off)   ({})", patchNum);
				else
					std::format_to(std::back_inserter(msgstr), "   ({})", patchNum);
#else
				if (txt.empty())
					msgstr += "(off)";
#endif
			}
			msgstr += '\n';
		}
		else
			doDisplayUpdate = false;
	}

	if (mainDisplay && doDisplayUpdate)
		mainDisplay->TextOut(msgstr);
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
					for (const BankPatchStatePtr& curItem : patches)
					{
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
	for (const BankPatchStatePtr& curSwitchItem : curPatches)
	{
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
		for (const BankPatchStatePtr& curItem : patches)
		{
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
		for (const BankPatchStatePtr& curItem : patches)
		{
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
	std::string info;
#ifdef _DEBUG
	info = std::format("{}   (bank {})", mName, mNumber);
#else
	info = mName;
#endif

	if (!mAdditionalText.empty())
	{
		info += '\n';
		info += mAdditionalText;
	}

	if (showPatchInfo)
	{
		info += '\n';
		for (auto & curPatch : mPatches)
		{
			for (int idx = ssPrimary; idx < ssCount; ++idx)
			{
				SwitchFunctionAssignment ss = static_cast<SwitchFunctionAssignment>(idx);
				PatchVect & patches = curPatch.second.GetPatchVect(ss);
				bool once = true;
				for (const BankPatchStatePtr& curItem : patches)
				{
					if (!curItem || !curItem->mPatch)
						continue;

					const int patchNum = curItem->mPatch->GetNumber();
					if (once)
					{
						// primary patch
						once = false;
						if (temporaryDisplay)
							curItem->mPatch->ClearSwitch(nullptr);
						if (ssPrimary == ss && ssPrimary == curPatch.second.mCurrentSwitchState)
						{
							curItem->mPatch->AssignSwitch(curPatch.first, switchDisplay);
							if (curItem->mOverrideSwitchName.length() && switchDisplay)
								switchDisplay->SetSwitchText(curPatch.first, curItem->mOverrideSwitchName);
						}
						else if (ssSecondary == ss && ssSecondary == curPatch.second.mCurrentSwitchState)
						{
							curItem->mPatch->AssignSwitch(curPatch.first, switchDisplay);
							if (curItem->mOverrideSwitchName.length() && switchDisplay)
								switchDisplay->SetSwitchText(curPatch.first, curItem->mOverrideSwitchName);
						}

						std::format_to(std::back_inserter(info), "sw{:2}", curPatch.first + 1);
						if (ssSecondary == idx)
							info += "-2:";
						else
							info += ":  ";
		
						if (temporaryDisplay)
						{
//							curItem->mPatch->AssignSwitch(-1, NULL);
							curItem->mPatch->ClearSwitch(nullptr);
						}
					}
					else
					{
						// secondary patches
						info += "       ";
					}

					info += curItem->mPatch->GetName();
#ifdef _DEBUG
					if (patchNum > 0)
						std::format_to(std::back_inserter(info), "   ({})\n", patchNum);
					else
#endif
						info += '\n';
				}
			}
		}
	}

	if (mainDisplay)
		mainDisplay->TextOut(info);
}

void
PatchBank::DisplayDetailedPatchInfo(int switchNumber, IMainDisplay * mainDisplay)
{
	std::string info = std::format(
#ifdef _DEBUG
		"Patch info for bank '{}' ({}), switch {}:\n", mName, mNumber, (switchNumber + 1)
#else
		"Patch info for bank '{}', switch {}:\n", mName, (switchNumber + 1)
#endif
	);

	bool displayedPatchheader = false;
	for (int idx = ssPrimary; idx < ssCount; ++idx)
	{
		for (auto & curPatch : mPatches)
		{
			if (curPatch.first != switchNumber)
				continue;

			int cnt = 0;
			SwitchFunctionAssignment ss = static_cast<SwitchFunctionAssignment>(idx);
			PatchVect & patches = curPatch.second.GetPatchVect(ss);
			if (ssSecondary == ss && !patches.empty())
				info += "secondary switch functions:\n";

			for (const BankPatchStatePtr& curItem : patches)
			{
				if (!curItem || !curItem->mPatch)
					continue;

				if (cnt == 0 && !displayedPatchheader)
				{
					info += "Status / Type / Name (num)\n";
					displayedPatchheader = true;
				}

// 				if (cnt == 1)
// 					info += "(Hidden patches)\n";

				 
				std::format_to(std::back_inserter(info), "{}{:<19}  {}", (curItem->mPatch->IsActive() ? "ON   " : "off  "), curItem->mPatch->GetPatchTypeStr(), curItem->mPatch->GetName());

#ifdef _DEBUG
				const int patchNum = curItem->mPatch->GetNumber();
				if (patchNum > 0)
					std::format_to(std::back_inserter(info), "   ({})\n", patchNum);
				else
#endif
					info += '\n';

				++cnt;
			}

			break;
		}
	}

	if (mainDisplay)
		mainDisplay->TextOut(info);
}

void
PatchBank::ResetPatches(IMainDisplay * mainDisplay, 
						ISwitchDisplay * switchDisplay)
{
	for (auto & curPatch : mPatches)
	{
		for (int idx = ssPrimary; idx < ssCount; ++idx)
		{
			SwitchFunctionAssignment ss = static_cast<SwitchFunctionAssignment>(idx);
			PatchVect & patches = curPatch.second.GetPatchVect(ss);
			for (const BankPatchStatePtr& curItem : patches)
			{
				if (!curItem || !curItem->mPatch)
					continue;

				curItem->mPatch->UpdateState(switchDisplay, false);
			}
		}
	}

	if (mainDisplay)
		mainDisplay->TextOut("Bank patches reset\n");
}

void
PatchBank::CreateExclusiveGroup(GroupSwitchesPtr switchNumbers)
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
	GroupSwitchesPtr grp = mGroupFromSwitch[switchNumberToSet];
	if (!grp)
		return;

	for (const int kCurSwitchNumber : *grp)
	{
		const bool enabled = kCurSwitchNumber == switchNumberToSet;

		// exclusive groups don't really support secondary functions.
		// keep secondary function groups separate from primary function groups.
		// basically unsupported for now.
		PatchVect & prevPatches = mPatches[kCurSwitchNumber].GetPatchVect(ssPrimary);
		for (const BankPatchStatePtr& curSwitchItem : prevPatches)
		{
			if (!curSwitchItem || !curSwitchItem->mPatch)
				continue;

			curSwitchItem->mPatch->UpdateState(switchDisplay, enabled);
		}
	}
}
