/*
 * mTroll MIDI Controller
 * Copyright (C) 2009-2010,2018,2024-2025 Sean Echevarria
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

#ifndef BaseSleepCommand_h__
#define BaseSleepCommand_h__

#include "IPatchCommand.h"
#include "IMidiOut.h"
#include "CrossPlatform.h"
#ifdef _WINDOWS
#undef TextOut		// stupid unicode support defines TextOut to TextOutW
#endif // _WINDOWS



class BaseSleepCommand : public IPatchCommand
{
public:
	BaseSleepCommand() = default;

	virtual void Exec() override
	{
		xp::Sleep(GetSleepAmount()); // amount in milliseconds
	}

protected:
	virtual int GetSleepAmount() = 0;
};

#endif // BaseSleepCommand_h__
