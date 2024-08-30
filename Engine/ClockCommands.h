/*
 * mTroll MIDI Controller
 * Copyright (C) 2024 Sean Echevarria
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

#ifndef ClockCommands_h__
#define ClockCommands_h__

#include "IPatchCommand.h"
#include "MidiControlEngine.h"


class EnableMidiClockCommand : public IPatchCommand
{
public:
	EnableMidiClockCommand(MidiControlEngine * eng) :
		mEngine(eng)
	{
	}

	virtual void Exec() override
	{
		mEngine->EnableMidiClock(true);
	}

private:
	EnableMidiClockCommand();

private:
	MidiControlEngine	*mEngine;
};


class DisableMidiClockCommand : public IPatchCommand
{
public:
	DisableMidiClockCommand(MidiControlEngine * eng) :
		mEngine(eng)
	{
	}

	virtual void Exec() override
	{
		mEngine->EnableMidiClock(false);
	}

private:
	DisableMidiClockCommand();

private:
	MidiControlEngine	*mEngine;
};


class SetClockTempoCommand : public IPatchCommand
{
public:
	SetClockTempoCommand(MidiControlEngine * eng, int tempo) :
		mEngine(eng),
		mTempo(tempo)
	{
	}

	virtual void Exec() override
	{
		mEngine->SetTempo(mTempo);
	}

private:
	SetClockTempoCommand();

private:
	MidiControlEngine	*mEngine;
	const int			mTempo;
};

#endif // ClockCommands_h__
