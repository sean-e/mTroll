/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2009,2017-2018 Sean Echevarria
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

#ifndef TogglePatch_h__
#define TogglePatch_h__

#include "TwoStatePatch.h"


// TogglePatch
// -----------------------------------------------------------------------------
// responds to SwitchPressed; SwitchReleased does not affect patch state
// supports expression pedals (psAllowOnlyActive) - but should it?
//
class TogglePatch : public TwoStatePatch
{
public:
	TogglePatch(int number, 
				const std::string & name, 
				IMidiOutPtr midiOut, 
				PatchCommands & cmdsA, 
				PatchCommands & cmdsB) :
		TwoStatePatch(number, name, midiOut, cmdsA, cmdsB, psAllowOnlyActive)
	{
	}

	virtual std::string GetPatchTypeStr() const override { return "toggle"; }

	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) override
	{
		if (IsActive())
			ExecCommandsB();
		else
			ExecCommandsA();

		UpdateDisplays(mainDisplay, switchDisplay);
	}

	virtual const std::string & GetDisplayText(bool checkState = false) const override
	{
		if (!checkState || IsActive())
			return GetName();

		// don't display the patch name when it has just been toggled off
		static const std::string empty("");
		return empty;
	}
};

#endif // TogglePatch_h__
