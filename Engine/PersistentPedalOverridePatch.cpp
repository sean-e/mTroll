/*
 * mTroll MIDI Controller
 * Copyright (C) 2015,2025 Sean Echevarria
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


PersistentPedalOverridePatch* PersistentPedalOverridePatch::sActiveOverride[ExpressionPedals::PedalCount] = { nullptr };
ExpressionPedals* PersistentPedalOverridePatch::sInactivePedals = nullptr;
ExpressionPedalAggregate* PersistentPedalOverridePatch::sAggregateOverridePedals = nullptr;


void
PersistentPedalOverridePatch::SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	const PersistentPedalOverridePatch * prev[ExpressionPedals::PedalCount] = { sActiveOverride[0], sActiveOverride[1], sActiveOverride[2], sActiveOverride[3] };

	TogglePatch::SwitchPressed(mainDisplay, switchDisplay);

	for (int idx = 0; idx < ExpressionPedals::PedalCount; ++idx)
	{
		if (prev[idx] && prev[idx] != sActiveOverride[idx])
		{
			// update switch of override patch that this one replaced
			prev[idx]->UpdateDisplays(nullptr, switchDisplay);
		}
	}
}

void
PersistentPedalOverridePatch::ExecCommandsA()
{
	for (int idx = 0; idx < ExpressionPedals::PedalCount; ++idx)
	{
		if (sActiveOverride[idx] && sActiveOverride[idx] != this && 
			(mPedals.IsLocalPedalEnabled(idx) || mPedals.IsGlobalPedalDisabled(idx)))
		{
			auto prev = sActiveOverride[idx];

			// replace the old override -- to short-circuit the reset logic in ExecCommandsB
			sActiveOverride[idx] = this;

			// update state of override patch that this one replaced
			prev->OverridePedals(true);
			prev->ExecCommandsB();
			prev->OverridePedals(false);
		}
	}

	if (sAggregateOverridePedals)
		sAggregateOverridePedals->OverridePedals(mPedals);

	if (gActivePatchPedals != sAggregateOverridePedals)
	{
		sInactivePedals = gActivePatchPedals;
		gActivePatchPedals = sAggregateOverridePedals;
	}

	OverridePedals(true);
	TogglePatch::ExecCommandsA();
	OverridePedals(false);

	_ASSERTE(gActivePatchPedals == sAggregateOverridePedals); // assert if the exec changed it underneath us
	for (int idx = 0; idx < ExpressionPedals::PedalCount; ++idx)
		if ((mPedals.IsLocalPedalEnabled(idx) || mPedals.IsGlobalPedalDisabled(idx)) && sActiveOverride[idx] != this)
			sActiveOverride[idx] = this;
}

void
PersistentPedalOverridePatch::ExecCommandsB()
{
	_ASSERTE(gActivePatchPedals == sAggregateOverridePedals);

	OverridePedals(true);
	TogglePatch::ExecCommandsB();
	OverridePedals(false);

	_ASSERTE(sInactivePedals != &mPedals);
	_ASSERTE(sInactivePedals != sAggregateOverridePedals);

	if (sAggregateOverridePedals)
		sAggregateOverridePedals->ClearPedalOverrides(mPedals);

	for (int idx = 0; idx < ExpressionPedals::PedalCount; ++idx)
		if (sActiveOverride[idx] == this && (mPedals.IsLocalPedalEnabled(idx) || mPedals.IsGlobalPedalDisabled(idx)))
			sActiveOverride[idx] = nullptr;

	// see if any other overrides are active, return if so
	for (auto & cur : sActiveOverride)
		if (cur)
			return;

	// else, reset to default non-persistent behavior
	gActivePatchPedals = sInactivePedals;
	sInactivePedals = nullptr;
}
