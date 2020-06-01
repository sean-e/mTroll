/*
 * mTroll MIDI Controller
 * Copyright (C) 2020 Sean Echevarria
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

#ifndef AxeMomentaryPatch_h__
#define AxeMomentaryPatch_h__

#include "MomentaryPatch.h"
#include "IAxeFx.h"


 // MomentaryPatch
 // -----------------------------------------------------------------------------
 // responds to SwitchPressed and SwitchReleased
 // No expression pedal support
 //
class AxeMomentaryPatch : public MomentaryPatch
{
	IAxeFxPtr		mAx = nullptr;
	int				mIsScene;

public:
	AxeMomentaryPatch(int number,
			const std::string & name,
			IMidiOutPtr midiOut,
			PatchCommands & cmdsA,
			PatchCommands & cmdsB,
			IAxeFxPtr axeMgr,
			int isScenePatch) :
		MomentaryPatch(number, name, midiOut, cmdsA, cmdsB),
		mAx(axeMgr),
		mIsScene(isScenePatch)
	{
	}

	virtual std::string GetPatchTypeStr() const override { return "axeMomentary"; }

	virtual void ExecCommandsA() override
	{
		__super::ExecCommandsA();

		if (!mCmdsA.empty())
			UpdateAxeMgr();
	}

	virtual void ExecCommandsB() override
	{
		__super::ExecCommandsB();

		if (!mCmdsB.empty())
			UpdateAxeMgr();
	}

	void ClearAxeMgr()
	{
		if (mAx)
			mAx = nullptr;
	}

private:
	void UpdateAxeMgr()
	{
		if (!mAx)
			return;

		if (mIsScene)
			mAx->UpdateSceneStatus(mIsScene - 1, true);
		else
		{
			// Due to getting a response from the Axe-Fx II before state of
			// externals was accurate (Feedback Return mute mapped to Extern
			// 8 came back inaccurate when SyncEffectsFromAxe called immediately).
			mAx->DelayedEffectsSyncFromAxe();
		}
	}
};

#endif // AxeMomentaryPatch_h__
