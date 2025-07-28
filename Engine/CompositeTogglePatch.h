/*
 * mTroll MIDI Controller
 * Copyright (C) 2021 Sean Echevarria
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

#ifndef CompositeTogglePatch_h__
#define CompositeTogglePatch_h__

#include "TogglePatch.h"
#include <sstream>


 // CompositeTogglePatch
 // -----------------------------------------------------------------------------
 // responds to SwitchPressed; SwitchReleased does not affect patch state
 // supports expression pedals (psAllowOnlyActive) - but should it?
 //
class CompositeTogglePatch : public TogglePatch
{
public:
	CompositeTogglePatch(int number,
		const std::string & name,
		IMidiOutPtr midiOut,
		std::vector<std::pair<int, int>> groupA,
		std::vector<std::pair<int, int>> groupB) :
		TogglePatch(number, name, midiOut, mCmdsA, mCmdsB),
		mPatchNumbersA(groupA),
		mPatchNumbersB(groupB)
	{
	}

	virtual std::string GetPatchTypeStr() const override { return "compositeToggle"; }

	virtual void CompleteInit(MidiControlEngine * eng, ITraceDisplay * trc) override
	{
		TogglePatch::CompleteInit(eng, trc);

		for (auto& curPair : mPatchNumbersA)
		{
			int curPatchNum = curPair.first;
			PatchPtr curPatch = eng->GetPatch(curPatchNum);
			if (curPatch)
			{
				auto isTwo = dynamic_cast<TwoStatePatch*>(curPatch.get());
				if (isTwo)
					mPatchesA.emplace_back(curPatch, curPair.second);
				else if (trc)
				{
					std::ostringstream traceMsg;
					traceMsg << "Patch " << curPatchNum << " referenced in CompositeTogglePatch " << GetName() << " (" << GetNumber() << ") is not a two-state patch type!\n";
					trc->Trace(traceMsg.str());
				}
			}
			else if (trc)
			{
				std::ostringstream traceMsg;
				traceMsg << "Patch " << curPatchNum << " referenced in CompositeTogglePatch " << GetName() << " (" << GetNumber() << ") does not exist!\n";
				trc->Trace(traceMsg.str());
			}
		}

		for (auto& curPair : mPatchNumbersB)
		{
			int curPatchNum = curPair.first;
			PatchPtr curPatch = eng->GetPatch(curPatchNum);
			if (curPatch)
			{
				auto isTwo = dynamic_cast<TwoStatePatch*>(curPatch.get());
				if (isTwo)
					mPatchesB.emplace_back(curPatch, curPair.second);
				else if (trc)
				{
					std::ostringstream traceMsg;
					traceMsg << "Patch " << curPatchNum << " referenced in CompositeTogglePatch " << GetName() << " (" << GetNumber() << ") is not a two-state patch type!\n";
					trc->Trace(traceMsg.str());
				}
			}
			else if (trc)
			{
				std::ostringstream traceMsg;
				traceMsg << "Patch " << curPatchNum << " referenced in CompositeTogglePatch " << GetName() << " (" << GetNumber() << ") does not exist!\n";
				trc->Trace(traceMsg.str());
			}
		}
	}

	virtual void ExecCommandsA() override
	{
		for (const auto& curPair : mPatchesA)
		{
			auto curTwo = dynamic_cast<TwoStatePatch*>(curPair.first.get());
			_ASSERTE(curTwo);
			if (curPair.second)
				curTwo->ExecCommandsB();
			else
				curTwo->ExecCommandsA();
		}

		TogglePatch::ExecCommandsA();
	}

	virtual void ExecCommandsB() override
	{
		TogglePatch::ExecCommandsB();

		for (const auto& curPair : mPatchesB)
		{
			auto curTwo = dynamic_cast<TwoStatePatch*>(curPair.first.get());
			_ASSERTE(curTwo);
			if (curPair.second)
				curTwo->ExecCommandsB();
			else
				curTwo->ExecCommandsA();
		}
	}

	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) override
	{
		const bool wasActive = IsActive();
		TogglePatch::SwitchPressed(mainDisplay, switchDisplay);

		for (const auto& curPair : wasActive ? mPatchesB : mPatchesA)
			curPair.first->UpdateDisplays(mainDisplay, switchDisplay);
	}

private:
	PatchCommands mCmdsA, mCmdsB;
	std::vector<std::pair<int, int>>		mPatchNumbersA, mPatchNumbersB;
	std::vector<std::pair<PatchPtr, int>>	mPatchesA, mPatchesB;
};

#endif // CompositeTogglePatch_h__
