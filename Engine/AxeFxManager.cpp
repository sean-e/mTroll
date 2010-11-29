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


// TODO: 
// axefx patch type or attribute?
// need queue (and lock) for outgoing queries
// need timer for timeout on query response
//		poll after program changes on axe ch?
//		http://www.fractalaudio.com/forum/viewtopic.php?f=14&t=21524&start=10
// sysex difference between disabled and not present?


void NormalizeName(std::string &effectName);


AxeFxManager::AxeFxManager(ISwitchDisplay * switchDisp,
						   ITraceDisplay *pTrace,
						   std::string appPath) :
	mSwitchDisplay(switchDisp),
	mTrace(pTrace),
	mRefCnt(0),
	mTempoPatch(NULL)
{
	AxemlLoader ldr(mTrace);
	ldr.Load(appPath + "/default.axeml", mAxeEffectInfo);
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

AxeEffectBlockInfo *
AxeFxManager::IdentifyBlockInfo(const byte * bytes)
{
	const int effectIdLs = bytes[0];
	const int effectIdMs = bytes[1];
	const int parameterIdLs = bytes[2];
	const int parameterIdMs = bytes[3];

	for (AxeEffectBlocks::iterator it = mAxeEffectInfo.begin(); 
		it != mAxeEffectInfo.end(); 
		++it)
	{
		AxeEffectBlockInfo & cur = *it;
		if (cur.mSysexEffectIdLs == effectIdLs &&
			cur.mSysexEffectIdMs == effectIdMs &&
			cur.mSysexBypassParameterIdLs == parameterIdLs &&
			cur.mSysexBypassParameterIdMs == parameterIdMs)
			return &(*it);
	}

	return NULL;
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

	AxeEffectBlockInfo * inf = IdentifyBlockInfo(bytes);
	if (!inf || !inf->mPatch /*|| !inf->mBypassCC*/)
		return;

	const bool isActive = 0 != bytes[4];
	if (inf->mPatch->IsActive() != isActive)
		inf->mPatch->UpdateState(mSwitchDisplay, isActive);

// 	if (mTrace)
// 	{
// 		const std::string msg(::GetAsciiHexStr(bytes, len, true) + "\n");
// 		mTrace->Trace(msg);
// 	}
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

struct DefaultAxeCcs
{
	std::string mParameter;
	int			mCc;
};

DefaultAxeCcs kDefaultAxeCcs[] = 
{
	// these don't have defaults (some can't be bypassed)
	{"feedback send", 0},
	{"send", 0},
	{"mixer 1", 0},
	{"mix 1", 0},
	{"mixer 2", 0},
	{"mix 2", 0},
	{"feedback return", 0},
	{"return", 0},
	{"noisegate", 0},
	{"input volume", 10},
	{"input vol", 10},
	{"out 1 volume", 11},
	{"out 1 vol", 11},
	{"out 2 volume", 12},
	{"out 2 vol", 12},
	{"output", 13},
	{"tap tempo", 14},
	{"tuner", 15},
	{"external 1", 16},
	{"ext 1", 16},
	{"external 2", 17},
	{"ext 2", 17},
	{"external 3", 18},
	{"ext 3", 18},
	{"external 4", 19},
	{"ext 4", 19},
	{"external 5", 20},
	{"ext 5", 20},
	{"external 6", 21},
	{"ext 6", 21},
	{"external 7", 22},
	{"ext 7", 22},
	{"external 8", 23},
	{"ext 8", 23},

	{"looper 1 record", 24},
	{"looper 1 play", 25},
	{"looper 1 once", 26},
	{"looper 1 overdub", 27},
	{"looper 1 reverse", 28},
	{"looper 2 record", 29},
	{"looper 2 play", 30},
	{"looper 2 once", 31},
	{"looper 2 overdub", 32},
	{"looper 2 reverse", 33},

	{"global preset effect toggle", 34},
	{"vol increment", 35},
	{"volume increment", 35},
	{"vol decrement", 36},
	{"volume decrement", 36},

	{"amp 1", 37},
	{"amp 2", 38},
	{"cabinet 1", 39},
	{"cab 1", 39},
	{"cabinet 2", 40},
	{"cab 2", 40},
	{"chorus 1", 41},
	{"chorus 2", 42},
	{"compressor 1", 43},
	{"comp 1", 43},
	{"compressor 2", 44},
	{"comp 2", 44},
	{"crossover 1", 45},
	{"x-over 1", 45},
	{"crossover 2", 46},
	{"x-over 2", 46},
	{"delay 1", 47},
	{"delay 1 (reverse)", 47},
	{"delay 2", 48},
	{"delay 2 (reverse)", 48},
	{"drive 1", 49},
	{"drive 2", 50},
	{"enhancer", 51},
	{"filter 1", 52},
	{"filter 2", 53},
	{"filter 3", 54},
	{"filter 4", 55},
	{"flanger 1", 56},
	{"flanger 2", 57},
	{"formant", 58},
	{"fx loop", 59},
	{"effects loop", 59},
	{"gate 1", 60},
	{"gate/expander 1", 60},
	{"gate 2", 61},
	{"gate/expander 2", 61},
	{"graphiceq 1", 62},
	{"graphic eq 1", 62},
	{"graphiceq 2", 63},
	{"graphic eq 2", 63},
	{"graphiceq 3", 64},
	{"graphic eq 3", 64},
	{"graphiceq 4", 65},
	{"graphic eq 4", 65},
	{"megatap delay", 66},
	{"metatap", 66},
	{"multiband comp 1", 67},
	{"multi comp 1", 67},
	{"multiband comp 2", 68},
	{"multi comp 1", 68},
	{"multidelay 1", 69},
	{"multi delay 1", 69},
	{"multidelay 2", 70},
	{"multi delay 2", 70},
	{"para eq 1", 71},
	{"parametric eq 1", 71},
	{"para eq 2", 72},
	{"parametric eq 2", 72},
	{"para eq 3", 73},
	{"parametric eq 3", 73},
	{"para eq 4", 74},
	{"parametric eq 4", 74},
	{"phaser 1", 75},
	{"phaser 2", 76},
	{"pitch 1", 77},
	{"pitch 1 (whammy)", 77},
	{"pitch 2", 78},
	{"pitch 2 (whammy)", 78},
	{"quad chorus 1", 79},
	{"quad 1", 79},
	{"quad chorus 2", 80},
	{"quad 2", 80},
	{"resonator 1", 81},
	{"resonator 2", 82},
	{"reverb 1", 83},
	{"reverb 2", 84},
	{"ring mod", 85},
	{"ringmod", 85},
	{"rotary 1", 86},
	{"rotary 2", 87},
	{"synth 1", 88},
	{"synth 2", 89},
	{"tremolo 1", 90},
	{"trem 1", 90},
	{"panner/tremolo 1", 90},
	{"tremolo 2", 91},
	{"trem 2", 91},
	{"panner/tremolo 2", 91},
	{"vocoder", 92},
	{"volume 1", 93},
	{"vol 1", 93},
	{"vol/pan 1", 93},
	{"volume 2", 94},
	{"vol 2", 94},
	{"vol/pan 2", 94},
	{"volume 3", 95},
	{"vol 3", 95},
	{"vol/pan 3", 95},
	{"volume 4", 96},
	{"vol 4", 96},
	{"vol/pan 4", 96},
	{"wah-wah 1", 97},
	{"wahwah 1", 97},
	{"wah 1", 97},
	{"wah-wah 2", 98},
	{"wahwah 2", 98},
	{"wah 2", 98},
	{"", -1}
};

int
GetDefaultAxeCc(const std::string &effectNameIn, ITraceDisplay * trc) 
{
	std::string effectName(effectNameIn);
	NormalizeName(effectName);

	for (int idx = 0; !kDefaultAxeCcs[idx].mParameter.empty(); ++idx)
		if (kDefaultAxeCcs[idx].mParameter == effectName)
			return kDefaultAxeCcs[idx].mCc;

	if (trc)
	{
		std::string msg("ERROR: AxeFx default CC for " + effectName + " is unknown\n");
		trc->Trace(msg);
	}
	return 0;
}
