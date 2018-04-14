/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2010,2013,2015,2018 Sean Echevarria
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
#include <algorithm>
#include <xfunctional>
#include "DeletePtr.h"


// TwoStatePatch
// -----------------------------------------------------------------------------
// Base class for classic pmc10 patch types.
//
class TwoStatePatch : public Patch
{
protected:
	enum PedalSupport {psDisallow, psAllow, psAllowOnlyActive};

	TwoStatePatch(int number, 
				  const std::string & name, 
				  IMidiOut * midiOut, 
				  PatchCommands & cmdsA, 
				  PatchCommands & cmdsB, 
				  PedalSupport pedalSupport) :
		Patch(number, name, midiOut),
		mPedalSupport(pedalSupport)
	{
		mCmdsA.swap(cmdsA);
		mCmdsB.swap(cmdsB);
	}

	~TwoStatePatch()
	{
		std::for_each(mCmdsA.begin(), mCmdsA.end(), DeletePtr<IPatchCommand>());
		std::for_each(mCmdsB.begin(), mCmdsB.end(), DeletePtr<IPatchCommand>());
	}

	virtual void BankTransitionActivation() override { ExecCommandsA(); }
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
};

#endif // TwoStatePatch_h__
