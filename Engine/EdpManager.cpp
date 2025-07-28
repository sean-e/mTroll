/*
 * mTroll MIDI Controller
 * Copyright (C) 2021-2023 Sean Echevarria
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
#include <sstream>
#include <algorithm>
#include "EdpManager.h"
#include "ITraceDisplay.h"
#include "HexStringUtils.h"
#include "IMidiIn.h"
#include "ISwitchDisplay.h"
#include "IMainDisplay.h"
#include "EdpIds.h"


#ifdef _DEBUG
constexpr int kDbgFlag = 1;
#else
constexpr int kDbgFlag = 0;
#endif

EdpManager::EdpManager(IMainDisplay * mainDisp, ISwitchDisplay * switchDisp, ITraceDisplay * pTrace) :
	mSwitchDisplay(switchDisp),
	mMainDisplay(mainDisp),
	mTrace(pTrace)
{
}

void
EdpManager::SubscribeToMidiIn(IMidiInPtr midiIn, int deviceIdx)
{
	midiIn->Subscribe(shared_from_this());

	const std::string name(midiIn->GetMidiInDeviceName(deviceIdx));
	if (-1 != name.find("U2MIDI"))
		mHackForCmeInterface = true;
}

void
EdpManager::ReceivedData(byte b1, byte b2, byte b3)
{
// 	if (mTrace)
// 	{
// 		const std::string msg(::GetAsciiHexStr((byte*)&dwParam1, 4, true) + "\n");
// 		mTrace->Trace(msg);
// 	}
}

bool
IsEdpSysex(const byte * bytes, const int len)
{
	constexpr byte kEdpSysexHeader[] = { 0xF0, 0x00, 0x01, 0x30, 0x0B };
	if (len < sizeof(kEdpSysexHeader))
		return false;

	if (::memcmp(bytes, kEdpSysexHeader, sizeof(kEdpSysexHeader)))
		return false;

	return true;
}

bool
EdpManager::ReceivedSysex(const byte * bytesIn, int len)
{
	if (!::IsEdpSysex(bytesIn, len))
		return false;

	const byte* bytes = bytesIn;
	std::vector<byte> backingStore;
	if (mHackForCmeInterface)
	{
		// EDP sends a SYSTEM RESET (0xFF) message in response to local data request.
		// EDP sends 2 SYSTEM RESET (0xFF) messages in response to global data request.
		// CME U2MIDI Pro interface munges those SYSTEM RESET messages into the SYSEX bytes.
		backingStore.reserve(len);
		for (int idx = 0; idx < len; ++idx)
			if (bytesIn[idx] != 0xFF)
				backingStore.push_back(bytesIn[idx]);

		len = backingStore.size();
		bytes = &backingStore[0];
	}

	if (len < 12)
		return true;

	constexpr int kCmdPos = 6;
	EdpSysexCommands cmd = static_cast<EdpSysexCommands>(bytes[kCmdPos]);
	switch (cmd)
	{
	case EdpSysexCommands::InfoData:
		// could be request if we are not actually reading the command byte (see:#cmdPosIsDifferent)
		break;
	case EdpSysexCommands::GlobalParamData:
		ReceiveGlobalParamData(&bytes[kCmdPos + 1], len - (kCmdPos + 1));
		break;
	case EdpSysexCommands::LocalParamData:
		ReceiveLocalParamData(&bytes[kCmdPos + 1], len - (kCmdPos + 1));
		break;
	default:
		// #cmdPosIsDifferent cmdPos is different for request than response
		switch (static_cast<EdpSysexCommands>(bytes[kCmdPos + 1]))
		{
		case EdpSysexCommands::InfoRequest:
		case EdpSysexCommands::GlobalParamRequest:
		case EdpSysexCommands::LocalParamRequest:
		case EdpSysexCommands::AllParamRequest:
		case EdpSysexCommands::GlobalParamReset:
		case EdpSysexCommands::LocalParamReset:
			break;
		default:
			if (kDbgFlag && mTrace)
			{
				const std::string byteDump(::GetAsciiHexStr(&bytes[kCmdPos], len - kCmdPos, true));
				std::ostringstream traceMsg;
				traceMsg << "EDP unexpected sysex: " << byteDump.c_str() << '\n';
				mTrace->Trace(traceMsg.str());
			}
		}
	}

	return true;
}

void
EdpManager::Closed(IMidiInPtr midIn)
{
	midIn->Unsubscribe(shared_from_this());
}

// req: F0 00 01 30 0B 01 01 10 00 0b 7f F7
// rec: F0 00 01 30 0B 01 11 00 0b 7F 01 01 06 02 24 01 02 54 00 01 F7
// bytes: 00 0b 7F 01 01 06 02 24 01 02 54 00 01 F7 (len 14)
void
EdpManager::ReceiveGlobalParamData(const byte * bytes, int len)
{
	if (14 != len)
	{
		if (kDbgFlag && mTrace)
		{
			const std::string byteDump(::GetAsciiHexStr(bytes, len, true));
			std::ostringstream traceMsg;
			traceMsg << "EDP unexpected global param data len: " << byteDump.c_str() << '\n';
			mTrace->Trace(traceMsg.str());
		}
		return;
	}

	if (bytes[0] != 0 || bytes[1] != 0xb || bytes[2] != 0x7f)
	{
		if (kDbgFlag && mTrace)
		{
			const std::string byteDump(::GetAsciiHexStr(bytes, len, true));
			std::ostringstream traceMsg;
			traceMsg << "EDP unexpected global param data: " << byteDump.c_str() << '\n';
			mTrace->Trace(traceMsg.str());
		}
		return;
	}

	constexpr int kByteOffset = 3;
	std::ostringstream dispMsg;
	dispMsg << "EDP Global State";
	dispMsg << "\nPreset: " << (int)bytes[(int)EdpGlobalParamIndexes::ParamSet + kByteOffset];
	if (bytes[(int)EdpGlobalParamIndexes::PrevParamSet + kByteOffset] != bytes[(int)EdpGlobalParamIndexes::ParamSet + kByteOffset])
		dispMsg << "\nPrev preset: " << (int)bytes[(int)EdpGlobalParamIndexes::PrevParamSet + kByteOffset];
	dispMsg << "\nMIDI ch/dev ID: " << (int)bytes[(int)EdpGlobalParamIndexes::MIDIChannel + kByteOffset] << "/" << (int)bytes[(int)EdpGlobalParamIndexes::MIDIDevID + kByteOffset];
	dispMsg << "\nCtrl/loop source: " << (int)bytes[(int)EdpGlobalParamIndexes::MIDIFirstKey + kByteOffset] << "/" << (int)bytes[(int)EdpGlobalParamIndexes::MIDIFirstLoop + kByteOffset];
	dispMsg << "\nVolume/feedback ctrl: " << (int)bytes[(int)EdpGlobalParamIndexes::MIDIVolCtrlr + kByteOffset] << "/" << (int)bytes[(int)EdpGlobalParamIndexes::MIDIFBCtrlr + kByteOffset];
	mMainDisplay->TextOut(dispMsg.str());
}

// req: F0 00 01 30 0B 01 01 12 00 13 00 F7
// rec: F0 00 01 30 0B 01 13 00 13 00 00 00 00 03 00 00 02 00 00 00 00 00 03 01 02 05 00 00 00 F7
// bytes: 00 13 00 00 00 00 03 00 00 02 00 00 00 00 00 03 01 02 05 00 00 00 F7 (len 23)
void
EdpManager::ReceiveLocalParamData(const byte * bytes, int len)
{
	if (23 != len)
	{
		if (kDbgFlag && mTrace)
		{
			const std::string byteDump(::GetAsciiHexStr(bytes, len, true));
			std::ostringstream traceMsg;
			traceMsg << "EDP unexpected local param data len: " << byteDump.c_str() << '\n';
			mTrace->Trace(traceMsg.str());
		}
		return;
	}

	if (bytes[0] != 0 || bytes[1] != 0x13)
	{
		if (kDbgFlag && mTrace)
		{
			const std::string byteDump(::GetAsciiHexStr(bytes, len, true));
			std::ostringstream traceMsg;
			traceMsg << "EDP unexpected local param data: " << byteDump.c_str() << '\n';
			mTrace->Trace(traceMsg.str());
		}
		return;
	}

	constexpr int kByteOffset = 3;
	std::ostringstream dispMsg;
	dispMsg << "EDP Local State";
	if (bytes[2])
		dispMsg << "\nPreset: " << (int)bytes[2]; // preset 0 is the active/edit buffer
	if (bytes[3])
		dispMsg << "\nUnknown param: " << (int)bytes[3]; // local param index 0 is not documented

	dispMsg << "\nRecord mode: " << ::EdpEnumToString(static_cast<EdpRecordMode>(bytes[(int)EdpLocalParamIndexes::RecordMode + kByteOffset]));
	dispMsg << "\nOverdub mode: " << ::EdpEnumToString(static_cast<EdpOverdubMode>(bytes[(int)EdpLocalParamIndexes::OverdubMode + kByteOffset]));
	dispMsg << "\nInsert mode: " << ::EdpEnumToString(static_cast<EdpInsertMode>(bytes[(int)EdpLocalParamIndexes::InsertMode + kByteOffset]));
	dispMsg << "\nTime quantize mode: " << ::EdpEnumToString(static_cast<EdpTimeQuantization>(bytes[(int)EdpLocalParamIndexes::TimingQuantization + kByteOffset]));
	dispMsg << "\n8th/cycle: " << ::DecodeEdpEighthsPerCycle(bytes[(int)EdpLocalParamIndexes::EighthsPerCycle + kByteOffset]);
	dispMsg << "\nMute mode: " << ::EdpEnumToString(static_cast<EdpMuteMode>(bytes[(int)EdpLocalParamIndexes::MuteMode + kByteOffset]));
	dispMsg << "\nRound mode: " << ::EdpEnumToString(static_cast<EdpRoundMode>(bytes[(int)EdpLocalParamIndexes::RoundMode + kByteOffset]));

	dispMsg << "\nNext loop switch quantization: " << ::EdpEnumToString(static_cast<EdpLoopSwitchQuant>(bytes[(int)EdpLocalParamIndexes::SwitchLoopQuantization + kByteOffset]));
	dispMsg << "\nNext loop copy: " << ::EdpEnumToString(static_cast<EdpNextLoopCopy>(bytes[(int)EdpLocalParamIndexes::NextLoopCopy + kByteOffset]));
	dispMsg << "\nNext loop auto record: " << ::EdpEnumToString(static_cast<EdpAutoRecord>(bytes[(int)EdpLocalParamIndexes::AutoRecord + kByteOffset]));
	dispMsg << "\nSampler style: " << ::EdpEnumToString(static_cast<EdpSamplerStyle>(bytes[(int)EdpLocalParamIndexes::SamplerStyle + kByteOffset]));

	dispMsg << "\nRecord trigger threshold: " << (int)bytes[(int)EdpLocalParamIndexes::TrigThreshold + kByteOffset];
	dispMsg << "\nInterface mode: " << ::EdpEnumToString(static_cast<EdpInterfaceModes>(bytes[(int)EdpLocalParamIndexes::InterfaceMode + kByteOffset]));
	dispMsg << "\nLoops: " << (int)bytes[(int)EdpLocalParamIndexes::MoreLoops + kByteOffset] + 1;
	dispMsg << "\nSync mode: " << ::EdpEnumToString(static_cast<EdpSyncMode>(bytes[(int)EdpLocalParamIndexes::SyncMode + kByteOffset]));
	dispMsg << "\nVelocity: " << ::EdpEnumToString(static_cast<EdpVelocity>(bytes[(int)EdpLocalParamIndexes::Velocity + kByteOffset]));
	dispMsg << "\nOverflow: " << ::EdpEnumToString(static_cast<EdpOverflow>(bytes[(int)EdpLocalParamIndexes::Overflow + kByteOffset]));

	if (bytes[21])
		dispMsg << "\nTempo: " << ::DecodeEdpTempo(bytes[(int)EdpLocalParamIndexes::Tempo + kByteOffset]) << "bpm";

	mMainDisplay->TextOut(dispMsg.str());
}
