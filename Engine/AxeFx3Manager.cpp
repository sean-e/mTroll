/*
 * mTroll MIDI Controller
 * Copyright (C) 2020 Sean Echevarria
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
#include <strstream>
#include <algorithm>
#include <QEvent>
#include <QApplication>
#include <QTimer>
#include <atomic>
#include "AxeFx3Manager.h"
#include "ITraceDisplay.h"
#include "HexStringUtils.h"
#include "IMidiIn.h"
#include "Patch.h"
#include "ISwitchDisplay.h"
#include "IMidiOut.h"
#include "IMainDisplay.h"
#include "AxeFxManager.h"
#include "AxeFx3EffectIds.h"


#ifdef _DEBUG
static const int kDbgFlag = 1;
#else
static const int kDbgFlag = 0;
#endif

static const int kDefaultNameSyncTimerInterval = 100;
static const int kDefaultEffectsSyncTimerInterval = 20;
#ifdef ITEM_COUNTING
std::atomic<int> gAxeFx3MgrCnt = 0;
#endif

static void NormalizeAxe3EffectName(std::string &effectName);
static void Axe3SynonymNormalization(std::string & name);

struct Axe3EffectBlockInfo
{
	std::string		mName;						// Amp 1
	std::string		mNormalizedName;			// amp 1
	int				mSysexEffectId;				// unique per name
	int				mSysexEffectIdMs;			//	derived 
	int				mSysexEffectIdLs;			//	derived
	bool			mEffectIsPresentInAxePatch;	// unique per name
	PatchPtr		mPatch;						// the patch assigned to this effectId
	std::vector<PatchPtr> mDependentPatches;	// optional dependent patches for this effectId, like channel select

	Axe3EffectBlockInfo() :
		mSysexEffectId(-1),
		mSysexEffectIdLs(-1),
		mSysexEffectIdMs(-1),
		mEffectIsPresentInAxePatch(true)
	{
	}

	Axe3EffectBlockInfo(int id, const std::string & name) :
		mName(name),
		mNormalizedName(name),
		mSysexEffectId(id),
		mEffectIsPresentInAxePatch(true)
	{
		mSysexEffectIdLs = mSysexEffectId & 0x0000000F;
		mSysexEffectIdMs = (mSysexEffectId >> 4) & 0x0000000F;
		::NormalizeAxe3EffectName(mNormalizedName);
	}
};


AxeFx3Manager::AxeFx3Manager(IMainDisplay * mainDisp, 
						   ISwitchDisplay * switchDisp,
						   ITraceDisplay *pTrace,
						   const std::string & appPath, 
						   int ch, 
						   AxeFxModel mod) :
	mSwitchDisplay(switchDisp),
	mMainDisplay(mainDisp),
	mTrace(pTrace),
	mLastTimeout(0),
	// mQueryLock(QMutex::Recursive),
	mMidiOut(nullptr),
	mFirmwareMajorVersion(0),
	mAxeChannel(ch),
	mModel(mod),
	mLooperState(0),
	mCurrentAxePreset(-1),
	mCurrentScene(-1)
{
#ifdef ITEM_COUNTING
	++gAxeFx3MgrCnt;
#endif

	::memset(mScenes, 0, sizeof(mScenes));
	::memset(mLooperPatches, 0, sizeof(mLooperPatches));

	mDelayedNameSyncTimer = new QTimer(this);
	connect(mDelayedNameSyncTimer, SIGNAL(timeout()), this, SLOT(SyncNameAndEffectsFromAxe()));
	mDelayedNameSyncTimer->setSingleShot(true);
	mDelayedNameSyncTimer->setInterval(kDefaultNameSyncTimerInterval);

	mDelayedEffectsSyncTimer = new QTimer(this);
	connect(mDelayedEffectsSyncTimer, SIGNAL(timeout()), this, SLOT(SyncEffectsFromAxe()));
	mDelayedEffectsSyncTimer->setSingleShot(true);
	mDelayedEffectsSyncTimer->setInterval(kDefaultEffectsSyncTimerInterval);

	LoadEffectPool();
}

AxeFx3Manager::~AxeFx3Manager()
{
	Shutdown();

#ifdef ITEM_COUNTING
	--gAxeFx3MgrCnt;
#endif
}

void
AxeFx3Manager::SetTempoPatch(PatchPtr patch)
{
	if (mTempoPatch && mTrace)
	{
		std::string msg("Warning: multiple Axe-Fx Tap Tempo patches\n");
		mTrace->Trace(msg);
	}
	mTempoPatch = patch;
}

void
AxeFx3Manager::SetScenePatch(int scene, PatchPtr patch)
{
	if (scene < 1 || scene > AxeScenes)
	{
		if (mTrace)
		{
			std::string msg("Warning: invalid Axe-Fx scene number (use 1-8)\n");
			mTrace->Trace(msg);
		}
		return;
	}

	if (mScenes[scene - 1] && mTrace)
	{
		std::string msg("Warning: multiple Axe-Fx patches for same scene\n");
		mTrace->Trace(msg);
	}

	mScenes[scene - 1] = patch;
}

bool
AxeFx3Manager::SetSyncPatch(PatchPtr patch, int bypassCc /*= -1*/)
{
	std::string normalizedEffectName(patch->GetName());
	::NormalizeAxe3EffectName(normalizedEffectName);
	for (Axe3EffectBlockInfo & cur : mAxeEffectInfo)
	{
		if (cur.mNormalizedName == normalizedEffectName)
		{
			if (cur.mPatch && mTrace)
			{
				std::string msg("Warning: multiple Axe-Fx patches for " + patch->GetName() + "\n");
				mTrace->Trace(msg);
			}

// 			if (-1 != bypassCc)
// 				cur.mBypassCC = bypassCc;

			cur.mPatch = patch;
			return true;
		}
	}

	const std::string xy(" x/y");
	const int xyPos = normalizedEffectName.find(xy);
	if (-1 != xyPos)
	{
		normalizedEffectName.replace(xyPos, xy.length(), "");
		for (Axe3EffectBlockInfo & cur : mAxeEffectInfo)
		{
			if (cur.mNormalizedName == normalizedEffectName)
			{
// 				if (cur.mXyPatch && mTrace)
// 				{
// 					std::string msg("Warning: multiple Axe-Fx patches for XY control '" + patch->GetName() + "'\n");
// 					mTrace->Trace(msg);
// 				}
// 
// 				cur.mXyPatch = patch;
				return true;
			}
		}

		if (mTrace)
		{
			std::string msg("Warning: no Axe-Fx effect found to sync for x/y control '" + patch->GetName() + "'\n");
			mTrace->Trace(msg);
		}
		return false;
	}

	if (mTrace && 
		0 != normalizedEffectName.find("looper ") &&
		0 != normalizedEffectName.find("external ") &&
		normalizedEffectName != "tuner" &&
		normalizedEffectName != "global preset effect toggle" &&
		normalizedEffectName != "volume increment" &&
		normalizedEffectName != "volume decrement" &&
		normalizedEffectName != "scene increment" &&
		normalizedEffectName != "scene decrement")
	{
		std::string msg("Warning: no Axe-Fx effect found to sync for '" + patch->GetName() + "'\n");
		mTrace->Trace(msg);
	}

	return false;
}

void
AxeFx3Manager::CompleteInit(IMidiOutPtr midiOut)
{
	mMidiOut = midiOut;
	if (!mMidiOut && mTrace)
	{
		const std::string msg("ERROR: no Midi Out set for AxeFx sync\n");
		mTrace->Trace(msg);
	}

	SendFirmwareVersionQuery();
}

void
AxeFx3Manager::SubscribeToMidiIn(IMidiInPtr midiIn)
{
	midiIn->Subscribe(shared_from_this());
}

void
AxeFx3Manager::Closed(IMidiInPtr midIn)
{
	midIn->Unsubscribe(shared_from_this());
}

void
AxeFx3Manager::ReceivedData(byte b1, byte b2, byte b3)
{
// 	if (mTrace)
// 	{
// 		const std::string msg(::GetAsciiHexStr((byte*)&dwParam1, 4, true) + "\n");
// 		mTrace->Trace(msg);
// 	}
}

// f0 00 01 74 10 ...
bool
IsAxeFx3Sysex(const byte * bytes, const int len)
{
	if (len < 7)
		return false;

	const byte kAxeFx3[] = { 0xf0, 0x00, 0x01, 0x74, 0x10 };
	if (::memcmp(bytes, kAxeFx3, 5))
		return false;

	return true;
}

