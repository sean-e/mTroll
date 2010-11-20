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


AxeFxManager::AxeFxManager(ISwitchDisplay * switchDisp,
						   ITraceDisplay *pTrace) :
	mSwitchDisplay(switchDisp),
	mTrace(pTrace),
	mRefCnt(0),
	mTempoPatch(NULL)
{
}

AxeFxManager::~AxeFxManager()
{
	// TODO: cancel timer
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
AxeFxManager::ReceivedData(byte b1, byte b2, byte b3)
{
// 	if (mTrace)
// 	{
// 		const std::string msg(::GetAsciiHexStr((byte*)&dwParam1, 4, true) + "\n");
// 		mTrace->Trace(msg);
// 	}
}

// f0 00 01 74
bool
IsAxeFxSysex(const byte * bytes, const int & len)
{
	if (len < 6)
		return false;

	const byte kAxeFxId[] = { 0xf0, 0x00, 0x01, 0x74 };
	return !::memcmp(bytes, kAxeFxId, 4);
}

// f0 00 01 74 01 10 f7 
bool
IsTempoSysex(const byte * bytes, const int & len) 
{
	if (len != 7)
		return false;

	const byte kTempoSysex[] = { 0xf0, 0x00, 0x01, 0x74, 0x01, 0x10, 0xf7 };
	return !::memcmp(bytes, kTempoSysex, 7);
}

void
AxeFxManager::ReceivedSysex(byte * bytes, int len)
{
	if (::IsTempoSysex(bytes, len))
	{
		if (mTempoPatch)
		{
			mTempoPatch->ActivateSwitchDisplay(mSwitchDisplay, true);
			mSwitchDisplay->SetIndicatorThreadSafe(false, mTempoPatch, 75);
		}
		return;
	}

	if (!::IsAxeFxSysex(bytes, len))
		return;

	if (mTrace)
	{
		const std::string msg(::GetAsciiHexStr(bytes, len, true) + "\n");
		mTrace->Trace(msg);
	}
}

void
AxeFxManager::Closed(IMidiIn * midIn)
{
	midIn->Unsubscribe(this);
	Release();
}
