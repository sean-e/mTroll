/*
 * mTroll MIDI Controller
 * Copyright (C) 2021-2022 Sean Echevarria
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

#pragma once

enum class EdpSysexCommands
{
	InfoRequest				= 0,
	InfoData				= 1,
	// ...
	GlobalParamRequest		= 0x10,
	GlobalParamData			= 0x11,
	LocalParamRequest		= 0x12,
	LocalParamData			= 0x13,
	AllParamRequest			= 0x14,
	// ...
	GlobalParamReset		= 0x20,
	LocalParamReset			= 0x21
};

enum class EdpGlobalParamIndexes
{
	PrevParamSet			= 0,
	ParamSet,
	MIDIChannel,
	MIDIReceiveCommand,
	MIDIFirstKey,
	MIDIVolCtrlr,
	MIDIFBCtrlr,
	MIDIFirstLoop,
	MIDIDevID,
	MIDISampleNumHi,
	MIDISampleNumLo
};

enum class EdpLocalParamIndexes
{
	InterfaceMode		 = 1,	// 4 bits
	TimingQuantization		,	// 2 bits
	EighthsPerCycle			,	// 7 bits
	SyncMode				,	// 3 bits
	TrigThreshold			,	// 4 bits
	RecordMode				,	// 2 bits
	OverdubMode				,	// 1 bit
	RoundMode				,	// 1 bit
	InsertMode				,	// 4 bits
	MuteMode				,	// 1 bit
	Overflow				,	// 1 bit
	MoreLoops				,	// 4 bits (real NLoops - 1)
	AutoRecord				,	// 1 bit
	NextLoopCopy			,	// 2 bits
	SwitchLoopQuantization	,	// 3 bits
	Velocity				,	// 1 bit
	SamplerStyle			,	// 2 bits
	Tempo						// 7 bits
};

enum class EdpInterfaceModes
{
	LoopMode				= 0,	// LOP
	DelayMode				= 1,	// DEL
	ExpertMode				= 2,	// EXP
	StutterMode				= 3,	// Stu
	OutMode					= 4,	// Out
	InputMode				= 5,	// In
	ReplaceMode				= 6,	// rPL
	FlipMode				= 7		// FLI
};

const char * const EdpEnumToString(EdpInterfaceModes v) noexcept
{
	switch (v)
	{
	case EdpInterfaceModes::LoopMode:	return "LoopMode (LOP)";
	case EdpInterfaceModes::DelayMode:	return "DelayMode (DEL)";
	case EdpInterfaceModes::ExpertMode:	return "ExpertMode (EXP)";
	case EdpInterfaceModes::StutterMode: return "StutterMode (Stu)";
	case EdpInterfaceModes::OutMode:	return "OutMode (Out)";
	case EdpInterfaceModes::InputMode:	return "InputMode (In)";
	case EdpInterfaceModes::ReplaceMode: return "ReplaceMode (rPL)";
	case EdpInterfaceModes::FlipMode:	return "FlipMode (FLI)";
	default:							return "invalid value";
	}
}

enum class EdpTimeQuantization
{
	Off						= 0,	// OFF
	Cycle					= 1,	// CYC
	Eighth					= 2,	// 8th
	Loop					= 3		// LOP
};

const char * const EdpEnumToString(EdpTimeQuantization v) noexcept
{
	switch (v)
	{
	case EdpTimeQuantization::Off:		return "Off";
	case EdpTimeQuantization::Cycle:	return "Cycle (CYC)";
	case EdpTimeQuantization::Eighth:	return "8th";
	case EdpTimeQuantization::Loop:		return "Loop (LOP)";
	default:							return "invalid value";
	}
}

inline int DecodeEdpEighthsPerCycle(byte val)
{
	if (val > 0x69)
		return 0;

	switch (val)
	{ 
	case 0:		return 8;
	case 1:		return 4;
	case 2:		return 2;
	case 3:		return 6;
	case 4:		return 12;
	case 5:		return 16;
	case 6:		return 32;
	case 7:		return 64;
	case 8:		return 128;
	case 9:		return 256;
	default:
		return val - 9;
	}
}

enum class EdpSyncMode
{
	Off						= 0,	// Off
	OutUserStartSong		= 1,	// Ous
	SyncIn					= 2,	// In
	SyncOut					= 3		// Out
};

const char * const EdpEnumToString(EdpSyncMode v) noexcept
{
	switch (v)
	{
	case EdpSyncMode::Off:				return "Off";
	case EdpSyncMode::OutUserStartSong:	return "OutUserStartSong (Ous)";
	case EdpSyncMode::SyncIn:			return "SyncIn (In)";
	case EdpSyncMode::SyncOut:			return "SyncOut (Out)";
	default:							return "invalid value";
	}
}

// TrigThreshold: 4-bits values 0-8

enum class EdpRecordMode
{
	Toggle					= 0,	// toG
	Sustain					= 1,	// SUS
	Safe					= 2,	// SAF
	NA						= 3		// NA
};

const char * const EdpEnumToString(EdpRecordMode v) noexcept
{
	switch (v)
	{
	case EdpRecordMode::Toggle:		return "Toggle (toG)";
	case EdpRecordMode::Sustain:	return "Sustain (SUS)";
	case EdpRecordMode::Safe:		return "Safe (SAF)";
	case EdpRecordMode::NA:			return "NA";
	default:						return "invalid value";
	}
}

enum class EdpOverdubMode
{
	Toggle					= 0,	// toG
	Sustain					= 1		// SUS
};

const char * const EdpEnumToString(EdpOverdubMode v) noexcept
{
	switch (v)
	{
	case EdpOverdubMode::Toggle:		return "Toggle (toG)";
	case EdpOverdubMode::Sustain:		return "Sustain (SUS)";
	default:							return "invalid value";
	}
}

enum class EdpRoundMode
{
	Off						= 0,	// OFF
	Round					= 1		// RND
};

const char * const EdpEnumToString(EdpRoundMode v) noexcept
{
	switch (v)
	{
	case EdpRoundMode::Off:			return "Off";
	case EdpRoundMode::Round:		return "Round (RND)";
	default:						return "invalid value";
	}
}

enum class EdpInsertMode
{
	InsertOnly				= 0,	// INS
	Rehearse				= 1,	// rhr
	Replace					= 2,	// rPL
	Substitute				= 3,	// Sub
	Reverse					= 4,	// rEV
	HalfSpeed				= 5,	// h.SP
	Sustain					= 6		// SUS
};

const char * const EdpEnumToString(EdpInsertMode v) noexcept
{
	switch (v)
	{
	case EdpInsertMode::InsertOnly:		return "InsertOnly (INS)";
	case EdpInsertMode::Rehearse:		return "Rehearse (rhr)";
	case EdpInsertMode::Replace:		return "Replace (rPL)";
	case EdpInsertMode::Substitute:		return "Substitute (Sub)";
	case EdpInsertMode::Reverse:		return "Reverse (rEV)";
	case EdpInsertMode::HalfSpeed:		return "Half-Speed (h.SP)";
	case EdpInsertMode::Sustain:		return "Sustain (SUS)";
	default:							return "invalid value";
	}
}

enum class EdpMuteMode
{
	Continuous				= 0,	// Cnt
	Start					= 1		// STA
};

const char * const EdpEnumToString(EdpMuteMode v) noexcept
{
	switch (v)
	{
	case EdpMuteMode::Continuous:		return "Continuous (Cnt) (Undo will start; Insert will play once then mute)";
	case EdpMuteMode::Start:			return "Start (STA) (Undo will continue; Insert will play once then mute)";
	default:							return "invalid value";
	}
}

enum class EdpOverflow
{
	Play					= 0,	// PLY
	Stop					= 1		// StP
};

const char * const EdpEnumToString(EdpOverflow v) noexcept
{
	switch (v)
	{
	case EdpOverflow::Play:				return "Play (PLY)";
	case EdpOverflow::Stop:				return "Stop (StP)";
	default:							return "invalid value";
	}
}

// MoreLoops: 4-bits values 0-15 => 1-16 loops

enum class EdpAutoRecord
{
	Off						= 0,	// OFF
	On						= 1		// On
};

const char * const EdpEnumToString(EdpAutoRecord v) noexcept
{
	switch (v)
	{
	case EdpAutoRecord::Off:			return "Off";
	case EdpAutoRecord::On:				return "On";
	default:							return "invalid value";
	}
}

enum class EdpNextLoopCopy
{
	Off						= 0,	// OFF
	Timing					= 1,	// ti
	Sound					= 2,	// Snd
	NA						= 3		// NA
};

const char * const EdpEnumToString(EdpNextLoopCopy v) noexcept
{
	switch (v)
	{
	case EdpNextLoopCopy::Off:			return "Off";
	case EdpNextLoopCopy::Timing:		return "Timing (ti)";
	case EdpNextLoopCopy::Sound:		return "Sound or Off (EDP reporting error)"; // "Sound (Snd)"
	case EdpNextLoopCopy::NA:			return "NA";
	default:							return "invalid value";
	}
}

enum class EdpLoopSwitchQuant
{
	Off						= 0,	// OFF
	Confirm					= 1,	// CnF
	CycleQuantize			= 2,	// CYC
	ConfirmCycle			= 3,	// CCY
	LoopQuant				= 4,	// LOP
	ConfirmLoop				= 5		// CLP
};

const char * const EdpEnumToString(EdpLoopSwitchQuant v) noexcept
{
	switch (v)
	{
	case EdpLoopSwitchQuant::Off:			return "Off";
	case EdpLoopSwitchQuant::Confirm:		return "Confirm (CnF)";
	case EdpLoopSwitchQuant::CycleQuantize:	return "CycleQuantize (CYC)";
	case EdpLoopSwitchQuant::ConfirmCycle:	return "ConfirmCycle (CCY)";
	case EdpLoopSwitchQuant::LoopQuant:		return "LoopQuantize (LOP)";
	case EdpLoopSwitchQuant::ConfirmLoop:	return "ConfirmLoop (CLP)";
	default:								return "invalid value";
	}
}

enum class EdpVelocity
{
	Off						= 0,	// OFF
	On						= 1		// On
};

const char * const EdpEnumToString(EdpVelocity v) noexcept
{
	switch (v)
	{
	case EdpVelocity::Off:				return "Off";
	case EdpVelocity::On:				return "On";
	default:							return "invalid value";
	}
}

enum class EdpSamplerStyle
{
	Run						= 0,	// run
	Once					= 1,	// One
	Start					= 2,	// StA
	Attack					= 3		// Att
};

const char * const EdpEnumToString(EdpSamplerStyle v) noexcept
{
	switch (v)
	{
	case EdpSamplerStyle::Run:			return "Run";
	case EdpSamplerStyle::Once:			return "Once (One)";
	case EdpSamplerStyle::Start:		return "Start (StA)";
	case EdpSamplerStyle::Attack:		return "Attack (Att)";
	default:							return "invalid value";
	}
}

// Tempo in BPM = (val * 2) + 24 (except for val of 0)
inline int DecodeEdpTempo(byte tempoVal)
{
	if (0 == tempoVal)
		return 0; // off (tempo select off)

	// max val is 0x7f => 278 bpm
	if (tempoVal > 127)
		return 278;

	return (tempoVal * 2) + 24;
}

// 16 total presets
// 15 for user
// preset 0 is the loaded/active space/buffer
// presets are 1-15
