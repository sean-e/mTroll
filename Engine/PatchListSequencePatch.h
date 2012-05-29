/*
 * mTroll MIDI Controller
 * Copyright (C) 2012 Sean Echevarria
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

#ifndef PatchListSequencePatch_h__
#define PatchListSequencePatch_h__

#include "IMidiOut.h"
#include "IPatchCommand.h"
#include <algorithm>
#include <strstream>
#include "DeletePtr.h"
#include "MidiControlEngine.h"


// PatchListSequencePatch
// -----------------------------------------------------------------------------
// responds to SwitchPressed and SwitchReleased
//
class PatchListSequencePatch : public Patch
{
public:
	PatchListSequencePatch(int number, const std::string & name, std::vector<int> & patchList) :
		Patch(number, name),
		mCurIndex(0),
		mCurrentSubPatch(NULL)
	{
		mPatchNumbers.swap(patchList);
	}

	~PatchListSequencePatch()
	{
	}
	
	virtual std::string GetPatchTypeStr() const { return "patchListSequence"; }

	virtual bool HasDisplayText() const { return true; }

	virtual const std::string & GetDisplayText() const 
	{ 
		if (!mCurrentSubPatch)
			return GetName();

		std::strstream msgstr;
		msgstr << mCurrentSubPatch->GetName() << " (" << mCurrentSubPatch->GetNumber() << ")" << std::endl << std::ends;
		static std::string sSubPatchDisplayText;
		sSubPatchDisplayText = msgstr.str();
		return sSubPatchDisplayText;
	}

	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
	{
		if (mCurrentSubPatch)
			mCurrentSubPatch->Deactivate(mainDisplay, switchDisplay);

		gActivePatchPedals = NULL;

		if (mCurIndex < mPatches.size())
		{
			mPatchIsActive = true;
			mCurrentSubPatch = mPatches[mCurIndex++];
			if (mCurrentSubPatch)
				mCurrentSubPatch->SwitchPressed(mainDisplay, switchDisplay);
		}
		else
		{
			mPatchIsActive = false;
			mCurIndex = 0;
			mCurrentSubPatch = NULL;
		}

		UpdateDisplays(mainDisplay, switchDisplay);
	}

	virtual void SwitchReleased(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) 
	{
		if (mCurrentSubPatch)
			mCurrentSubPatch->SwitchReleased(mainDisplay, switchDisplay);
	}

	virtual void OverridePedals(bool overridePedals) 
	{
	}

	virtual void BankTransitionActivation()
	{
		mCurIndex = 0;
		if (mCurrentSubPatch)
		{
			mCurrentSubPatch->Deactivate(NULL, NULL);
			mCurrentSubPatch = NULL;
		}
		SwitchPressed(NULL, NULL);
	}

	virtual void BankTransitionDeactivation()
	{
		mPatchIsActive = false;
		mCurIndex = 0;
		if (mCurrentSubPatch)
		{
			mCurrentSubPatch->Deactivate(NULL, NULL);
			mCurrentSubPatch = NULL;
		}
	}

	virtual void Deactivate(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
	{
		if (!IsActive())
			return;

		mCurIndex = mPatches.size() + 1;
		if (mCurrentSubPatch)
		{
			mCurrentSubPatch->Deactivate(NULL, NULL);
			mCurrentSubPatch = NULL;
		}
		SwitchPressed(mainDisplay, switchDisplay);
	}

	virtual void CompleteInit(MidiControlEngine * eng, ITraceDisplay * trc)
	{
		for (std::vector<int>::iterator it = mPatchNumbers.begin();
			it != mPatchNumbers.end();
			++it)
		{
			int curNum = *it;
			Patch * curPatch = eng->GetPatch(curNum);
			// leave (empty) slot in sequence even if no patch located
			mPatches.push_back(curPatch);
			
			if (!curPatch && trc)
			{
				std::strstream traceMsg;
				traceMsg << "Patch " << curNum << " referenced in PatchListSequence " << GetName() << " (" << GetNumber() << ") does not exist!" << std::endl << std::ends;
				trc->Trace(std::string(traceMsg.str()));
			}
		}
	}

private:
	size_t					mCurIndex;
	std::vector<int>		mPatchNumbers;
	std::vector<Patch*>		mPatches;
	Patch *					mCurrentSubPatch;
};

#endif // PatchListSequencePatch_h__
