/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2010,2013,2015,2018,2021,2024-2025 Sean Echevarria
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

#ifndef TwoStatePatch_h__
#define TwoStatePatch_h__

#include "IMidiOut.h"
#include "Patch.h"
#include "IPatchCommand.h"


class MidiControlEngine;


// TwoStatePatch
// -----------------------------------------------------------------------------
// Base class for classic pmc10 patch types.
//
// If groupId is set, then all patches of same groupId coordinate so that only 
// one of that group is ever active.
//
class TwoStatePatch : public Patch
{
protected:
	enum PedalSupport {psDisallow, psAllow, psAllowOnlyActive};

	TwoStatePatch(int number, 
				  const std::string & name, 
				  IMidiOutPtr midiOut, 
				  PatchCommands & cmdsA, 
				  PatchCommands & cmdsB, 
				  PedalSupport pedalSupport) :
		Patch(number, name, midiOut),
		mPedalSupport(pedalSupport)
	{
		mCmdsA.swap(cmdsA);
		mCmdsB.swap(cmdsB);
	}

	TwoStatePatch(int number,
		const std::string & name,
		IMidiOutPtr midiOut,
		PatchCommands * cmdsA,
		PatchCommands * cmdsB,
		PedalSupport pedalSupport) :
		Patch(number, name, midiOut),
		mPedalSupport(pedalSupport)
	{
		if (cmdsA)
			mCmdsA.swap(*cmdsA);
		if (cmdsB)
			mCmdsB.swap(*cmdsB);
	}

	~TwoStatePatch() = default;

public:
	virtual bool SetGroupId(const std::string &groupId) { mGroupId = groupId; return true; }
	virtual void DeactivateGroupedPatch() { ExecCommandsB(); }

	virtual void CompleteInit(MidiControlEngine * eng, ITraceDisplay * trc) override;
	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) override;

	virtual void BankTransitionActivation() override;
	virtual void BankTransitionDeactivation() override { ExecCommandsB(); }

	virtual void ExecCommandsA();
	virtual void ExecCommandsB();

private:
	TwoStatePatch();
	TwoStatePatch(const TwoStatePatch &);

protected:
	PatchCommands	mCmdsA;
	PatchCommands	mCmdsB;
	const PedalSupport	mPedalSupport;
	MidiControlEngine	*mEng = nullptr;
	std::string		mGroupId;
};

#endif // TwoStatePatch_h__
