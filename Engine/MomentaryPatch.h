/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2010,2018 Sean Echevarria
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

#ifndef MomentaryPatch_h__
#define MomentaryPatch_h__

#include "TwoStatePatch.h"


// MomentaryPatch
// -----------------------------------------------------------------------------
// responds to SwitchPressed and SwitchReleased
// No expression pedal support
//
class MomentaryPatch : public TwoStatePatch
{
public:
	MomentaryPatch(int number, 
					const std::string & name, 
					IMidiOutPtr midiOut, 
					PatchCommands & cmdsA, 
					PatchCommands & cmdsB) :
		TwoStatePatch(number, name, midiOut, cmdsA, cmdsB, psDisallow)
	{
	}

	virtual std::string GetPatchTypeStr() const override { return "momentary"; }
	
	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) override
	{
		ExecCommandsA();
		UpdateDisplays(mainDisplay, switchDisplay);
	}

	virtual void SwitchReleased(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) override
	{
		ExecCommandsB();
		UpdateDisplays(mainDisplay, switchDisplay);
	}

	virtual void Deactivate(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) override
	{
		if (!IsActive())
			return;

		SwitchReleased(mainDisplay, switchDisplay);
	}
};

#endif // MomentaryPatch_h__
