/*
 * mTroll MIDI Controller
 * Copyright (C) 2020,2024 Sean Echevarria
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

#ifndef AxeMomentaryPatch_h__
#define AxeMomentaryPatch_h__

#include "AxeTogglePatch.h"


// MomentaryPatch
// -----------------------------------------------------------------------------
// responds to SwitchPressed and SwitchReleased
// No expression pedal support
// This class was originally a descendant of MomentaryPatch but changed to a descendent 
// of AxeTogglePatch in order to support hybrid toggle/momentary patch types.
//
class AxeMomentaryPatch : public AxeTogglePatch
{
public:
	AxeMomentaryPatch(int number,
			const std::string & name,
			IMidiOutPtr midiOut,
			PatchCommands & cmdsA,
			PatchCommands & cmdsB,
			IAxeFxPtr axeMgr,
			int isScenePatch) :
		AxeTogglePatch(number, name, midiOut, cmdsA, cmdsB, axeMgr, isScenePatch, TogglePatch::PatchLogicStyle::Momentary)
	{
	}
};

#endif // AxeMomentaryPatch_h__
