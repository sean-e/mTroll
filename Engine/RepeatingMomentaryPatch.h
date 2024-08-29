/*
 * mTroll MIDI Controller
 * Copyright (C) 2023-2024 Sean Echevarria
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

#ifndef RepeatingMomentaryPatch_h__
#define RepeatingMomentaryPatch_h__

#include "RepeatingPatch.h"


 // RepeatingMomentaryPatch
 // -----------------------------------------------------------------------------
 // responds to SwitchPressed and SwitchReleased
 // No expression pedal support
 //
class RepeatingMomentaryPatch : public RepeatingPatch
{
public:
	RepeatingMomentaryPatch(int number,
		const std::string & name,
		IMidiOutPtr midiOut,
		PatchCommands & cmdsA,
		PatchCommands & cmdsB) :
		RepeatingPatch(number, name, midiOut, cmdsA, cmdsB)
	{
	}

	virtual std::string GetPatchTypeStr() const override { return "repeatingMomentary"; }
	virtual bool SetGroupId(const std::string &) override { return false; } // doesn't support grouping

	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) override
	{
		if (StartThread())
		{
			UpdateDisplays(mainDisplay, switchDisplay);
		}
	}

	virtual void SwitchReleased(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) override
	{
		if (StopThread())
		{
			ExecCommandsB();
			UpdateDisplays(mainDisplay, switchDisplay);
		}
	}

	virtual void Deactivate(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) override
	{
		if (!IsActive())
			return;

		SwitchReleased(mainDisplay, switchDisplay);
	}
};

#endif // RepeatingMomentaryPatch_h__
