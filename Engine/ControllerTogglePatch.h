/*
 * mTroll MIDI Controller
 * Copyright (C) 2022,2025 Sean Echevarria
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

#ifndef ControllerTogglePatch_h__
#define ControllerTogglePatch_h__

#include <memory>
#include "TogglePatch.h"
#include "MidiCommandString.h"


 // ControllerTogglePatch
 // -----------------------------------------------------------------------------
 // responds to SwitchPressed; SwitchReleased does not affect patch state
 // supports expression pedals (psAllowOnlyActive) - but should it?
 //
class ControllerTogglePatch : public TogglePatch
{
public:
	ControllerTogglePatch(int number,
		const std::string & name,
		IMidiOutPtr midiOut,
		int channel,
		int controller) :
		TogglePatch(number, name, midiOut)
	{
		Bytes bytesA, bytesB;
		bytesA.push_back(0xb0 | (byte)channel);
		bytesA.push_back((byte)controller);
		bytesA.push_back(127);

		bytesB.push_back(0xb0 | (byte)channel);
		bytesB.push_back((byte)controller);
		bytesB.push_back(0);

		mCmdsA.push_back(std::make_shared<MidiCommandString>(midiOut, bytesA));
		mCmdsB.push_back(std::make_shared<MidiCommandString>(midiOut, bytesB));
	}

	virtual std::string GetPatchTypeStr() const override { return "controllerToggle"; }
};

using ControllerTogglePatchPtr = std::shared_ptr<ControllerTogglePatch>;

#endif // ControllerTogglePatch_h__
