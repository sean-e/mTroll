/*
 * mTroll MIDI Controller
 * Copyright (C) 2010,2014,2018 Sean Echevarria
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

#ifndef IMidiInGenerator_h__
#define IMidiInGenerator_h__


class IMidiIn;

using IMidiInPtr = std::shared_ptr<IMidiIn>;


// IMidiInGenerator
// ----------------------------------------------------------------------------
// use to create IMidiIn
//
class IMidiInGenerator
{
public:
	virtual IMidiInPtr	CreateMidiIn(unsigned int deviceIdx) = 0;
	virtual IMidiInPtr	GetMidiIn(unsigned int deviceIdx) = 0;
	virtual unsigned int GetMidiInDeviceIndex(const std::string &deviceName) = 0;
	virtual void		OpenMidiIns() = 0;
	virtual void		CloseMidiIns() = 0;
};

#endif // IMidiInGenerator_h__
