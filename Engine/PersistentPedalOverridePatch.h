/*
 * mTroll MIDI Controller
 * Copyright (C) 2015 Sean Echevarria
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

#ifndef PersistentPedalOverridePatch_h__
#define PersistentPedalOverridePatch_h__

#include "TogglePatch.h"


// PersistentPedalOverridePatch
// -----------------------------------------------------------------------------
// responds to SwitchPressed; SwitchReleased does not affect patch state
// supports expression pedals - it exists for expression pedals
//
class PersistentPedalOverridePatch : public TogglePatch
{
public:
	PersistentPedalOverridePatch(int number,
		const std::string & name,
		IMidiOut * midiOut,
		PatchCommands & cmdsA,
		PatchCommands & cmdsB) :
		TogglePatch(number, name, midiOut, cmdsA, cmdsB)
	{
	}

	virtual ~PersistentPedalOverridePatch()
	{
		if (sActiveOverride == this)
			sActiveOverride = nullptr;
	}

	virtual std::string GetPatchTypeStr() const { return "persistentPedalOverride"; }

	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);

	virtual void ExecCommandsA();
	virtual void ExecCommandsB();

	static bool PedalOverridePatchIsActive() { return sActiveOverride != nullptr; }
	static void SetInactivePedals(ExpressionPedals * newPedals)
	{
		sInactivePedals = newPedals;
	}

private:
	static PersistentPedalOverridePatch *sActiveOverride;
	static ExpressionPedals				*sInactivePedals;
};

#endif // PersistentPedalOverridePatch_h__
