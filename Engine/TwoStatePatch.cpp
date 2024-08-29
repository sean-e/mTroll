/*
* mTroll MIDI Controller
* Copyright (C) 2015-2016,2018,2024 Sean Echevarria
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
#include "MidiControlEngine.h"


void
TwoStatePatch::CompleteInit(MidiControlEngine * eng, ITraceDisplay * trc)
{
	if (!mGroupId.empty())
	{
		_ASSERTE(eng);
		mEng = eng;
		mEng->AddToPatchGroup(mGroupId, this);
	}
}

void
TwoStatePatch::SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	if (mEng && !mGroupId.empty())
		mEng->DeactivateRestOfPatchGroup(mGroupId, this, mainDisplay, switchDisplay);
}

void
TwoStatePatch::BankTransitionActivation()
{
	if (mEng && !mGroupId.empty())
		mEng->DeactivateRestOfPatchGroup(mGroupId, this, nullptr, nullptr);

	ExecCommandsA();
}

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

	for (const auto& cmd : mCmdsA)
		cmd->Exec();
}

void
TwoStatePatch::ExecCommandsB()
{
	for (const auto &cmd : mCmdsB)
		cmd->Exec();

	mPatchIsActive = false;

	if (!mOverridePedals)
	{
		if (psAllowOnlyActive == mPedalSupport && gActivePatchPedals == &mPedals)
			gActivePatchPedals = nullptr;
	}
}
