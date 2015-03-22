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

#include "PersistentPedalOverridePatch.h"


PersistentPedalOverridePatch* PersistentPedalOverridePatch::sActiveOverride = nullptr;
ExpressionPedals* PersistentPedalOverridePatch::sInactivePedals = nullptr;


void
PersistentPedalOverridePatch::SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	PersistentPedalOverridePatch * prev = sActiveOverride;

	__super::SwitchPressed(mainDisplay, switchDisplay);

	if (prev && prev != sActiveOverride)
		prev->UpdateDisplays(nullptr, switchDisplay);
}

void
PersistentPedalOverridePatch::ExecCommandsA()
{
	if (sActiveOverride && sActiveOverride != this)
	{
		sActiveOverride->ExecCommandsB();
		sInactivePedals = gActivePatchPedals;
		gActivePatchPedals = &mPedals;
	}
	else if (gActivePatchPedals != &mPedals)
		sInactivePedals = gActivePatchPedals;

	__super::ExecCommandsA();

	_ASSERTE(gActivePatchPedals == &mPedals);
	sActiveOverride = this;
}

void
PersistentPedalOverridePatch::ExecCommandsB()
{
	_ASSERTE(sActiveOverride == this);
	_ASSERTE(gActivePatchPedals == &mPedals);

	__super::ExecCommandsB();

	_ASSERTE(sInactivePedals != &mPedals);
	gActivePatchPedals = sInactivePedals;
	sInactivePedals = nullptr;
	sActiveOverride = nullptr;
}