void
AxeFx3Manager::ReceivedSysex(const byte * bytes, int len)
{
	if (!::IsAxeFx3Sysex(bytes, len))
		return;

	switch (bytes[5])
	{
	case 2:
		return;
	case 4:
		// receive patch dump
		if (kDbgFlag && mTrace)
		{
			const std::string msg("AxeFx: received patch dump\n");
			mTrace->Trace(msg);
		}
		return;
	case 8:
		if (mFirmwareMajorVersion)
			return;
		ReceiveFirmwareVersionResponse(bytes, len);
		return;
	case 0xe:
		ReceivePresetEffectsV2(&bytes[6], len - 6);
		return;
	case 0xf:
		ReceivePresetName(&bytes[6], len - 6);
		return;
	case 0x10:
		// Tempo: f0 00 01 74 10 10 f7
		if (mTempoPatch)
		{
			mTempoPatch->ActivateSwitchDisplay(mSwitchDisplay, true);
			mSwitchDisplay->SetIndicatorThreadSafe(false, mTempoPatch, 75);
		}
		return;
	case 0x11:
		// tuner info
		// nn ss cc
		// nn = note 0-11
		// ss = string 0-5, 0 = low E
		// cc = cents offset binary, 63 = 0, 62 = -1, 64 = +1
		return;
	case 0x14:
		// preset loaded
		ReceivePresetNumber(&bytes[6], len - 6);
		DelayedNameSyncFromAxe(true);
		return;
	case 0x20:
		// routing grid layout
		return;
	case 0x21:
		// x/y change or scene selected
		DelayedEffectsSyncFromAxe();
		return;
	case 0x23:
		// looper status
		ReceiveLooperStatus(&bytes[6], len - 6);
		return;
	case 0x29:
		// scene status/update
		ReceiveSceneStatus(&bytes[6], len - 6);
		return;
	case 0x64:
		// indicates an error or unsupported message
		if (kDbgFlag && mTrace)
		{
			if ((len - 5) > 2 && bytes[6] == 0x23)
			{
				// ignore looper status monitor ack.
				// the message to enable the monitor is required, the status messages are not 
				// otherwise sent; but no idea why 0x64 is sent as ack.
				break;
			}
			else
			{
				const std::string msg("AxeFx: error or unsupported message\n");
				mTrace->Trace(msg);
			}
		}
	default:
		if (kDbgFlag && mTrace)
		{
			const std::string byteDump(::GetAsciiHexStr(&bytes[5], len - 5, true));
			std::strstream traceMsg;
			traceMsg << byteDump.c_str() << std::endl << std::ends;
			mTrace->Trace(std::string(traceMsg.str()));
		}
	}
}

Axe3EffectBlockInfo *
AxeFx3Manager::IdentifyBlockInfoUsingCc(const byte * bytes)
{
	int effectId;
	int cc;

	const int ccLs = bytes[0] >> 1;
	const int ccMs = (bytes[1] & 1) << 6;
	cc = ccMs | ccLs;

	const int effectIdLs = bytes[2] >> 3;
	const int effectIdMs = (bytes[3] << 4) & 0xff;
	effectId = effectIdMs | effectIdLs;

	for (auto & cur : mAxeEffectInfo)
	{
		if (cur.mSysexEffectId == effectId /*&& cur.mBypassCC == cc*/)
			return &cur;
	}

	return nullptr;
}

// This method is provided for last chance guess.  Only use if all other methods
// fail.  It can return incorrect information since many effects are available
// in quantity.  This will return the first effect Id match to the exclusion
// of instance id.  It is meant for Feedback Return which is a single instance
// effect that has no default cc for bypass.
Axe3EffectBlockInfo *
AxeFx3Manager::IdentifyBlockInfoUsingEffectId(const byte * bytes)
{
	int effectId;
	const int effectIdLs = bytes[2] >> 3;
	const int effectIdMs = (bytes[3] << 4) & 0xff;
	effectId = effectIdMs | effectIdLs;

	for (auto & cur : mAxeEffectInfo)
	{
		if (cur.mSysexEffectId == effectId)
			return &cur;
	}

	return nullptr;
}

Axe3EffectBlocks::iterator
AxeFx3Manager::GetBlockInfo(PatchPtr patch)
{
	for (Axe3EffectBlocks::iterator it = mAxeEffectInfo.begin(); 
		it != mAxeEffectInfo.end(); 
		++it)
	{
		const Axe3EffectBlockInfo & cur = *it;
		if (cur.mPatch == patch)
			return it;
	}

	return mAxeEffectInfo.end();
}

Axe3EffectBlocks::iterator
AxeFx3Manager::GetBlockInfo(const std::string& normalizedEffectName)
{
	for (Axe3EffectBlocks::iterator it = mAxeEffectInfo.begin();
		it != mAxeEffectInfo.end();
		++it)
	{
		const Axe3EffectBlockInfo & cur = *it;
		if (cur.mNormalizedName == normalizedEffectName)
			return it;
	}

	return mAxeEffectInfo.end();
}

void
AxeFx3Manager::SyncNameAndEffectsFromAxe()
{
	RequestPresetName();
}

void
AxeFx3Manager::SyncEffectsFromAxe()
{
	RequestPresetEffects();
}

void
AxeFx3Manager::Shutdown()
{
	if (mDelayedNameSyncTimer)
	{
		if (mDelayedNameSyncTimer->isActive())
			mDelayedNameSyncTimer->stop();
		delete mDelayedNameSyncTimer;
		mDelayedNameSyncTimer = nullptr;
	}

	if (mDelayedEffectsSyncTimer)
	{
		if (mDelayedEffectsSyncTimer->isActive())
			mDelayedEffectsSyncTimer->stop();
		delete mDelayedEffectsSyncTimer;
		mDelayedEffectsSyncTimer = nullptr;
	}

	mAxeEffectInfo.clear();
	mTempoPatch = nullptr;

	for (auto & cur : mLooperPatches)
		cur = nullptr;

	for (auto & cur : mScenes)
		cur = nullptr;
}

void
AxeFx3Manager::DelayedNameSyncFromAxe(bool force /*= false*/)
{
	if (!mDelayedNameSyncTimer)
		return;

	if (!force)
	{
		// Axe-Fx II sends preset loaded message, so we can ignore our 
		// unforced calls to DelalyedNameSyncFromAxe.
		return;
	}

	if (mDelayedNameSyncTimer->isActive())
	{
		class StopDelayedSyncTimer : public QEvent
		{
			AxeFx3ManagerPtr mMgr;

		public:
			StopDelayedSyncTimer(AxeFx3ManagerPtr mgr) :
			  QEvent(User), 
				  mMgr(mgr)
			{
			}

			~StopDelayedSyncTimer()
			{
				mMgr->mDelayedNameSyncTimer->stop();
			}
		};

		QCoreApplication::postEvent(this, new StopDelayedSyncTimer(GetSharedThis()));
	}

	class StartDelayedSyncTimer : public QEvent
	{
		AxeFx3ManagerPtr mMgr;

	public:
		StartDelayedSyncTimer(AxeFx3ManagerPtr mgr) :
		  QEvent(User), 
		  mMgr(mgr)
		{
		}

		~StartDelayedSyncTimer()
		{
			mMgr->mDelayedNameSyncTimer->start();
		}
	};

	QCoreApplication::postEvent(this, new StartDelayedSyncTimer(GetSharedThis()));
}

void
AxeFx3Manager::DelayedEffectsSyncFromAxe()
{
	if (!mDelayedEffectsSyncTimer)
		return;

	if (mDelayedEffectsSyncTimer->isActive())
	{
		class StopDelayedSyncTimer : public QEvent
		{
			AxeFx3ManagerPtr mMgr;

		public:
			StopDelayedSyncTimer(AxeFx3ManagerPtr mgr) :
			  QEvent(User), 
				  mMgr(mgr)
			{
			}

			~StopDelayedSyncTimer()
			{
				mMgr->mDelayedEffectsSyncTimer->stop();
			}
		};

		QCoreApplication::postEvent(this, new StopDelayedSyncTimer(GetSharedThis()));
	}

	class StartDelayedSyncTimer : public QEvent
	{
		AxeFx3ManagerPtr mMgr;

	public:
		StartDelayedSyncTimer(AxeFx3ManagerPtr mgr) :
		  QEvent(User), 
		  mMgr(mgr)
		{
		}

		~StartDelayedSyncTimer()
		{
			mMgr->mDelayedEffectsSyncTimer->start();
		}
	};

	QCoreApplication::postEvent(this, new StartDelayedSyncTimer(GetSharedThis()));
}

