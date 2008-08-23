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
				int midiOutPortNumber, 
				IMidiOut * midiOut, 
				const Bytes & midiStringA, 
				const Bytes & midiStringB) :
		TwoStatePatch(number, name, midiOutPortNumber, midiOut, midiStringA, midiStringB, psAllowOnlyActive)
	{
	}

	virtual std::string GetPatchTypeStr() const { return "toggle"; }

	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
	{
		if (mPatchIsActive)
			SendStringB();
		else
			SendStringA();

		UpdateDisplays(mainDisplay, switchDisplay);
	}
};

#endif // TogglePatch_h__
