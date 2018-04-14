/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2012,2014-2015,2017-2018 Sean Echevarria
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

#include "Patch.h"
#include <strstream>
#include <atomic>
#include "IMidiOut.h"
#include "IMainDisplay.h"
#include "ISwitchDisplay.h"
#include "ITraceDisplay.h"


ExpressionPedals * gActivePatchPedals = nullptr;
#ifdef ITEM_COUNTING
std::atomic<int> gPatchCnt = 0;
#endif

Patch::Patch(int number, 
			 const std::string & name,
			 IMidiOut * midiOut /*= NULL*/) :
	mNumber(number),
	mName(name),
	mPatchIsActive(false),
	mPedals(midiOut),
	mOverridePedals(false),
	mPatchSupportsDisabledState(false),
	mPatchIsDisabled(false)
{
#ifdef ITEM_COUNTING
	++gPatchCnt;
#endif
}

Patch::~Patch()
{
#ifdef ITEM_COUNTING
	--gPatchCnt;
#endif
}

void
Patch::AssignSwitch(int switchNumber, ISwitchDisplay * switchDisplay)
{
	_ASSERTE(switchNumber != -1 && switchNumber >= 0);
	mSwitchNumbers.insert(switchNumber);
	if (switchDisplay)
		switchDisplay->SetSwitchText(switchNumber, GetDisplayText());
	UpdateDisplays(nullptr, switchDisplay);
}

void
Patch::RemoveSwitch(int switchNumber, ISwitchDisplay * switchDisplay)
{
	if (mSwitchNumbers.empty())
		return;

	if (switchDisplay)
	{
		switchDisplay->SetSwitchDisplay(switchNumber, false);
		switchDisplay->ClearSwitchText(switchNumber);
	}

	mSwitchNumbers.erase(switchNumber);
}

void
Patch::ClearSwitch(ISwitchDisplay * switchDisplay)
{
	if (mSwitchNumbers.empty())
		return;

	if (switchDisplay)
	{
		for (const int switchNumber : mSwitchNumbers)
		{
			switchDisplay->SetSwitchDisplay(switchNumber, false);
			switchDisplay->ClearSwitchText(switchNumber);
		}
	}

	mSwitchNumbers.clear();
}

void
Patch::UpdateDisplays(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) const
{
	if (mSwitchNumbers.empty())
		return;

	if (switchDisplay)
	{
		for (const int switchNumber : mSwitchNumbers)
		{
			if (mPatchSupportsDisabledState && !mPatchIsDisabled && !mPatchIsActive)
				switchDisplay->DimSwitchDisplay(switchNumber);
			else
				switchDisplay->SetSwitchDisplay(switchNumber, mPatchIsActive);

			if (HasDisplayText())
				switchDisplay->SetSwitchText(switchNumber, GetDisplayText());
		}
	}

	if (mainDisplay)
	{
		std::strstream msgstr;
		if (mNumber > 0)
			msgstr << mNumber << " ";
		msgstr << GetDisplayText(true) << std::endl << std::ends;
		mainDisplay->TextOut(msgstr.str());
	}
}

void
Patch::Deactivate(IMainDisplay * mainDisplay, 
				  ISwitchDisplay * switchDisplay)
{
	if (!IsActive())
		return;

	SwitchPressed(mainDisplay, switchDisplay);
}

void
Patch::ActivateSwitchDisplay(ISwitchDisplay * switchDisplay,
							 bool activate) const
{
	if (mSwitchNumbers.empty())
		return;

	if (switchDisplay)
	{
		for (const int switchNumber : mSwitchNumbers)
			switchDisplay->SetSwitchDisplay(switchNumber, activate);
	}
}

void
Patch::UpdateState(ISwitchDisplay * switchDisplay, bool active)
{
	if (mPatchIsActive == active)
	{
		if (!mPatchSupportsDisabledState)
			return;

		if (!mPatchIsDisabled)
			return;
	}

	mPatchIsActive = active;
	if (mPatchSupportsDisabledState && mPatchIsDisabled)
		mPatchIsDisabled = false;
	UpdateDisplays(nullptr, switchDisplay);
}

void
Patch::Disable(ISwitchDisplay * switchDisplay)
{
	_ASSERTE(mPatchSupportsDisabledState);
	if (!mPatchIsActive && mPatchIsDisabled)
		return;

	mPatchIsActive = false;
	mPatchIsDisabled = true;
	UpdateDisplays(nullptr, switchDisplay);
}
