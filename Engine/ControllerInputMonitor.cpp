/*
 * mTroll MIDI Controller
 * Copyright (C) 2022,2025 Sean Echevarria
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

#include <format>
#include "ControllerInputMonitor.h"
#include "ITraceDisplay.h"
#include "HexStringUtils.h"
#include "IMidiIn.h"


ControllerInputMonitor::ControllerInputMonitor(ISwitchDisplay * switchDisp, ITraceDisplay * pTrace) :
	mSwitchDisplay(switchDisp),
	mTrace(pTrace)
{
}

void
ControllerInputMonitor::SubscribeToMidiIn(IMidiInPtr midiIn)
{
	midiIn->Subscribe(shared_from_this());
}

void
ControllerInputMonitor::AddPatch(ControllerTogglePatchPtr p, int channel, int controller)
{
	auto const key = ListenerMapKey(channel, controller);
	if (mPatches.find(key) != mPatches.end())
	{
		if (mTrace)
			mTrace->Trace("Error setting up input device monitor: duplicate port+channel in DeviceChannelMap?\n");
		return;
	}

	mPatches[key] = p;
}

void
ControllerInputMonitor::ReceivedData(byte b1, byte b2, byte b3)
{
	// is b1 a control change?
	if ((b1 & 0xF0) != 0xb0)
		return;

	const ListenerMapKey k(b1 & 0x0F, b2);
	ListenerMap::iterator it = mPatches.find(k);
	if (it == mPatches.end())
		return;

	it->second->UpdateState(mSwitchDisplay, b3 > 0);
}

bool
ControllerInputMonitor::ReceivedSysex(const byte * bytes, int len)
{
	return false;
}

void
ControllerInputMonitor::Closed(IMidiInPtr midIn)
{
	midIn->Unsubscribe(shared_from_this());
}
