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

#ifndef NormalPatch_h__
#define NormalPatch_h__

#include "TwoStatePatch.h"


// NormalPatch
// -----------------------------------------------------------------------------
// responds to SwitchPressed; SwitchReleased does not affect patch state
// 
class NormalPatch : public TwoStatePatch
{
public:
	NormalPatch(int number, 
				const std::string & name, 
				IMidiOut * midiOut, 
				PatchCommands & cmdsA, 
				PatchCommands & cmdsB) :
		TwoStatePatch(number, name, midiOut, cmdsA, cmdsB, psAllow)
	{
	}

	virtual std::string GetPatchTypeStr() const { return "normal"; }
	virtual bool IsPatchVolatile() const { return true; }
	virtual void DeactivateVolatilePatch() { ExecCommandsB(); }

	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
	{
		if (mPatchIsActive)
		{
			ExecCommandsB();
			ExecCommandsA();
		}
		else
			ExecCommandsA();

		UpdateDisplays(mainDisplay, switchDisplay);
	}
};

#endif // NormalPatch_h__
