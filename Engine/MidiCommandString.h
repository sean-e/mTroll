/*
 * mTroll MIDI Controller
 * Copyright (C) 2009,2018 Sean Echevarria
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

#ifndef MidiCommandString_h__
#define MidiCommandString_h__

#include "IPatchCommand.h"
#include "IMidiOut.h"


class MidiCommandString : public IPatchCommand
{
public:
	MidiCommandString(IMidiOut * midiOut, 
					  Bytes & midiString) :
		mMidiOut(midiOut)
	{
		mCommandString.swap(midiString);
	}

	virtual void Exec() override
	{
		if (!mMidiOut)
			return;

		switch (mCommandString.size())
		{
		case 3:
			mMidiOut->MidiOut(mCommandString[0], mCommandString[1], mCommandString[2]);
			break;
		case 2:
			mMidiOut->MidiOut(mCommandString[0], mCommandString[1]);
			break;
		case 0:
			break;
		default:
			mMidiOut->MidiOut(mCommandString);
		}
	}

private:
	MidiCommandString();

private:
	IMidiOut	*mMidiOut;
	Bytes	mCommandString;
};

#endif // MidiCommandString_h__
