/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2009 Sean Echevarria
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
#include "IMidiOut.h"
#include "IMainDisplay.h"
#include "ISwitchDisplay.h"
#include "ITraceDisplay.h"


ExpressionPedals * gActivePatchPedals = NULL;

Patch::Patch(int number, 
			 const std::string & name,
			 IMidiOut * midiOut /*= NULL*/) :
	mNumber(number),
	mName(name),
	mPatchIsActive(false),
	mPedals(midiOut)
{
}

Patch::~Patch()
{
}

void
Patch::AssignSwitch(int switchNumber, ISwitchDisplay * switchDisplay)
{
	_ASSERTE(switchNumber != -1 && switchNumber >= 0);
	mSwitchNumbers.push_back(switchNumber);
	if (switchDisplay)
		switchDisplay->SetSwitchText(switchNumber, mName);
	UpdateDisplays(NULL, switchDisplay);
}

void
Patch::ClearSwitch(ISwitchDisplay * switchDisplay)
{
	if (mSwitchNumbers.empty())
		return;

	if (switchDisplay)
	{
		for (std::vector<int>::iterator it = mSwitchNumbers.begin(); 
			it != mSwitchNumbers.end();
			++it)
		{
			switchDisplay->SetSwitchDisplay(*it, false);
			switchDisplay->ClearSwitchText(*it);
		}
	}

	mSwitchNumbers.clear();
}

void
Patch::UpdateDisplays(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	if (mSwitchNumbers.empty())
		return;

	if (switchDisplay)
	{
		for (std::vector<int>::iterator it = mSwitchNumbers.begin(); 
			it != mSwitchNumbers.end();
			++it)
		{
			switchDisplay->SetSwitchDisplay(*it, mPatchIsActive);
		}
	}

	if (mainDisplay)
	{
		std::strstream msgstr;
		msgstr << mNumber << " " << mName << std::endl << std::ends;
		mainDisplay->TextOut(msgstr.str());
	}
}

void
Patch::Activate(IMainDisplay * mainDisplay, 
				ISwitchDisplay * switchDisplay)
{
	if (IsActive())
		return;

	SwitchPressed(mainDisplay, switchDisplay);
}

void
Patch::Deactivate(IMainDisplay * mainDisplay, 
				  ISwitchDisplay * switchDisplay)
{
	if (!IsActive())
		return;

	SwitchPressed(mainDisplay, switchDisplay);
}
