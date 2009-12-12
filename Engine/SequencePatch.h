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

#ifndef SequencePatch_h__
#define SequencePatch_h__

#include "IMidiOut.h"
#include "IPatchCommand.h"
#include <algorithm>
#include "DeletePtr.h"


// SequencePatch
// -----------------------------------------------------------------------------
// responds to SwitchPressed; SwitchReleased does not affect patch state
// does not support expression pedals
//
class SequencePatch : public Patch
{
public:
	SequencePatch(int number, const std::string & name, IMidiOut * midiOut, PatchCommands & cmds) :
		Patch(number, name, midiOut),
		mCurIndex(0)
	{
		mCmds.swap(cmds);
	}

	~SequencePatch()
	{
		std::for_each(mCmds.begin(), mCmds.end(), DeletePtr<IPatchCommand>());
	}
	
	virtual std::string GetPatchTypeStr() const { return "sequence"; }

	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
	{
		if (mCurIndex < mCmds.size())
		{
			mPatchIsActive = true;
			IPatchCommand * curCmd = mCmds[mCurIndex++];
			if (curCmd)
				curCmd->Exec();

// should SequencePatches be able to use expr pedals?
// 			if (mMidiByteStrings.size() > 1)
// 				gActivePatchPedals = &mPedals;
		}

		if (mCurIndex >= mCmds.size())
		{
			mPatchIsActive = false;
			mCurIndex = 0;
			if (gActivePatchPedals == &mPedals)
				gActivePatchPedals = NULL;
		}

		UpdateDisplays(mainDisplay, switchDisplay);
	}

	virtual void BankTransitionActivation()
	{
		mCurIndex = 0;
		SwitchPressed(NULL, NULL);
	}

	virtual void BankTransitionDeactivation()
	{
		mPatchIsActive = false;
		mCurIndex = 0;
	}

	virtual void Activate(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
	{
		if (IsActive() && 0 == mCurIndex)
			return;

		mCurIndex = 0;
		SwitchPressed(mainDisplay, switchDisplay);
	}

	virtual void Deactivate(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
	{
		if (!IsActive() || 0 != mCurIndex)
			return;

		mCurIndex = mCmds.size() + 1;
		SwitchPressed(mainDisplay, switchDisplay);
	}

private:
	size_t					mCurIndex;
	PatchCommands			mCmds;
};

#endif // SequencePatch_h__