Bytes
AxeFx3Manager::GetCommandString(const std::string& commandName, bool enable)
{
	const Bytes emptyCommand;
	if (commandName.empty())
		return emptyCommand;

	// get normalizedName
	std::string name(commandName);
	::NormalizeAxe3EffectName(name);

	if ('t' == name[0])
	{
		if ("taptempo" == name)
		{
			if (enable)
			{
				// Tempo Tap (command 10H)
				Bytes bytes{ 0xf0, 0x00, 0x01, 0x74, 0x10, 0x10 };
				AppendChecksumAndTerminate(bytes);
				return bytes;
			}

			return emptyCommand;
		}

		if ("tuner" == name)
		{
			Bytes bytes{ 0xf0, 0x00, 0x01, 0x74, 0x10, 0x11 };
			bytes.push_back(enable ? 1 : 0);
			AppendChecksumAndTerminate(bytes);
			return bytes;
		}
	}

	// lookup name to get effect ID
	Axe3EffectBlocks::iterator fx = GetBlockInfo(name);
	if (fx == mAxeEffectInfo.end())
	{
		if (-1 != name.find("loop"))
		{
			// #axe3implementLoopCommandString add support for looper state command 0x0f
			return emptyCommand;
		}

		int pos = name.find("scene");
		if (-1 != pos)
		{
			// scene select command
			if (enable)
			{
				const std::string sceneStr(&name.c_str()[pos + 5]);
				if (!sceneStr.empty())
				{
					int axeFxScene;
					if (sceneStr.length() > 1 && sceneStr[0] == ' ')
						axeFxScene = ::atoi(&sceneStr.c_str()[1]);
					else
						axeFxScene = ::atoi(sceneStr.c_str());

					Bytes bytes{ 0xf0, 0x00, 0x01, 0x74, 0x10, 0x0c };
					bytes.push_back(axeFxScene - 1);
					AppendChecksumAndTerminate(bytes);
					return bytes;
				}
			}

			return emptyCommand;
		}

		return emptyCommand;
	}

	if (!fx->mSysexEffectId)
		return emptyCommand;

	// #axe3implementChannelCommandString add support for set channel command 0x0b

	// else, is an effect bypass command (command 0x0A)
	Bytes bytes{ 0xf0, 0x00, 0x01, 0x74, 0x10, 0x0A };
	bytes.push_back(fx->mSysexEffectIdLs);
	bytes.push_back(fx->mSysexEffectIdMs);
	bytes.push_back(enable ? 0 : 1);
	AppendChecksumAndTerminate(bytes);
	return bytes;
}

void
AxeFx3Manager::SendFirmwareVersionQuery()
{
	// request firmware vers axefx2: F0 00 01 74 03 08 0A 04 F7
	if (!mMidiOut)
		return;

	// Axe-Fx 3
	// --------
	// send:		F0 00 01 74 03 08 0A 04 F7
	// respond:		F0 00 01 74 03 08 02 00 0C F7	(for 2.0)
	// respond:		F0 00 01 74 03 08 03 02 0f F7	(for 3.2)

	Bytes bb{ 0xF0, 0x00, 0x01, 0x74, byte(mModel), 0x08, 0x0a };
	AppendChecksumAndTerminate(bb);
	QMutexLocker lock(&mQueryLock);
	mMidiOut->MidiOut(bb);
}

void
AxeFx3Manager::ReceiveFirmwareVersionResponse(const byte * bytes, int len)
{
	// response to firmware version request: F0 00 01 74 10 08 0A 03 F7
	if (len < 9)
		return;

	switch (bytes[4])
	{
	case Axe3:
		if (0xa == bytes[6] && 0x04 == bytes[7])
			return; // our request got looped back
		break;
	}

	std::string model;
	switch (bytes[4])
	{
	case Axe3:
		model = "III ";
		_ASSERTE(mModel == Axe3);
		break;
	default:
		model = "Unknown model ";
	}

	if (mTrace)
	{
		std::strstream traceMsg;
		traceMsg << "Axe-Fx " << model << "version " << (int) bytes[6] << "." << (int) bytes[7] << std::endl << std::ends;
		mTrace->Trace(std::string(traceMsg.str()));
	}

	mFirmwareMajorVersion = (int) bytes[6];
	EnableLooperStatusMonitor(true);
	SyncNameAndEffectsFromAxe();
}

void
AxeFx3Manager::RequestPresetName()
{
	if (!mFirmwareMajorVersion)
		SendFirmwareVersionQuery();

	QMutexLocker lock(&mQueryLock);
	if (!mFirmwareMajorVersion)
		return;

	Bytes bb{ 0xF0, 0x00, 0x01, 0x74, byte(mModel), 0x0f };
	AppendChecksumAndTerminate(bb);
	mMidiOut->MidiOut(bb);
}

void
AxeFx3Manager::ReceivePresetName(const byte * bytes, int len)
{
	if (len < 21)
		return;

	std::string patchName((const char *)bytes, 21);
	if (mMainDisplay && !patchName.empty())
	{
		mCurrentAxePresetName = patchName;
		DisplayPresetStatus();
	}

	RequestPresetEffects();
}

void
AxeFx3Manager::RequestPresetEffects()
{
	if (!mFirmwareMajorVersion)
		SendFirmwareVersionQuery();

	QMutexLocker lock(&mQueryLock);
	if (!mFirmwareMajorVersion)
		return;

	// default to no fx in patch, but don't update LEDs until later
	for (auto & cur : mAxeEffectInfo)
	{
// 		if (-1 == cur.mSysexBypassParameterId)
// 			continue;

		if (-1 == cur.mSysexEffectId)
			continue;

		if (!cur.mPatch)
			continue;

		cur.mEffectIsPresentInAxePatch = false;
	}

	Bytes bb{ 0xF0, 0x00, 0x01, 0x74, byte(mModel), 0x0e };
	AppendChecksumAndTerminate(bb);
	mMidiOut->MidiOut(bb);
}

void
AxeFx3Manager::ReceivePresetEffectsV2(const byte * bytes, int len)
{
	// request: F0 00 01 74 03 0e 08 F7
	// response with compressor 1 bypassed (cc 43 0x2b):
	//			02 56 7C 27 06 F7
	// compressor 1 enabled:
	//			03 56 7C 27 06 F7 
	// compressor 1 enabled: (cc 44 0x2c)
	//			03 58 7C 27 06 F7 
	// comp 1 (cc 43 0x2b):
	// 	56 7C 27 06
	// 	56: 1010110
	// 	2b: 101011
	// comp 1 (cc 44 0x2c)
	// 	58 7C 27 06
	// 	58: 1011000
	// 	2c:	101100
	// comp 1 (none)
	// 	00 7E 27 06
	// comp 1 (pedal)
	// 	02 7E 27 06
	// comp 1 (cc 1 0x01)
	// 	02 7C 27 06
	// 	2: 10
	// 	1: 1
	// comp 1 (cc 127 0x7f)
	// 	7E 7D 27 06
	// 	7e: 1111110
	// 	7f: 1111111

	// amp 1 bypassed (cc 37 0x25):
	//			02 4A 10 53 06 F7 
	// amp 1 in X:
	//			03 4A 10 53 06 F7
	// amp 1 in X  (cc 38 0x26):
	//			03 4C 10 53 06 F7 
	// amp 1 in Y:
	//			01 4A 10 53 06 F7

	// for each effect there are 5 bytes:
	//	1 (whole?) byte for the state: off(y):0 / on(y):1 / off(x)(no xy):2 / on(x)(no xy):3
	//	1 byte: 7 bits for ccLs / 1 bit ?
	//	1 byte: 6 bits ? / 1 bit for cc pedal or cc none / 1 bit for ccMs
	//	1 byte: 5 bits for effectIdLs / 3 bits ?
	//	1 byte: 4 bits ? / 4 bits for effectIdMs
	//	
	for (int idx = 0; (idx + 5) < len; idx += 5)
	{
		if (false && kDbgFlag && mTrace)
		{
			const std::string byteDump(::GetAsciiHexStr(&bytes[idx], 5, true) + "\n");
			mTrace->Trace(byteDump);
		}

		Axe3EffectBlockInfo * inf = nullptr;
		inf = IdentifyBlockInfoUsingCc(bytes + idx + 1);
		if (!inf)
		{
			inf = IdentifyBlockInfoUsingEffectId(bytes + idx + 1);
			if (inf && mTrace && inf->mNormalizedName != "feedback return")
			{
				std::strstream traceMsg;
				traceMsg << "Axe sync warning: potentially unexpected sync for  " << inf->mName << " " << std::endl << std::ends;
				mTrace->Trace(std::string(traceMsg.str()));
			}
		}

		if (inf && inf->mPatch)
		{
			inf->mEffectIsPresentInAxePatch = true;
			const byte kParamVal = bytes[idx];
			const bool isActive = (3 == kParamVal); // enabled X or no xy
			const bool isActiveY = (1 == kParamVal);
			const bool isBypassed = (2 == kParamVal); // disabled X or no xy
			const bool isBypassedY = (0 == kParamVal);

			if (isBypassed || isBypassedY || isActive || isActiveY)
			{
				inf->mPatch->UpdateState(mSwitchDisplay, !(isBypassed || isBypassedY));

// 				if (inf->mXyPatch)
// 				{
// 					// X is the active state, Y is inactive
// 					const bool isX = isActive || isBypassed;
// 					const bool isY = isActiveY || isBypassedY;
// 					_ASSERTE(isX ^ isY);
// 					// Axe-FxII considers X active and Y inactive, but I prefer
// 					// LED off for X and on for Y.  Original behavior below was to
// 					// use isX instead of isY.  See AxeTogglePatch::AxeTogglePatch
// 					// for the other change required for LED inversion of X/Y.
// 					inf->mXyPatch->UpdateState(mSwitchDisplay, isY);
// 				}
			}
			else if (mTrace)
			{
				const std::string byteDump(::GetAsciiHexStr(bytes + idx, 5, true));
				std::strstream traceMsg;
				traceMsg << "Unrecognized bypass param value for " << inf->mName << " " << byteDump.c_str() << std::endl << std::ends;
				mTrace->Trace(std::string(traceMsg.str()));
			}
		}
		else
		{
			if (mTrace && !inf)
			{
				const std::string msg("Axe sync error: No inf for ");
				mTrace->Trace(msg);
				const std::string byteDump(::GetAsciiHexStr(&bytes[idx], 5, true) + "\n");
				mTrace->Trace(byteDump);
			}
		}
	}

	TurnOffLedsForNaEffects();
}

