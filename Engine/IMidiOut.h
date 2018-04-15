/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2008,2013,2018 Sean Echevarria
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

#ifndef IMidiOut_h__
#define IMidiOut_h__

#include <string>
#include <vector>
#include <memory>

using byte = unsigned char;
using Bytes = std::vector<byte>;
class ISwitchDisplay;


// IMidiOut
// ----------------------------------------------------------------------------
// use to send MIDI
//
class IMidiOut : public std::enable_shared_from_this<IMidiOut>
{
public:
	virtual ~IMidiOut() = default;

	virtual unsigned int GetMidiOutDeviceCount() const = 0;
	virtual std::string GetMidiOutDeviceName(unsigned int deviceIdx) const = 0;
	virtual void SetActivityIndicator(ISwitchDisplay * activityIndicator, int activityIndicatorIdx) = 0;
	virtual void EnableActivityIndicator(bool enable) = 0;
	virtual bool OpenMidiOut(unsigned int deviceIdx) = 0;
	virtual bool IsMidiOutOpen() const = 0;
	virtual bool MidiOut(const Bytes & bytes) = 0;
	virtual void MidiOut(byte singleByte, bool useIndicator = true) = 0;
	virtual void MidiOut(byte byte1, byte byte2, bool useIndicator = true) = 0;
	virtual void MidiOut(byte byte1, byte byte2, byte byte3, bool useIndicator = true) = 0;
	virtual bool SuspendMidiOut() = 0;
	virtual bool ResumeMidiOut() = 0;
	virtual void CloseMidiOut() = 0;
};

using IMidiOutPtr = std::shared_ptr<IMidiOut>;

#endif // IMidiOut_h__
