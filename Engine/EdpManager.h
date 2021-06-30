/*
 * mTroll MIDI Controller
 * Copyright (C) 2021 Sean Echevarria
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

#ifndef EdpManager_h__
#define EdpManager_h__

#include <memory>
#include "IMidiInSubscriber.h"
#include "HexStringUtils.h"

class IMainDisplay;
class ITraceDisplay;
class ISwitchDisplay;
class EdpManager;

using EdpManagerPtr = std::shared_ptr<EdpManager>;

// EdpManager
// ----------------------------------------------------------------------------
// Manages extended Echoplex Digital Pro support
//
class EdpManager :
	public IMidiInSubscriber
{
public:
	EdpManager(IMainDisplay * mainDisp, ISwitchDisplay * switchDisp, ITraceDisplay * pTrace);
	virtual ~EdpManager() = default;

	void SubscribeToMidiIn(IMidiInPtr midiIn);

	static Bytes GetGlobalStateRequest()
	{
		return { 0xF0, 0x00, 0x01, 0x30, 0x0B, 0x01, 0x01, 0x10, 0x00, 0x0b, 0x7f, 0xF7 };
	}

	static Bytes GetLocalStateRequest() 
	{
		return { 0xF0, 0x00, 0x01, 0x30, 0x0B, 0x01, 0x01, 0x12, 0x00, 0x13, 0x00, 0xF7 };
	}

	// IMidiInSubscriber
	void ReceivedData(byte b1, byte b2, byte b3) override;
	bool ReceivedSysex(const byte * bytes, int len) override;
	void Closed(IMidiInPtr midIn) override;

private:
	// basically an overload of IMidiInSubscriber::shared_from_this() but returning 
	// EdpManagerPtr instead of IMidiInSubscriberPtr
	EdpManagerPtr GetSharedThis()
	{
		return std::dynamic_pointer_cast<EdpManager>(IMidiInSubscriber::shared_from_this());
	}

	void ReceiveInfoData(const byte * bytes, int len);
	void ReceiveGlobalParamData(const byte * bytes, int len);
	void ReceiveLocalParamData(const byte * bytes, int len);

private:
	IMainDisplay	* mMainDisplay;
	ITraceDisplay	* mTrace;
	ISwitchDisplay	* mSwitchDisplay;
};

#endif // EdpManager_h__
