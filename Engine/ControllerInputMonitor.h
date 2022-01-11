/*
 * mTroll MIDI Controller
 * Copyright (C) 2022 Sean Echevarria
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

#ifndef ControllerInputMonitor_h__
#define ControllerInputMonitor_h__

#include <memory>
#include <map>
#include "IMidiInSubscriber.h"
#include "ControllerTogglePatch.h"

class ITraceDisplay;
class ISwitchDisplay;
class ControllerInputMonitor;

using ControllerInputMonitorPtr = std::shared_ptr<ControllerInputMonitor>;

// ControllerInputMonitor
// ----------------------------------------------------------------------------
// updates patch state upon reception of control change input
//
class ControllerInputMonitor :
	public IMidiInSubscriber
{
public:
	ControllerInputMonitor(ISwitchDisplay * switchDisp, ITraceDisplay * pTrace);
	virtual ~ControllerInputMonitor() = default;

	void SubscribeToMidiIn(IMidiInPtr midiIn);
	void AddPatch(ControllerTogglePatchPtr p, int channel, int controller);

	// IMidiInSubscriber
	void ReceivedData(byte b1, byte b2, byte b3) override;
	bool ReceivedSysex(const byte * bytes, int len) override;
	void Closed(IMidiInPtr midIn) override;

private:
	ITraceDisplay	* mTrace;
	ISwitchDisplay	* mSwitchDisplay;
	using ListenerMapKey = std::pair<int, int>;
	using ListenerMap = std::map<ListenerMapKey, ControllerTogglePatchPtr>;
	ListenerMap mPatches;
};

#endif // ControllerInputMonitor_h__
