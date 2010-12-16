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

#ifndef AxeFxProgramChange_h__
#define AxeFxProgramChange_h__

#include "MidiCommandString.h"
#include "AxeFxManager.h"
#ifdef _WINDOWS
#include <windows.h>
	#define SLEEP	Sleep
	#undef TextOut		// stupid unicode support defines TextOut to TextOutW
#else
	#define SLEEP	sleep
#endif // _WINDOWS


class AxeFxProgramChange : public MidiCommandString
{
public:
	AxeFxProgramChange(IMidiOut * midiOut, 
					  Bytes & midiString,
					  AxeFxManager * mgr) :
		MidiCommandString(midiOut, midiString),
		mAxeMgr(mgr)
	{
		if (mAxeMgr)
			mAxeMgr->AddRef();
	}

	~AxeFxProgramChange()
	{
		if (mAxeMgr)
			mAxeMgr->Release();
	}

	void Exec()
	{
		__super::Exec();
		SLEEP(50); // too long?

		if (mAxeMgr)
			mAxeMgr->SyncFromAxe();
	}

private:
	AxeFxProgramChange();

private:
	AxeFxManager * mAxeMgr;
};

#endif // AxeFxProgramChange_h__
