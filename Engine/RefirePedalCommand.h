/*
 * mTroll MIDI Controller
 * Copyright (C) 2009 Sean Echevarria
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

#ifndef RefirePedalCommand_h__
#define RefirePedalCommand_h__

#include "IPatchCommand.h"
#include "IMidiOut.h"

class MidiControlEngine;


class RefirePedalCommand : public IPatchCommand
{
public:
	RefirePedalCommand(MidiControlEngine * eng, 
					   int pedalNumber) :
		mEngine(eng),
		mPedalNumber(pedalNumber)
	{
	}

	void Exec()
	{
		mEngine->RefirePedal(mPedalNumber);
	}

private:
	RefirePedalCommand();

private:
	MidiControlEngine	*mEngine;
	int			mPedalNumber;
};

#endif // RefirePedalCommand_h__
