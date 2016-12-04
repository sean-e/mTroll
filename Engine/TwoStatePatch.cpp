/*
* mTroll MIDI Controller
* Copyright (C) 2015-2016 Sean Echevarria
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

#include "TwoStatePatch.h"
#include "PersistentPedalOverridePatch.h"


void
TwoStatePatch::ExecCommandsA()
{
	mPatchIsActive = true;

	if (!mOverridePedals)
	{
		if (psDisallow != mPedalSupport)
		{
			ExpressionPedals * newPedals = nullptr;
			if (mPedals.HasAnySettings())
				newPedals = &mPedals;

			if (PersistentPedalOverridePatch::PedalOverridePatchIsActive())
			{
				PersistentPedalOverridePatch::SetInactivePedals(newPedals);
			}
			else
			{
				// do this here rather than SwitchPressed to that pedals can be
				// set on bank load rather than only during patch load
				gActivePatchPedals = newPedals;
			}
		}
	}

	std::for_each(mCmdsA.begin(), mCmdsA.end(), std::mem_fun(&IPatchCommand::Exec));
}

void
TwoStatePatch::ExecCommandsB()
{
	std::for_each(mCmdsB.begin(), mCmdsB.end(), std::mem_fun(&IPatchCommand::Exec));
	mPatchIsActive = false;

	if (!mOverridePedals)
	{
		if (psAllowOnlyActive == mPedalSupport && gActivePatchPedals == &mPedals)
			gActivePatchPedals = NULL;
	}
}
