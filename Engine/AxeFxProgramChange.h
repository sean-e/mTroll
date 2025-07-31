/*
 * mTroll MIDI Controller
 * Copyright (C) 2010-2012,2018,2020,2025 Sean Echevarria
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

#ifndef AxeFxProgramChange_h__
#define AxeFxProgramChange_h__

#include "MidiCommandString.h"
#include "IAxeFx.h"


class AxeFxProgramChange : public MidiCommandString
{
public:
	AxeFxProgramChange(IMidiOutPtr midiOut, 
					  Bytes & midiString,
					  IAxeFxPtr mgr) :
		MidiCommandString(midiOut, midiString),
		mAxeMgr(mgr)
	{
	}

	~AxeFxProgramChange() = default;

	void Exec() override
	{
		MidiCommandString::Exec();
		if (mAxeMgr)
			mAxeMgr->DelayedNameSyncFromAxe();
	}

private:
	AxeFxProgramChange();

private:
	IAxeFxPtr		mAxeMgr = nullptr;
};

#endif // AxeFxProgramChange_h__
