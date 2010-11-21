/*
 * mTroll MIDI Controller
 * Copyright (C) 2010 Sean Echevarria
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

#include <string>
#include <memory.h>
#include "AxeFxManager.h"
#include "ITraceDisplay.h"
#include "HexStringUtils.h"
#include "IMidiIn.h"
#include "Patch.h"
#include "ISwitchDisplay.h"
#include "AxemlLoader.h"


AxeFxManager::AxeFxManager(ISwitchDisplay * switchDisp,
						   ITraceDisplay *pTrace) :
	mSwitchDisplay(switchDisp),
	mTrace(pTrace),
	mRefCnt(0),
	mTempoPatch(NULL)
{
	AxemlLoader ldr(mTrace);
	ldr.Load("debug/default.axeml");	
}

AxeFxManager::~AxeFxManager()
{
	// TODO: cancel timers?
}

void
AxeFxManager::AddRef()
{
	// this should only happen on the UI thread, otherwise mRefCnt needs 
	// to be thread protected
	++mRefCnt;
}

void
AxeFxManager::Release()
{
	// this should only happen on the UI thread, otherwise mRefCnt needs 
	// to be thread protected
	if (!--mRefCnt)
		delete this;
}

void
AxeFxManager::Closed(IMidiIn * midIn)
{
	midIn->Unsubscribe(this);
	Release();
}

void
AxeFxManager::ReceivedData(byte b1, byte b2, byte b3)
{
// 	if (mTrace)
// 	{
// 		const std::string msg(::GetAsciiHexStr((byte*)&dwParam1, 4, true) + "\n");
// 		mTrace->Trace(msg);
// 	}
}

// f0 00 01 74 01 ...
bool
IsAxeFxSysex(const byte * bytes, const int len)
{
	if (len < 7)
		return false;

	const byte kAxeFxId[] = { 0xf0, 0x00, 0x01, 0x74, 0x01 };
	return !::memcmp(bytes, kAxeFxId, 5);
}

void
AxeFxManager::ReceivedSysex(const byte * bytes, int len)
{
	// http://axefxwiki.guitarlogic.org/index.php?title=Axe-Fx_SysEx_Documentation
	if (!::IsAxeFxSysex(bytes, len))
		return;

	// Tempo: f0 00 01 74 01 10 f7 
	if (0x10 == bytes[5])
	{
		if (mTempoPatch)
		{
			mTempoPatch->ActivateSwitchDisplay(mSwitchDisplay, true);
			mSwitchDisplay->SetIndicatorThreadSafe(false, mTempoPatch, 75);
		}
		return;
	}

	// MIDI_PARAM_VALUE: f0 00 01 74 01 02 ...
	if (2 == bytes[5])
	{
		ReceiveParamValue(&bytes[6], len - 6);
		return;
	}
}

void
AxeFxManager::ReceiveParamValue(const byte * bytes, int len)
{
/*
	0xdd effect ID LS nibble 
	0xdd effect ID MS nibble 
	0xdd parameter ID LS nibble 
	0xdd parameter ID MS nibble 
	0xdd parameter value LS nibble 
	0xdd parameter value MS nibble 
	0xdd null-terminated string byte0 
	0xdd byte1 
	... 
	0x00 null character 
	0xF7 sysex end 
 */

	if (len < 8)
	{
		if (mTrace)
		{
			const std::string msg("truncated param value msg\n");
			mTrace->Trace(msg);
		}
		return;
	}

	if (mTrace)
	{
		const std::string msg(::GetAsciiHexStr(bytes, len, true) + "\n");
		mTrace->Trace(msg);
	}
}

/*
 *	MIDI_SET_PARAMETER (or GET)
    0xF0 sysex start 
    0x00 Manf. ID byte0 
    0x01 Manf. ID byte1 
    0x74 Manf. ID byte2
    0x01 Model / device
    0x02 Function ID 
    0xdd effect ID LS nibble 
    0xdd effect ID MS nibble 
    0xdd parameter ID LS nibble 
    0xdd parameter ID MS nibble 
    0xdd parameter value LS nibble 
    0xdd parameter value MS nibble 
    0xdd query(0) or set(1) value 
    0xF7 sysex end 
*/
