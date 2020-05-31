/*
 * mTroll MIDI Controller
 * Copyright (C) 2012-2013,2015,2017-2018,2020 Sean Echevarria
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
#include <memory>
#include "MidiControlEngine.h"
#include "PersistentPedalOverridePatch.h"


using PatchPtr = std::shared_ptr<Patch>;

// PatchListSequencePatch
// -----------------------------------------------------------------------------
// responds to SwitchPressed and SwitchReleased
//
class PatchListSequencePatch : public Patch
{
public:
	PatchListSequencePatch(int number, const std::string & name, std::vector<int> & patchList, bool immediateWraparound) :
		Patch(number, name),
		mCurIndex(0),
		mCurrentSubPatch(nullptr),
		mImmediateWraparound(immediateWraparound)
	{
		mPatchNumbers.swap(patchList);
	}

	~PatchListSequencePatch()
	{
	}
	
	virtual std::string GetPatchTypeStr() const override { return "patchListSequence"; }

	virtual bool HasDisplayText() const override { return true; }

	virtual bool IsPatchVolatile() const override
	{ 
		return true;
	}

	virtual void DeactivateVolatilePatch() override
	{
		if (mCurrentSubPatch)
			mCurrentSubPatch->DeactivateVolatilePatch();

		if (!PersistentPedalOverridePatch::PedalOverridePatchIsActive())
			gActivePatchPedals = nullptr;
	}

	virtual const std::string & GetDisplayText(bool /*checkState = false*/) const override
	{ 
		if (!mCurrentSubPatch)
			return GetName();

		std::strstream msgstr;
		msgstr << mCurrentSubPatch->GetName();
		const int patchNum = mCurrentSubPatch->GetNumber();
		if (patchNum > 0)
			msgstr << " (" << patchNum << ")";
		msgstr << std::ends;
		static std::string sSubPatchDisplayText;
		sSubPatchDisplayText = msgstr.str();
		return sSubPatchDisplayText;
	}

	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) override
	{
		if (mCurrentSubPatch)
			mCurrentSubPatch->Deactivate(mainDisplay, switchDisplay);

		if (!PersistentPedalOverridePatch::PedalOverridePatchIsActive())
			gActivePatchPedals = nullptr;

		if (mImmediateWraparound && mCurIndex == mPatches.size())
		{
			// gapless restart resets to index 0 in non-active state,
			// though the subPatch is active
			mCurIndex = 0;
			mPatchIsActive = false;
		}

		if (mCurIndex < mPatches.size())
		{
			if (mImmediateWraparound)
			{
				if (!mCurIndex && !mCurrentSubPatch)
				{
					// in gapless restart, at start, if the first step
					// is already active, then proceed directly to the next
					auto p = mPatches[mCurIndex];
					if (p && p->IsActive())
						++mCurIndex;
				}

				if (mCurIndex)
				{
					// gaplessRestart is not active at index 0, though
					// the subPatch is active
					mPatchIsActive = true;
				}
			}
			else
			{
				mPatchIsActive = true;
			}

			mCurrentSubPatch = mPatches[mCurIndex++];
			if (mCurrentSubPatch)
				mCurrentSubPatch->SwitchPressed(mainDisplay, switchDisplay);
		}
		else
		{
			mPatchIsActive = false;
			mCurIndex = 0;
			DeactivateVolatilePatch();
			mCurrentSubPatch = nullptr;
		}

		UpdateDisplays(mainDisplay, switchDisplay);
	}

	virtual void SwitchReleased(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) override
	{
		if (mCurrentSubPatch)
			mCurrentSubPatch->SwitchReleased(mainDisplay, switchDisplay);
	}

	virtual void OverridePedals(bool overridePedals) override
	{
	}

	virtual void BankTransitionActivation() override
	{
		mCurIndex = 0;
		if (mCurrentSubPatch)
		{
			DeactivateVolatilePatch();
			mCurrentSubPatch->Deactivate(nullptr, nullptr);
			mCurrentSubPatch = nullptr;
		}
		SwitchPressed(nullptr, nullptr);
	}

	virtual void BankTransitionDeactivation() override
	{
		mPatchIsActive = false;
		mCurIndex = 0;
		if (mCurrentSubPatch)
		{
			DeactivateVolatilePatch();
			mCurrentSubPatch->Deactivate(nullptr, nullptr);
			mCurrentSubPatch = nullptr;
		}
	}

	virtual void Deactivate(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) override
	{
		if (!IsActive())
			return;

		mCurIndex = mPatches.size() + 1;
		if (mCurrentSubPatch)
		{
			DeactivateVolatilePatch();
			mCurrentSubPatch->Deactivate(nullptr, nullptr);
			mCurrentSubPatch = nullptr;
		}
		SwitchPressed(mainDisplay, switchDisplay);
	}

	virtual void CompleteInit(MidiControlEngine * eng, ITraceDisplay * trc) override
	{
		for (int curNum : mPatchNumbers)
		{
			PatchPtr curPatch = eng->GetPatch(curNum);
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

	virtual void UpdateState(ISwitchDisplay * switchDisplay, bool active) override
	{
		_ASSERTE(!active); // only support going to disabled state
		if (mPatchIsActive)
		{
			if (!PersistentPedalOverridePatch::PedalOverridePatchIsActive())
				gActivePatchPedals = nullptr;

			mCurIndex = 0;
			DeactivateVolatilePatch();
			mCurrentSubPatch = nullptr;
		}

		Patch::UpdateState(switchDisplay, active);
	}

private:
	size_t					mCurIndex;
	std::vector<int>		mPatchNumbers;
	std::vector<PatchPtr>	mPatches;
	PatchPtr				mCurrentSubPatch;
	bool					mImmediateWraparound;
};

#endif // PatchListSequencePatch_h__