void
AxeFx3Manager::TurnOffLedsForNaEffects()
{
	// turn off LEDs for all effects not in cur preset
	for (auto & cur : mAxeEffectInfo)
	{
// 		if (-1 == cur.mSysexBypassParameterId)
// 			continue;

		if (-1 == cur.mSysexEffectId)
			continue;

		if (!cur.mPatch)
			continue;

		if (!cur.mEffectIsPresentInAxePatch)
		{
			cur.mPatch->Disable(mSwitchDisplay);
			for (const auto &p : cur.mDependentPatches)
				p->Disable(mSwitchDisplay);
		}
	}
}

void
AxeFx3Manager::ReceivePresetNumber(const byte * bytes, int len)
{
	if (len < 3)
		return;

	mCurrentAxePresetName.clear();
	mCurrentAxeSceneName.clear();
	// 0xdd Preset Number bits 6-0
	// 0xdd Preset Number bits 13-7
	const int presetMs = bytes[0] << 7;
	const int presetLs = bytes[1];
	mCurrentAxePreset = presetMs | presetLs;
}

void
AxeFx3Manager::EnableLooperStatusMonitor(bool enable)
{
	// http://forum.fractalaudio.com/other-midi-controllers/39161-using-sysex-recall-present-effect-bypass-status-info-available.html#post737573
	// enable status monitor
	//		F0 00 01 74 03 23 01 24 F7
	// disable status
	//		F0 00 01 74 03 23 00 25 F7
	QMutexLocker lock(&mQueryLock);
	const byte rawBytes[] = 
	{ 0xF0, 0x00, 0x01, 0x74, byte(mModel), 0x23, byte(enable ? 1 : 0), byte(enable ? 0x24 : 0x25), 0xF7 };
	const Bytes bb(rawBytes, rawBytes + sizeof(rawBytes));
	mMidiOut->MidiOut(bb);
}

enum AxeFxLooperState
{ 
	// independent states
	loopStateStopped	= 0, 
	loopStateRecord		= 1, 
	loopStatePlay		= 2,

	// state modifiers
	loopStatePlayOnce	= 1 << 2, 
	loopStateOverdub	= 1 << 3, 
	loopStateReverse	= 1 << 4, 
	loopStateHalfSpeed	= 1 << 5, 
	loopStateUndo		= 1 << 6 
};

static std::string
GetLooperStateDesc(int loopState)
{
	std::string stateStr;
	if (loopState & loopStateRecord)
		stateStr.append("recording");
	else if (loopState & loopStatePlay)
		stateStr.append("playing");
	else
		stateStr.append("stopped");

	if (loopState & loopStatePlayOnce)
		stateStr.append(", once");
	if (loopState & loopStateOverdub)
		stateStr.append(", overdub");
	if (loopState & loopStateUndo)
		stateStr.append(", undo");
	if (loopState & loopStateReverse)
		stateStr.append(", reverse");
	if (loopState & loopStateHalfSpeed)
		stateStr.append(", 1/2 speed");

	return stateStr;
}

void
AxeFx3Manager::ReceiveLooperStatus(const byte * bytes, int len)
{
	// http://forum.fractalaudio.com/other-midi-controllers/39161-using-sysex-recall-present-effect-bypass-status-info-available.html#post737573
	if (len < 4)
		return;

	const int newLoopState = bytes[0];
	if (mLooperState == newLoopState)
		return;

	if (((mLooperState & loopStateRecord) != (newLoopState & loopStateRecord)) && mLooperPatches[loopPatchRecord])
	{
		if ((mLooperState & loopStateRecord) && !(newLoopState & loopStateRecord))
			mLooperPatches[loopPatchRecord]->UpdateState(mSwitchDisplay, false);
		else
			mLooperPatches[loopPatchRecord]->UpdateState(mSwitchDisplay, true);
	}

	if (((mLooperState & loopStatePlay) != (newLoopState & loopStatePlay)) && mLooperPatches[loopPatchPlay])
	{
		if ((mLooperState & loopStatePlay) && !(newLoopState & loopStatePlay))
			mLooperPatches[loopPatchPlay]->UpdateState(mSwitchDisplay, false);
		else
			mLooperPatches[loopPatchPlay]->UpdateState(mSwitchDisplay, true);
	}

	if (((mLooperState & loopStatePlayOnce) != (newLoopState & loopStatePlayOnce)) && mLooperPatches[loopPatchPlayOnce])
	{
		if ((mLooperState & loopStatePlayOnce) && !(newLoopState & loopStatePlayOnce))
			mLooperPatches[loopPatchPlayOnce]->UpdateState(mSwitchDisplay, false);
		else
			mLooperPatches[loopPatchPlayOnce]->UpdateState(mSwitchDisplay, true);
	}

	if (((mLooperState & loopStateUndo) != (newLoopState & loopStateUndo)) && mLooperPatches[loopPatchUndo])
	{
		if ((mLooperState & loopStateUndo) && !(newLoopState & loopStateUndo))
			mLooperPatches[loopPatchUndo]->UpdateState(mSwitchDisplay, false);
		else
			mLooperPatches[loopPatchUndo]->UpdateState(mSwitchDisplay, true);
	}

	if (((mLooperState & loopStateOverdub) != (newLoopState & loopStateOverdub)) && mLooperPatches[loopPatchOverdub])
	{
		if ((mLooperState & loopStateOverdub) && !(newLoopState & loopStateOverdub))
			mLooperPatches[loopPatchOverdub]->UpdateState(mSwitchDisplay, false);
		else
			mLooperPatches[loopPatchOverdub]->UpdateState(mSwitchDisplay, true);
	}

	if (((mLooperState & loopStateReverse) != (newLoopState & loopStateReverse)) && mLooperPatches[loopPatchReverse])
	{
		if ((mLooperState & loopStateReverse) && !(newLoopState & loopStateReverse))
			mLooperPatches[loopPatchReverse]->UpdateState(mSwitchDisplay, false);
		else
			mLooperPatches[loopPatchReverse]->UpdateState(mSwitchDisplay, true);
	}

	if (((mLooperState & loopStateHalfSpeed) != (newLoopState & loopStateHalfSpeed)) && mLooperPatches[loopPatchHalf])
	{
		if ((mLooperState & loopStateHalfSpeed) && !(newLoopState & loopStateHalfSpeed))
			mLooperPatches[loopPatchHalf]->UpdateState(mSwitchDisplay, false);
		else
			mLooperPatches[loopPatchHalf]->UpdateState(mSwitchDisplay, true);
	}

	mLooperState = newLoopState;

	if (mMainDisplay)
	{
		std::strstream traceMsg;
		traceMsg << "Axe-Fx looper: " << GetLooperStateDesc(mLooperState) << std::endl << std::ends;
		mMainDisplay->TransientTextOut(std::string(traceMsg.str()));
	}
}

