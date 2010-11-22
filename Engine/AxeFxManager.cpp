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
#include <algorithm>
#include "AxeFxManager.h"
#include "ITraceDisplay.h"
#include "HexStringUtils.h"
#include "IMidiIn.h"
#include "Patch.h"
#include "ISwitchDisplay.h"
#include "AxemlLoader.h"


void NormalizeName(std::string &effectName);


AxeFxManager::AxeFxManager(ISwitchDisplay * switchDisp,
						   ITraceDisplay *pTrace) :
	mSwitchDisplay(switchDisp),
	mTrace(pTrace),
	mRefCnt(0),
	mTempoPatch(NULL)
{
	AxemlLoader ldr(mTrace);
	ldr.Load("debug/default.axeml", mAxeEffectInfo);
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
AxeFxManager::SetTempoPatch(Patch * patch)
{
	if (mTempoPatch && mTrace)
	{
		std::string msg("Warning: multiple Axe-Fx Tap Tempo patches\n");
		mTrace->Trace(msg);
	}
	mTempoPatch = patch;
}

void
AxeFxManager::SetSyncPatch(const std::string & effectNameIn, Patch * patch)
{
	std::string effectName(effectNameIn);
	::NormalizeName(effectName);
	for (AxeEffectBlocks::iterator it = mAxeEffectInfo.begin(); 
		it != mAxeEffectInfo.end(); 
		++it)
	{
		AxeEffectBlockInfo & cur = *it;
		if (cur.mName == effectName)
		{
			if (cur.mPatch && mTrace)
			{
				std::string msg("Warning: multiple Axe-Fx patches for " + effectNameIn + "\n");
				mTrace->Trace(msg);
			}
			cur.mPatch = patch;
			return;
		}
	}

	if (mTrace)
	{
		std::string msg("Warning: no Axe-Fx effect found to sync for " + effectNameIn + "\n");
		mTrace->Trace(msg);
	}
}

void
AxeFxManager::CompleteInit()
{
	// TODO: create indexes?
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

void 
NormalizeName(std::string &effectName) 
{
	std::transform(effectName.begin(), effectName.end(), effectName.begin(), ::tolower);

	int pos = effectName.find("axe");
	if (-1 == pos)
		return;

	pos = effectName.find("axe ");
	if (-1 == pos)
		pos = effectName.find("axefx ");
	if (-1 == pos)
		pos = effectName.find("axe-fx ");
	if (-1 != pos)
		effectName = effectName.substr(pos);
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


int
GetDefaultAxeCc(const std::string &effectNameIn, ITraceDisplay * trc) 
{
	std::string effectName(effectNameIn);
	NormalizeName(effectName);

	if (effectName == "feedback send" || 
		effectName == "mixer 1" || 
		effectName == "mixer 2" ||
		effectName == "feedback return" ||
		effectName == "noisegate")
		return 0; // these don't have defaults (some can't be bypassed)

	if (effectName == "input volume")
		return 10;
	if (effectName == "out 1 volume")
		return 11;
	if (effectName == "out 2 volume")
		return 12;
	if (effectName == "output")
		return 13;
	if (effectName == "tap tempo")
		return 14;
	if (effectName == "tuner")
		return 15;

	if (effectName == "external 1" || effectName == "ext 1")
		return 16;
	if (effectName == "external 2" || effectName == "ext 2")
		return 17;
	if (effectName == "external 3" || effectName == "ext 3")
		return 18;
	if (effectName == "external 4" || effectName == "ext 4")
		return 19;
	if (effectName == "external 5" || effectName == "ext 5")
		return 20;
	if (effectName == "external 6" || effectName == "ext 6")
		return 21;
	if (effectName == "external 7" || effectName == "ext 7")
		return 22;
	if (effectName == "external 8" || effectName == "ext 8")
		return 23;

	if (effectName == "looper 1 record")
		return 24;
	if (effectName == "looper 1 play")
		return 25;
	if (effectName == "looper 1 once")
		return 26;
	if (effectName == "looper 1 overdub")
		return 27;
	if (effectName == "looper 1 reverse")
		return 28;
	if (effectName == "looper 2 record")
		return 29;
	if (effectName == "looper 2 play")
		return 30;
	if (effectName == "looper 2 once")
		return 31;
	if (effectName == "looper 2 overdub")
		return 32;
	if (effectName == "looper 2 reverse")
		return 33;

	if (effectName == "global preset effect toggle")
		return 34;
	if (effectName == "volume increment")
		return 35;
	if (effectName == "volume decrement")
		return 36;

	if (effectName == "amp 1")
		return 37;
	if (effectName == "amp 2")
		return 38;
	if (effectName == "cabinet 1")
		return 39;
	if (effectName == "cabinet 2")
		return 40;
	if (effectName == "chorus 1")
		return 41;
	if (effectName == "chorus 2")
		return 42;
	if (effectName == "compressor 1" || effectName == "comp 1")
		return 43;
	if (effectName == "compressor 2" || effectName == "comp 2")
		return 44;
	if (effectName == "crossover 1" || effectName == "x-over 1")
		return 45;
	if (effectName == "crossover 2" || effectName == "x-over 2")
		return 46;
	if (effectName == "delay 1" || effectName == "delay 1 (reverse)")
		return 47;
	if (effectName == "delay 2" || effectName == "delay 2 (reverse)")
		return 48;
	if (effectName == "drive 1")
		return 49;
	if (effectName == "drive 2")
		return 50;
	if (effectName == "enhancer")
		return 51;
	if (effectName == "filter 1")
		return 52;
	if (effectName == "filter 2")
		return 53;
	if (effectName == "filter 3")
		return 54;
	if (effectName == "filter 4")
		return 55;
	if (effectName == "flanger 1")
		return 56;
	if (effectName == "flanger 2")
		return 57;
	if (effectName == "formant")
		return 58;
	if (effectName == "fx loop" || effectName == "effects loop")
		return 59;
	if (effectName == "gate 1" || effectName == "gate/expander 1")
		return 60;
	if (effectName == "gate 2" || effectName == "gate/expander 2")
		return 61;
	if (effectName == "graphiceq 1" || effectName == "graphic eq 1")
		return 62;
	if (effectName == "graphiceq 2" || effectName == "graphic eq 2")
		return 63;
	if (effectName == "graphiceq 3" || effectName == "graphic eq 3")
		return 64;
	if (effectName == "graphiceq 4" || effectName == "graphic eq 4")
		return 65;
	if (effectName == "megatap delay" || effectName == "metatap")
		return 66;
	if (effectName == "multiband comp 1" || effectName == "multi comp 1")
		return 67;
	if (effectName == "multiband comp 2" || effectName == "multi comp 1")
		return 68;
	if (effectName == "multidelay 1" || effectName == "multi delay 1")
		return 69;
	if (effectName == "multidelay 2" || effectName == "multi delay 2")
		return 70;
	if (effectName == "para eq 1" || effectName == "parametric eq 1")
		return 71;
	if (effectName == "para eq 2" || effectName == "parametric eq 2")
		return 72;
	if (effectName == "para eq 3" || effectName == "parametric eq 3")
		return 73;
	if (effectName == "para eq 4" || effectName == "parametric eq 4")
		return 74;
	if (effectName == "phaser 1")
		return 75;
	if (effectName == "phaser 2")
		return 76;
	if (effectName == "pitch 1" || effectName == "pitch 1 (whammy)")
		return 77;
	if (effectName == "pitch 2" || effectName == "pitch 2 (whammy)")
		return 78;
	if (effectName == "quad chorus 1")
		return 79;
	if (effectName == "quad chorus 2")
		return 80;
	if (effectName == "resonator 1")
		return 81;
	if (effectName == "resonator 2")
		return 82;
	if (effectName == "reverb 1")
		return 83;
	if (effectName == "reverb 2")
		return 84;
	if (effectName == "ring mod" || effectName == "ringmod")
		return 85;
	if (effectName == "rotary 1")
		return 86;
	if (effectName == "rotary 2")
		return 87;
	if (effectName == "synth 1")
		return 88;
	if (effectName == "synth 2")
		return 89;
	if (effectName == "tremolo 1" || effectName == "panner/tremolo 1")
		return 90;
	if (effectName == "tremolo 2" || effectName == "panner/tremolo 2")
		return 91;
	if (effectName == "vocoder")
		return 92;
	if (effectName == "volume 1" || effectName == "vol/pan 1")
		return 93;
	if (effectName == "volume 2" || effectName == "vol/pan 2")
		return 94;
	if (effectName == "volume 3" || effectName == "vol/pan 3")
		return 95;
	if (effectName == "volume 4" || effectName == "vol/pan 4")
		return 96;
	if (effectName == "wah-wah 1" || effectName == "wahwah 1")
		return 97;
	if (effectName == "wah-wah 2" || effectName == "wahwah 2")
		return 98;

	if (trc)
	{
		std::string msg("ERROR: AxeFx default CC for " + effectName + " is unknown\n");
		trc->Trace(msg);
	}
	return 0;
}
