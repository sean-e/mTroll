/*
 * mTroll MIDI Controller
 * Copyright (C) 2010,2013,2018 Sean Echevarria
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

#ifndef IMidiIn_h__
#define IMidiIn_h__

#include <string>

using byte = unsigned char;
class IMidiInSubscriber;


// IMidiIn
// ----------------------------------------------------------------------------
// use to receive MIDI
//
class IMidiIn
{
public:
	virtual ~IMidiIn() = default;

	virtual unsigned int GetMidiInDeviceCount() const = 0;
	virtual std::string GetMidiInDeviceName(unsigned int deviceIdx) const = 0;
	virtual bool OpenMidiIn(unsigned int deviceIdx) = 0;
	virtual bool IsMidiInOpen() const = 0;
	virtual bool Subscribe(IMidiInSubscriber* sub) = 0;
	virtual void Unsubscribe(IMidiInSubscriber* sub) = 0;
	virtual bool SuspendMidiIn() = 0;
	virtual bool ResumeMidiIn() = 0;
	virtual void CloseMidiIn() = 0;
};

#endif // IMidiIn_h__