void
AxeFx3Manager::SetLooperPatch(PatchPtr patch)
{
	std::string name(patch->GetName());
	::NormalizeAxe3EffectName(name);
	LoopPatchIdx idx = loopPatchCnt;
	if (-1 != name.find("looper record"))
		idx = loopPatchRecord;
	else if (-1 != name.find("looper play"))
		idx = loopPatchPlay;
	else if (-1 != name.find("looper once"))
		idx = loopPatchPlayOnce;
	else if (-1 != name.find("looper undo"))
		idx = loopPatchUndo;
	else if (-1 != name.find("looper overdub"))
		idx = loopPatchOverdub;
	else if (-1 != name.find("looper reverse"))
		idx = loopPatchReverse;
	else if (-1 != name.find("looper half"))
		idx = loopPatchHalf;
	else
		return;

	if (mLooperPatches[idx] && mTrace)
	{
		std::string msg("Warning: multiple Axe-Fx patches for same looper command\n");
		mTrace->Trace(msg);
	}

	mLooperPatches[idx] = patch;
}

void
AxeFx3Manager::ReceiveSceneStatus(const byte * bytes, int len)
{
	if (len < 1)
		return;

	int newScene = bytes[0];
	if (mCurrentScene == newScene)
		return;

	if (newScene >= AxeScenes)
	{
		_ASSERTE(newScene < AxeScenes);
		if (mTrace)
		{
			std::string msg("Warning: invalid scene number received\n");
			mTrace->Trace(msg);
		}
		return;
	}

	if (mCurrentScene > -1 && mScenes[mCurrentScene])
		mScenes[mCurrentScene]->UpdateState(mSwitchDisplay, false);

	mCurrentScene = newScene;

	if (mScenes[mCurrentScene])
		mScenes[mCurrentScene]->UpdateState(mSwitchDisplay, true);

	if (mCurrentAxePresetName.empty())
		RequestPresetName();
	else
		DisplayPresetStatus();
}

void
AxeFx3Manager::DisplayPresetStatus()
{
	if (!mMainDisplay)
		return;

	const std::string kPrefix("Axe-Fx preset: ");
	std::string curText;//(mMainDisplay->GetCurrentText());
// 	int pos = curText.rfind(kPrefix);
// 	if (std::string::npos != pos)
// 		curText = curText.substr(0, pos);
	
	if (mCurrentAxePreset > -1 && mCurrentAxePreset < 1000)
	{
		char presetBuf[4];
		::_itoa_s(mCurrentAxePreset + 1, presetBuf, 10);
		if (mCurrentAxePresetName.empty())
			curText += kPrefix + presetBuf;
		else
			curText += kPrefix + presetBuf + " " + mCurrentAxePresetName;
	}
	else if (!mCurrentAxePresetName.empty())
		curText += kPrefix + mCurrentAxePresetName;

	if (-1 != mCurrentScene)
	{
		char sceneBuf[4];
		::_itoa_s(mCurrentScene + 1, sceneBuf, 10);
		curText += std::string("\nAxe-Fx scene: ") + sceneBuf;
		if (!mCurrentAxeSceneName.empty())
			curText += std::string(" ") + mCurrentAxeSceneName;
	}

	if (curText.length())
		mMainDisplay->TextOut(curText);
}

void
AxeFx3Manager::AppendChecksumAndTerminate(Bytes &data)
{
	byte chkSum = 0;
	for (const auto &b : data)
		chkSum ^= b;
	data.push_back(chkSum & 0x7F);
	data.push_back(0xF7);
}

