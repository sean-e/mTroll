/*
 * mTroll MIDI Controller
 * Copyright (C) 2010 Sean Echevarria
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

#ifndef AxeTogglePatch_h__
#define AxeTogglePatch_h__

#include "TogglePatch.h"
#include "AxeFxManager.h"


// AxeTogglePatch
// -----------------------------------------------------------------------------
// responds to SwitchPressed; SwitchReleased does not affect patch state
// supports expression pedals (psAllowOnlyActive) - but should it?
//
class AxeTogglePatch : public TogglePatch
{
	AxeFxManager * mAx;
public:
	AxeTogglePatch(int number, 
				const std::string & name, 
				IMidiOut * midiOut, 
				PatchCommands & cmdsA, 
				PatchCommands & cmdsB,
				AxeFxManager * axeMgr) :
		TogglePatch(number, name, midiOut, cmdsA, cmdsB),
		mAx(axeMgr)
	{
		if (mAx)
			mAx->AddRef();
	}

	virtual ~AxeTogglePatch()
	{
		if (mAx)
			mAx->Release();
	}

	virtual std::string GetPatchTypeStr() const { return "axeToggle"; }

	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
	{
		TogglePatch::SwitchPressed(mainDisplay, switchDisplay);
		if (mAx)
			mAx->SyncFromAxe(this);
	}

	void ClearAxeMgr() 
	{
		if (mAx)
		{
			mAx->Release();
			mAx = NULL;
		}
	}

};

#endif // AxeTogglePatch_h__