void
AxeFx3Manager::LoadEffectPool()
{
	mAxeEffectInfo = {
		{ FractalAudio::AxeFx3::ID_TUNER, "Tuner" },
		{ FractalAudio::AxeFx3::ID_IRCAPTURE, "" },
		{ FractalAudio::AxeFx3::ID_INPUT1, "Input 1" },
		{ FractalAudio::AxeFx3::ID_INPUT2, "Input 2" },
		{ FractalAudio::AxeFx3::ID_INPUT3, "Input 3" },
		{ FractalAudio::AxeFx3::ID_INPUT4, "Input 4" },
		{ FractalAudio::AxeFx3::ID_INPUT5, "Input 5" },
		{ FractalAudio::AxeFx3::ID_OUTPUT1, "Output 1" },
		{ FractalAudio::AxeFx3::ID_OUTPUT2, "Output 2" },
		{ FractalAudio::AxeFx3::ID_OUTPUT3, "Output 3" },
		{ FractalAudio::AxeFx3::ID_OUTPUT4, "Output 4" },
		{ FractalAudio::AxeFx3::ID_COMP1, "Compressor 1" },
		{ FractalAudio::AxeFx3::ID_COMP2, "Compressor 2" },
		{ FractalAudio::AxeFx3::ID_COMP3, "Compressor 3" },
		{ FractalAudio::AxeFx3::ID_COMP4, "Compressor 4" },
		{ FractalAudio::AxeFx3::ID_GRAPHEQ1, "Graphic EQ 1" },
		{ FractalAudio::AxeFx3::ID_GRAPHEQ2, "Graphic EQ 2" },
		{ FractalAudio::AxeFx3::ID_GRAPHEQ3, "Graphic EQ 3" },
		{ FractalAudio::AxeFx3::ID_GRAPHEQ4, "Graphic EQ 4" },
		{ FractalAudio::AxeFx3::ID_PARAEQ1, "Parametric EQ 1" },
		{ FractalAudio::AxeFx3::ID_PARAEQ2, "Parametric EQ 2" },
		{ FractalAudio::AxeFx3::ID_PARAEQ3, "Parametric EQ 3" },
		{ FractalAudio::AxeFx3::ID_PARAEQ4, "Parametric EQ 4" },
		{ FractalAudio::AxeFx3::ID_DISTORT1, "Amp 1" },
		{ FractalAudio::AxeFx3::ID_DISTORT2, "Amp 2" },
		{ FractalAudio::AxeFx3::ID_DISTORT3, "Amp 3" },
		{ FractalAudio::AxeFx3::ID_DISTORT4, "Amp 4" },
		{ FractalAudio::AxeFx3::ID_CAB1, "Cabinet 1" },
		{ FractalAudio::AxeFx3::ID_CAB2, "Cabinet 2" },
		{ FractalAudio::AxeFx3::ID_CAB3, "Cabinet 3" },
		{ FractalAudio::AxeFx3::ID_CAB4, "Cabinet 4" },
		{ FractalAudio::AxeFx3::ID_REVERB1, "Reverb 1" },
		{ FractalAudio::AxeFx3::ID_REVERB2, "Reverb 2" },
		{ FractalAudio::AxeFx3::ID_REVERB3, "Reverb 3" },
		{ FractalAudio::AxeFx3::ID_REVERB4, "Reverb 4" },
		{ FractalAudio::AxeFx3::ID_DELAY1, "Delay 1" },
		{ FractalAudio::AxeFx3::ID_DELAY2, "Delay 2" },
		{ FractalAudio::AxeFx3::ID_DELAY3, "Delay 3" },
		{ FractalAudio::AxeFx3::ID_DELAY4, "Delay 4" },
		{ FractalAudio::AxeFx3::ID_MULTITAP1, "Multidelay 1" },
		{ FractalAudio::AxeFx3::ID_MULTITAP2, "Multidelay 2" },
		{ FractalAudio::AxeFx3::ID_MULTITAP3, "Multidelay 3" },
		{ FractalAudio::AxeFx3::ID_MULTITAP4, "Multidelay 4" },
		{ FractalAudio::AxeFx3::ID_CHORUS1, "Chorus 1" },
		{ FractalAudio::AxeFx3::ID_CHORUS2, "Chorus 2" },
		{ FractalAudio::AxeFx3::ID_CHORUS3, "Chorus 3" },
		{ FractalAudio::AxeFx3::ID_CHORUS4, "Chorus 4" },
		{ FractalAudio::AxeFx3::ID_FLANGER1, "Flanger 1" },
		{ FractalAudio::AxeFx3::ID_FLANGER2, "Flanger 2" },
		{ FractalAudio::AxeFx3::ID_FLANGER3, "Flanger 3" },
		{ FractalAudio::AxeFx3::ID_FLANGER4, "Flanger 4" },
		{ FractalAudio::AxeFx3::ID_ROTARY1, "Rotary 1" },
		{ FractalAudio::AxeFx3::ID_ROTARY2, "Rotary 2" },
		{ FractalAudio::AxeFx3::ID_ROTARY3, "Rotary 3" },
		{ FractalAudio::AxeFx3::ID_ROTARY4, "Rotary 4" },
		{ FractalAudio::AxeFx3::ID_PHASER1, "Phaser 1" },
		{ FractalAudio::AxeFx3::ID_PHASER2, "Phaser 2" },
		{ FractalAudio::AxeFx3::ID_PHASER3, "Phaser 3" },
		{ FractalAudio::AxeFx3::ID_PHASER4, "Phaser 4" },
		{ FractalAudio::AxeFx3::ID_WAH1, "Wah-wah 1" },
		{ FractalAudio::AxeFx3::ID_WAH2, "Wah-wah 2" },
		{ FractalAudio::AxeFx3::ID_WAH3, "Wah-wah 3" },
		{ FractalAudio::AxeFx3::ID_WAH4, "Wah-wah 4" },
		{ FractalAudio::AxeFx3::ID_FORMANT1, "Formant 1" },
		{ FractalAudio::AxeFx3::ID_FORMANT2, "Formant 2" },
		{ FractalAudio::AxeFx3::ID_FORMANT3, "Formant 3" },
		{ FractalAudio::AxeFx3::ID_FORMANT4, "Formant 4" },
		{ FractalAudio::AxeFx3::ID_VOLUME1, "Vol/Pan 1" },
		{ FractalAudio::AxeFx3::ID_VOLUME2, "Vol/Pan 2" },
		{ FractalAudio::AxeFx3::ID_VOLUME3, "Vol/Pan 3" },
		{ FractalAudio::AxeFx3::ID_VOLUME4, "Vol/Pan 4" },
		{ FractalAudio::AxeFx3::ID_TREMOLO1, "Tremolo 1" },
		{ FractalAudio::AxeFx3::ID_TREMOLO2, "Tremolo 2" },
		{ FractalAudio::AxeFx3::ID_TREMOLO3, "Tremolo 3" },
		{ FractalAudio::AxeFx3::ID_TREMOLO4, "Tremolo 4" },
		{ FractalAudio::AxeFx3::ID_PITCH1, "Pitch Shift 1" },
		{ FractalAudio::AxeFx3::ID_PITCH2, "Pitch Shift 2" },
		{ FractalAudio::AxeFx3::ID_PITCH3, "Pitch Shift 3" },
		{ FractalAudio::AxeFx3::ID_PITCH4, "Pitch Shift 4" },
		{ FractalAudio::AxeFx3::ID_FILTER1, "Filter 1" },
		{ FractalAudio::AxeFx3::ID_FILTER2, "Filter 2" },
		{ FractalAudio::AxeFx3::ID_FILTER3, "Filter 3" },
		{ FractalAudio::AxeFx3::ID_FILTER4, "Filter 4" },
		{ FractalAudio::AxeFx3::ID_FUZZ1, "Drive 1" },
		{ FractalAudio::AxeFx3::ID_FUZZ2, "Drive 2" },
		{ FractalAudio::AxeFx3::ID_FUZZ3, "Drive 3" },
		{ FractalAudio::AxeFx3::ID_FUZZ4, "Drive 4" },
		{ FractalAudio::AxeFx3::ID_ENHANCER1, "Enhancer 1" },
		{ FractalAudio::AxeFx3::ID_ENHANCER2, "Enhancer 2" },
		{ FractalAudio::AxeFx3::ID_ENHANCER3, "Enhancer 3" },
		{ FractalAudio::AxeFx3::ID_ENHANCER4, "Enhancer 4" },
		{ FractalAudio::AxeFx3::ID_MIXER1, "Mixer 1" },
		{ FractalAudio::AxeFx3::ID_MIXER2, "Mixer 2" },
		{ FractalAudio::AxeFx3::ID_MIXER3, "Mixer 3" },
		{ FractalAudio::AxeFx3::ID_MIXER4, "Mixer 4" },
		{ FractalAudio::AxeFx3::ID_SYNTH1, "Synth 1" },
		{ FractalAudio::AxeFx3::ID_SYNTH2, "Synth 2" },
		{ FractalAudio::AxeFx3::ID_SYNTH3, "Synth 3" },
		{ FractalAudio::AxeFx3::ID_SYNTH4, "Synth 4" },
		{ FractalAudio::AxeFx3::ID_VOCODER1, "Vocoder 1" },
		{ FractalAudio::AxeFx3::ID_VOCODER2, "Vocoder 2" },
		{ FractalAudio::AxeFx3::ID_VOCODER3, "Vocoder 3" },
		{ FractalAudio::AxeFx3::ID_VOCODER4, "Vocoder 4" },
		{ FractalAudio::AxeFx3::ID_MEGATAP1, "Megatap 1" },
		{ FractalAudio::AxeFx3::ID_MEGATAP2, "Megatap 2" },
		{ FractalAudio::AxeFx3::ID_MEGATAP3, "Megatap 3" },
		{ FractalAudio::AxeFx3::ID_MEGATAP4, "Megatap 4" },
		{ FractalAudio::AxeFx3::ID_CROSSOVER1, "Crossover 1" },
		{ FractalAudio::AxeFx3::ID_CROSSOVER2, "Crossover 2" },
		{ FractalAudio::AxeFx3::ID_CROSSOVER3, "Crossover 3" },
		{ FractalAudio::AxeFx3::ID_CROSSOVER4, "Crossover 4" },
		{ FractalAudio::AxeFx3::ID_GATE1, "Gate/Expander 1" },
		{ FractalAudio::AxeFx3::ID_GATE2, "Gate/Expander 2" },
		{ FractalAudio::AxeFx3::ID_GATE3, "Gate/Expander 3" },
		{ FractalAudio::AxeFx3::ID_GATE4, "Gate/Expander 4" },
		{ FractalAudio::AxeFx3::ID_RINGMOD1, "Ring Mod 1" },
		{ FractalAudio::AxeFx3::ID_RINGMOD2, "Ring Mod 2" },
		{ FractalAudio::AxeFx3::ID_RINGMOD3, "Ring Mod 3" },
		{ FractalAudio::AxeFx3::ID_RINGMOD4, "Ring Mod 4" },
		{ FractalAudio::AxeFx3::ID_MULTICOMP1, "Multiband Compressor 1" },
		{ FractalAudio::AxeFx3::ID_MULTICOMP2, "Multiband Compressor 2" },
		{ FractalAudio::AxeFx3::ID_MULTICOMP3, "Multiband Compressor 3" },
		{ FractalAudio::AxeFx3::ID_MULTICOMP4, "Multiband Compressor 4" },
		{ FractalAudio::AxeFx3::ID_TENTAP1, "Ten-Tap Delay 1" },
		{ FractalAudio::AxeFx3::ID_TENTAP2, "Ten-Tap Delay 2" },
		{ FractalAudio::AxeFx3::ID_TENTAP3, "Ten-Tap Delay 3" },
		{ FractalAudio::AxeFx3::ID_TENTAP4, "Ten-Tap Delay 4" },
		{ FractalAudio::AxeFx3::ID_RESONATOR1, "Resonator 1" },
		{ FractalAudio::AxeFx3::ID_RESONATOR2, "Resonator 2" },
		{ FractalAudio::AxeFx3::ID_RESONATOR3, "Resonator 3" },
		{ FractalAudio::AxeFx3::ID_RESONATOR4, "Resonator 4" },
		{ FractalAudio::AxeFx3::ID_LOOPER1, "Looper 1" },
		{ FractalAudio::AxeFx3::ID_LOOPER2, "Looper 2" },
		{ FractalAudio::AxeFx3::ID_LOOPER3, "Looper 3" },
		{ FractalAudio::AxeFx3::ID_LOOPER4, "Looper 4" },
		{ FractalAudio::AxeFx3::ID_TONEMATCH1, "Tone Match 1" },
		{ FractalAudio::AxeFx3::ID_TONEMATCH2, "Tone Match 2" },
		{ FractalAudio::AxeFx3::ID_TONEMATCH3, "Tone Match 3" },
		{ FractalAudio::AxeFx3::ID_TONEMATCH4, "Tone Match 4" },
		{ FractalAudio::AxeFx3::ID_RTA1, "RTA 1" },
		{ FractalAudio::AxeFx3::ID_RTA2, "RTA 2" },
		{ FractalAudio::AxeFx3::ID_RTA3, "RTA 3" },
		{ FractalAudio::AxeFx3::ID_RTA4, "RTA 4" },
		{ FractalAudio::AxeFx3::ID_PLEX1, "Plex Delay 1" },
		{ FractalAudio::AxeFx3::ID_PLEX2, "Plex Delay 2" },
		{ FractalAudio::AxeFx3::ID_PLEX3, "Plex Delay 3" },
		{ FractalAudio::AxeFx3::ID_PLEX4, "Plex Delay 4" },
		{ FractalAudio::AxeFx3::ID_FBSEND1, "Send 1" },
		{ FractalAudio::AxeFx3::ID_FBSEND2, "Send 2" },
		{ FractalAudio::AxeFx3::ID_FBSEND3, "Send 3" },
		{ FractalAudio::AxeFx3::ID_FBSEND4, "Send 4" },
		{ FractalAudio::AxeFx3::ID_FBRETURN1, "Return 1" },
		{ FractalAudio::AxeFx3::ID_FBRETURN2, "Return 2" },
		{ FractalAudio::AxeFx3::ID_FBRETURN3, "Return 3" },
		{ FractalAudio::AxeFx3::ID_FBRETURN4, "Return 4" },
		{ FractalAudio::AxeFx3::ID_MIDIBLOCK, "Scene MIDI Block" },
		{ FractalAudio::AxeFx3::ID_MULTIPLEXER1, "Multiplexer 1" }, 
		{ FractalAudio::AxeFx3::ID_MULTIPLEXER2, "Multiplexer 2" },
		{ FractalAudio::AxeFx3::ID_MULTIPLEXER3, "Multiplexer 3" },
		{ FractalAudio::AxeFx3::ID_MULTIPLEXER4, "Multiplexer 4" },
		{ FractalAudio::AxeFx3::ID_IRPLAYER1, "IR Player 1" },
		{ FractalAudio::AxeFx3::ID_IRPLAYER2, "IR Player 2" },
		{ FractalAudio::AxeFx3::ID_IRPLAYER3, "IR Player 3" },
		{ FractalAudio::AxeFx3::ID_IRPLAYER4, "IR Player 4" },
		{ FractalAudio::AxeFx3::ID_FOOTCONTROLLER, "" },
		{ FractalAudio::AxeFx3::ID_PRESET_FC, "" }
	};
}

void
NormalizeAxe3EffectName(std::string &effectName)
{
	std::transform(effectName.begin(), effectName.end(), effectName.begin(), ::tolower);

	int pos = effectName.find("axe");
	if (-1 != pos)
	{
		std::string searchStr;

		searchStr = "axe ";
		pos = effectName.find(searchStr);
		if (-1 == pos)
		{
			searchStr = "axefx ";
			pos = effectName.find(searchStr);
		}
		if (-1 == pos)
		{
			searchStr = "axe-fx ";
			pos = effectName.find(searchStr);
		}

		if (-1 != pos)
			effectName = effectName.substr(pos + searchStr.length());
	}

	Axe3SynonymNormalization(effectName);
}

void
Axe3SynonymNormalization(std::string & name)
{
#define MapName(subst, legit) if (name == subst) { name = legit; return; }

	int pos = name.find('(');
	if (std::string::npos != pos)
	{
		name = name.substr(0, pos);
		while (name.at(name.length() - 1) == ' ')
			name = name.substr(0, name.length() - 1);
	}

// 	pos = name.find(" xy");
// 	if (std::string::npos != pos)
// 		name.replace(pos, 3, " x/y");

	switch (name[0])
	{
	case 'a':
		MapName("amp match", "tone match");
		MapName("arpeggiator 1", "pitch shift 1");
		MapName("arpeggiator 2", "pitch shift 2");
		MapName("arpeggiator 3", "pitch shift 3");
		MapName("arpeggiator 4", "pitch shift 4");
		break;
	case 'b':
		MapName("band delay 1", "multidelay 1");
		MapName("band delay 2", "multidelay 2");
		MapName("band delay 3", "multidelay 3");
		MapName("band delay 4", "multidelay 4");
		MapName("band dly 1", "multidelay 1");
		MapName("band dly 2", "multidelay 2");
		MapName("band dly 3", "multidelay 3");
		MapName("band dly 4", "multidelay 4");
		break;
	case 'c':
		MapName("cab 1", "cabinet 1");
		MapName("cab 2", "cabinet 2");
		MapName("cab 3", "cabinet 3");
		MapName("cab 4", "cabinet 4");
		MapName("comp 1", "compressor 1");
		MapName("comp 2", "compressor 2");
		MapName("comp 3", "compressor 3");
		MapName("comp 4", "compressor 4");
		MapName("crystals 1", "pitch shift 1");
		MapName("crystals 2", "pitch shift 2");
		MapName("crystals 3", "pitch shift 3");
		MapName("crystals 4", "pitch shift 4");
		break;
	case 'd':
		MapName("dec scene", "scene decrement");
		MapName("decrement scene", "scene decrement");
		MapName("delay 1 (reverse)", "delay 1");
		MapName("delay 2 (reverse)", "delay 2");
		MapName("delay 3 (reverse)", "delay 3");
		MapName("delay 4 (reverse)", "delay 4");
		MapName("detune 1", "pitch shift 1");
		MapName("detune 2", "pitch shift 2");
		MapName("detune 3", "pitch shift 3");
		MapName("detune 4", "pitch shift 4");
		MapName("diff 1", "multidelay 1");
		MapName("diff 2", "multidelay 2");
		MapName("diff 3", "multidelay 3");
		MapName("diff 4", "multidelay 4");
		MapName("diffuser 1", "multidelay 1");
		MapName("diffuser 2", "multidelay 2");
		MapName("diffuser 3", "multidelay 3");
		MapName("diffuser 4", "multidelay 4");
		MapName("dly 1", "delay 1");
		MapName("dly 2", "delay 2");
		MapName("dly 3", "delay 3");
		MapName("dly 4", "delay 4");
		break;
	case 'e':
		MapName("enhancer", "enhancer 1");
		MapName("eq match", "tone match");
		MapName("eqmatch", "tone match");
		MapName("expander 1", "gate/expander 1");
		MapName("expander 2", "gate/expander 2");
		MapName("expander 3", "gate/expander 3");
		MapName("expander 4", "gate/expander 4");
		MapName("ext 1", "external 1");
		MapName("ext 2", "external 2");
		MapName("ext 3", "external 3");
		MapName("ext 4", "external 4");
		MapName("ext 5", "external 5");
		MapName("ext 6", "external 6");
		MapName("ext 7", "external 7");
		MapName("ext 8", "external 8");
		MapName("ext 9", "external 9");
		MapName("ext 10", "external 10");
		MapName("ext 11", "external 11");
		MapName("ext 12", "external 12");
		MapName("extern 1", "external 1");
		MapName("extern 2", "external 2");
		MapName("extern 3", "external 3");
		MapName("extern 4", "external 4");
		MapName("extern 5", "external 5");
		MapName("extern 6", "external 6");
		MapName("extern 7", "external 7");
		MapName("extern 8", "external 8");
		MapName("extern 9", "external 9");
		MapName("extern 10", "external 10");
		MapName("extern 11", "external 11");
		MapName("extern 12", "external 12");
		break;
	case 'f':
		MapName("fb ret", "return 1");
		MapName("fb rtrn", "return 1");
		MapName("fb return", "return 1");
		MapName("fdbk ret", "return 1");
		MapName("fdbk rtrn", "return 1");
		MapName("fdbk return", "return 1");
		MapName("feed ret", "return 1");
		MapName("feed rtrn", "return 1");
		MapName("feed return", "return 1");
		MapName("feedback ret", "return 1");
		MapName("feedback rtrn", "return 1");
		MapName("fb send", "send 1");
		MapName("fdbk send", "send 1");
		MapName("feedsend", "send 1");
		MapName("feed send", "send 1");
		MapName("formant", "formant 1");
		MapName("fuzz 1", "drive 1");
		MapName("fuzz 2", "drive 2");
		MapName("fuzz 3", "drive 3");
		MapName("fuzz 4", "drive 4");
		break;
	case 'g':
		MapName("gate 1", "gate/expander 1");
		MapName("gate 2", "gate/expander 2");
		MapName("gate 3", "gate/expander 3");
		MapName("gate 4", "gate/expander 4");
		MapName("graphiceq 1", "graphic eq 1");
		MapName("graphiceq 2", "graphic eq 2");
		MapName("graphiceq 3", "graphic eq 3");
		MapName("graphiceq 4", "graphic eq 4");
		break;
	case 'h':
		MapName("half-speed", "looper half");
		break;
	case 'i':
		MapName("in vol", "input volume");
		MapName("inc scene", "scene increment");
		MapName("increment scene", "scene increment");
		MapName("input", "input volume");
		MapName("input vol", "input volume");
		break;
	case 'l':
		MapName("loop 1 rec", "looper 1 record");
		MapName("loop 1 record", "looper 1 record");
		MapName("loop 1 play", "looper 1 play");
		MapName("loop 1 once", "looper 1 once");
		MapName("loop 1 overdub", "looper 1 overdub");
		MapName("loop 1 rev", "looper 1 reverse");
		MapName("loop 1 reverse", "looper 1 reverse");
		MapName("loop half-speed", "looper half");
		MapName("loop half speed", "looper half");
		MapName("loop metronome", "looper metronome");
		MapName("loop rec", "looper record");
		MapName("loop record", "looper record");
		MapName("loop play", "looper play");
		MapName("loop play once", "looper once");
		MapName("loop once", "looper once");
		MapName("loop overdub", "looper overdub");
		MapName("loop rev", "looper reverse");
		MapName("loop reverse", "looper reverse");
		MapName("loop undo", "looper undo");
		MapName("looper", "looper 1");
		MapName("looper half-speed", "looper half");
		MapName("looper half speed", "looper half");
		break;
	case 'm':
		MapName("mdly 1", "multidelay 1");
		MapName("mdly 2", "multidelay 2");
		MapName("mdly 3", "multidelay 3");
		MapName("mdly 4", "multidelay 4");
		MapName("megatap", "megatap 1");
		MapName("megatap delay", "megatap 1");
		MapName("megatap delay 1", "megatap 1");
		MapName("megatap delay 2", "megatap 2");
		MapName("megatap delay 3", "megatap 3");
		MapName("megatap delay 4", "megatap 4");
		MapName("metronome", "looper metronome");
		MapName("midi block", "scene midi block");
		MapName("mix 1", "mixer 1");
		MapName("mix 2", "mixer 2");
		MapName("mix 3", "mixer 3");
		MapName("mix 4", "mixer 4");
		MapName("multi comp 1", "multiband compressor 1");
		MapName("multi comp 2", "multiband compressor 2");
		MapName("multi comp 3", "multiband compressor 3");
		MapName("multi comp 4", "multiband compressor 4");
		MapName("multiband comp 1", "multiband compressor 1");
		MapName("multiband comp 2", "multiband compressor 2");
		MapName("multiband comp 3", "multiband compressor 3");
		MapName("multiband comp 4", "multiband compressor 4");
		MapName("multi delay 1", "multidelay 1");
		MapName("multi delay 2", "multidelay 2");
		MapName("multi delay 3", "multidelay 3");
		MapName("multi delay 4", "multidelay 4");
		break;
	case 'n':
		MapName("next scene", "scene increment");
// 		MapName("noise gate 1", "gate/expander 1");
// 		MapName("noise gate 2", "gate/expander 2");
// 		MapName("noise gate 3", "gate/expander 3");
// 		MapName("noise gate 4", "gate/expander 4");
		break;
	case 'o':
		MapName("octave 1", "pitch shift 1");
		MapName("octave 2", "pitch shift 2");
		MapName("octave 3", "pitch shift 3");
		MapName("octave 4", "pitch shift 4");
// 		MapName("out", "output");
		MapName("out 1 vol", "out 1 volume");
		MapName("out 2 vol", "out 2 volume");
		MapName("out 3 vol", "out 3 volume");
		MapName("out 4 vol", "out 4 volume");
		MapName("output 1 vol", "out 1 volume");
		MapName("output 2 vol", "out 2 volume");
		MapName("output 3 vol", "out 3 volume");
		MapName("output 4 vol", "out 4 volume");
		break;
	case 'p':
		MapName("para eq 1", "parametric eq 1");
		MapName("para eq 2", "parametric eq 2");
		MapName("para eq 3", "parametric eq 3");
		MapName("para eq 4", "parametric eq 4");
		MapName("pitch 1", "pitch shift 1");
		MapName("pitch 2", "pitch shift 2");
		MapName("pitch 3", "pitch shift 3");
		MapName("pitch 4", "pitch shift 4");
		MapName("pitch 1 (whammy)", "pitch shift 1");
		MapName("pitch 2 (whammy)", "pitch shift 2");
		MapName("pitch 3 (whammy)", "pitch shift 3");
		MapName("pitch 4 (whammy)", "pitch shift 4");
		MapName("plex 1", "plex delay 1");
		MapName("plex 2", "plex delay 2");
		MapName("plex 3", "plex delay 3");
		MapName("plex 4", "plex delay 4");
		MapName("plex dly 1", "plex delay 1");
		MapName("plex dly 2", "plex delay 2");
		MapName("plex dly 3", "plex delay 3");
		MapName("plex dly 4", "plex delay 4");
		MapName("previous scene", "scene decrement");
		break;
	case 'r':
		MapName("return", "return 1");
		MapName("reverse 1", "delay 1");
		MapName("reverse 2", "delay 2");
		MapName("reverse 3", "delay 3");
		MapName("reverse 4", "delay 4");
		MapName("reverse delay 1", "delay 1");
		MapName("reverse delay 2", "delay 2");
		MapName("reverse delay 3", "delay 3");
		MapName("reverse delay 4", "delay 4");
		MapName("reverse dly 1", "delay 1");
		MapName("reverse dly 2", "delay 2");
		MapName("reverse dly 3", "delay 3");
		MapName("reverse dly 4", "delay 4");
		MapName("rhythm tap 1", "multidelay 1");
		MapName("rhythm tap 2", "multidelay 2");
		MapName("rhythm tap 3", "multidelay 3");
		MapName("rhythm tap 4", "multidelay 4");
		MapName("ring modulator", "ring mod 1");
		MapName("ring modulator 1", "ring mod 1");
		MapName("ring modulator 2", "ring mod 2");
		MapName("ring modulator 3", "ring mod 3");
		MapName("ring modulator 4", "ring mod 4");
		MapName("ringmod", "ring mod 1");
		MapName("ring mod", "ring mod 1");
		MapName("ringmod 1", "ring mod 1");
		MapName("ringmod 2", "ring mod 2");
		MapName("ringmod 3", "ring mod 3");
		MapName("ringmod 4", "ring mod 4");
		MapName("rta", "rta 1");
		break;
	case 's':
		MapName("send", "send 1");
		MapName("scene inc", "scene increment");
		MapName("scene dec", "scene decrement");
		MapName("scene midi", "scene midi block");
		MapName("shift 1", "pitch shift 1");
		MapName("shift 2", "pitch shift 2");
		MapName("shift 3", "pitch shift 3");
		MapName("shift 4", "pitch shift 4");
		break;
	case 't':
		MapName("tap", "taptempo");
		MapName("tap tempo", "taptempo");
		MapName("ten tap 1", "ten-tap delay 1");
		MapName("ten tap 2", "ten-tap delay 2");
		MapName("ten tap 3", "ten-tap delay 3");
		MapName("ten tap 4", "ten-tap delay 4");
		MapName("ten tap delay 1", "ten-tap delay 1");
		MapName("ten tap delay 2", "ten-tap delay 2");
		MapName("ten tap delay 3", "ten-tap delay 3");
		MapName("ten tap delay 4", "ten-tap delay 4");
		MapName("tone", "tone match 1");
		MapName("tonematch", "tone match 1");
		MapName("tone match", "tone match 1");
		MapName("trem 1", "tremolo 1");
		MapName("trem 2", "tremolo 2");
		MapName("trem 3", "tremolo 3");
		MapName("trem 4", "tremolo 4");
		break;
	case 'u':
		MapName("usb", "input 5");
		MapName("usb input", "input 5");
		break;
	case 'v':
		MapName("vocoder", "vocoder 1");
		MapName("vol dec", "volume decrement");
		MapName("vol decrement", "volume decrement");
		MapName("vol inc", "volume increment");
		MapName("vol increment", "volume increment");
		MapName("vol 1", "vol/pan 1");
		MapName("vol 2", "vol/pan 2");
		MapName("vol 3", "vol/pan 3");
		MapName("vol 4", "vol/pan 4");
		MapName("volume 1", "vol/pan 1");
		MapName("volume 2", "vol/pan 2");
		MapName("volume 3", "vol/pan 3");
		MapName("volume 4", "vol/pan 4");
		MapName("volume/pan 1", "vol/pan 1");
		MapName("volume/pan 2", "vol/pan 2");
		MapName("volume/pan 3", "vol/pan 3");
		MapName("volume/pan 4", "vol/pan 4");
		break;
	case 'w':
		MapName("wah 1", "wah-wah 1");
		MapName("wah 2", "wah-wah 2");
		MapName("wah 3", "wah-wah 3");
		MapName("wah 4", "wah-wah 4");
		MapName("wahwah 1", "wah-wah 1");
		MapName("wahwah 2", "wah-wah 2");
		MapName("wahwah 3", "wah-wah 3");
		MapName("wahwah 4", "wah-wah 4");
		MapName("whammy 1", "pitch shift 1");
		MapName("whammy 2", "pitch shift 2");
		MapName("whammy 3", "pitch shift 3");
		MapName("whammy 4", "pitch shift 4");
		break;
	case 'x':
		MapName("x-over 1", "crossover 1");
		MapName("x-over 2", "crossover 2");
		MapName("x-over 3", "crossover 3");
		MapName("x-over 4", "crossover 4");
		break;
	}
}