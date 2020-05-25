/*
 * mTroll MIDI Controller
 * Copyright (C) 2010-2015,2018,2020 Sean Echevarria
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


#ifdef _DEBUG
static const int kDbgFlag = 1;
#else
static const int kDbgFlag = 0;
#endif

static void SynonymNormalization(std::string & name);

static const int kDefaultNameSyncTimerInterval = 100;
static const int kDefaultEffectsSyncTimerInterval = 20;
#ifdef ITEM_COUNTING
std::atomic<int> gAxeFx3MgrCnt = 0;
#endif

AxeFx3Manager::AxeFx3Manager(IMainDisplay * mainDisp, 
						   ISwitchDisplay * switchDisp,
						   ITraceDisplay *pTrace,
						   const std::string & appPath, 
						   int ch, 
						   AxeFxModel mod) :
	mSwitchDisplay(switchDisp),
	mMainDisplay(mainDisp),
	mTrace(pTrace),
	mRefCnt(0),
	mTimeoutCnt(0),
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

	mQueryTimer = new QTimer(this);
	connect(mQueryTimer, SIGNAL(timeout()), this, SLOT(QueryTimedOut()));
	mQueryTimer->setSingleShot(true);
	mQueryTimer->setInterval(2000);

	mDelayedNameSyncTimer = new QTimer(this);
	connect(mDelayedNameSyncTimer, SIGNAL(timeout()), this, SLOT(SyncNameAndEffectsFromAxe()));
	mDelayedNameSyncTimer->setSingleShot(true);
	mDelayedNameSyncTimer->setInterval(kDefaultNameSyncTimerInterval);

	mDelayedEffectsSyncTimer = new QTimer(this);
	connect(mDelayedEffectsSyncTimer, SIGNAL(timeout()), this, SLOT(SyncEffectsFromAxe()));
	mDelayedEffectsSyncTimer->setSingleShot(true);
	mDelayedEffectsSyncTimer->setInterval(kDefaultEffectsSyncTimerInterval);

	AxemlLoader ldr(mTrace);
	if (Axe2 <= mModel)
	{
		// C:\Program Files (x86)\Fractal Audio\Axe-Edit 1.0\Configs\II\default.axeml
		ldr.Load(mModel, appPath + "/config/axefx2.default.axeml", mAxeEffectInfo);
	}
	else
	{
		// C:\Program Files (x86)\Fractal Audio\Axe-Edit 1.0\Configs\Ultra\default.axeml
		ldr.Load(mModel, appPath + "/config/default.axeml", mAxeEffectInfo);
	}
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
	::NormalizeAxeEffectName(normalizedEffectName);
	for (AxeEffectBlockInfo & cur : mAxeEffectInfo)
	{
		if (cur.mNormalizedName == normalizedEffectName)
		{
			if (cur.mPatch && mTrace)
			{
				std::string msg("Warning: multiple Axe-Fx patches for " + patch->GetName() + "\n");
				mTrace->Trace(msg);
			}

			if (-1 != bypassCc)
				cur.mBypassCC = bypassCc;

			cur.mPatch = patch;
			return true;
		}
	}

	const std::string xy(" x/y");
	const int xyPos = normalizedEffectName.find(xy);
	if (-1 != xyPos)
	{
		normalizedEffectName.replace(xyPos, xy.length(), "");
		for (AxeEffectBlockInfo & cur : mAxeEffectInfo)
		{
			if (cur.mNormalizedName == normalizedEffectName)
			{
				if (cur.mXyPatch && mTrace)
				{
					std::string msg("Warning: multiple Axe-Fx patches for XY control '" + patch->GetName() + "'\n");
					mTrace->Trace(msg);
				}

				cur.mXyPatch = patch;
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
	// http://www.fractalaudio.com/forum/viewtopic.php?f=14&t=21524&start=10
	// http://axefxwiki.guitarlogic.org/index.php?title=Axe-Fx_SysEx_Documentation
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
	case 0xd:
		// tuner info
		return;
	case 0xe:
		if (Axe2 <= bytes[4])
		{
			_ASSERTE(Axe2 == mModel || Axe2XL == mModel || Axe2XLPlus == mModel);
			ReceivePresetEffectsV2(&bytes[6], len - 6);
		}
		else
		{
			_ASSERTE(Axe2 > mModel);
			ReceivePresetEffects(&bytes[6], len - 6);
		}
		return;
	case 0xf:
		ReceivePresetName(&bytes[6], len - 6);
		return;
	case 0x10:
		// Tempo: f0 00 01 74 01 10 f7
		if (mTempoPatch)
		{
			mTempoPatch->ActivateSwitchDisplay(mSwitchDisplay, true);
			mSwitchDisplay->SetIndicatorThreadSafe(false, mTempoPatch, 75);
		}
		return;
	case 0x11:
		// x y state change (prior to axeII fw9?)
//		DelayedEffectsSyncFromAxe();
		if (kDbgFlag && mTrace)
		{
			const std::string msg("AxeFx: X/Y state change\n");
			mTrace->Trace(msg);
		}
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

AxeEffectBlockInfo *
AxeFx3Manager::IdentifyBlockInfoUsingCc(const byte * bytes)
{
	int effectId;
	int cc;

	if (Axe2 <= mModel)
	{
		const int ccLs = bytes[0] >> 1;
		const int ccMs = (bytes[1] & 1) << 6;
		cc = ccMs | ccLs;

		const int effectIdLs = bytes[2] >> 3;
		const int effectIdMs = (bytes[3] << 4) & 0xff;
		effectId = effectIdMs | effectIdLs;
	}
	else
	{
		const int effectIdLs = bytes[0];
		const int effectIdMs = bytes[1] << 4;
		effectId = effectIdMs | effectIdLs;

		const int ccLs = bytes[2];
		const int ccMs = bytes[3] << 4;
		cc = ccMs | ccLs;
	}

	for (auto & cur : mAxeEffectInfo)
	{
		if (cur.mSysexEffectId == effectId && cur.mBypassCC == cc)
			return &cur;
	}

	return nullptr;
}

// This method is provided for last chance guess.  Only use if all other methods
// fail.  It can return incorrect information since many effects are available
// in quantity.  This will return the first effect Id match to the exclusion
// of instance id.  It is meant for Feedback Return which is a single instance
// effect that has no default cc for bypass.
AxeEffectBlockInfo *
AxeFx3Manager::IdentifyBlockInfoUsingEffectId(const byte * bytes)
{
	int effectId;
	if (Axe2 <= mModel)
	{
		const int effectIdLs = bytes[2] >> 3;
		const int effectIdMs = (bytes[3] << 4) & 0xff;
		effectId = effectIdMs | effectIdLs;
	}
	else
	{
		const int effectIdLs = bytes[0];
		const int effectIdMs = bytes[1] << 4;
		effectId = effectIdMs | effectIdLs;
	}

	for (auto & cur : mAxeEffectInfo)
	{
		if (cur.mSysexEffectId == effectId)
			return &cur;
	}

	return nullptr;
}

AxeEffectBlocks::iterator
AxeFx3Manager::GetBlockInfo(PatchPtr patch)
{
	for (AxeEffectBlocks::iterator it = mAxeEffectInfo.begin(); 
		it != mAxeEffectInfo.end(); 
		++it)
	{
		const AxeEffectBlockInfo & cur = *it;
		if (cur.mPatch == patch)
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
AxeFx3Manager::KillResponseTimer()
{
	if (!mQueryTimer->isActive())
		return;

	class StopQueryTimer : public QEvent
	{
		AxeFx3ManagerPtr mMgr;

	public:
		StopQueryTimer(AxeFx3ManagerPtr mgr) :
		  QEvent(User), 
		  mMgr(mgr)
		{
		}

		~StopQueryTimer()
		{
			mMgr->mQueryTimer->stop();
		}
	};

	QCoreApplication::postEvent(this, new StopQueryTimer(GetSharedThis()));
}

class StartQueryTimer : public QEvent
{
	AxeFx3ManagerPtr mMgr;

public:
	StartQueryTimer(AxeFx3ManagerPtr mgr) :
	  QEvent(User), 
	  mMgr(mgr)
	{
	}

	~StartQueryTimer()
	{
		mMgr->mQueryTimer->start();
	}
};

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

	if (mQueryTimer)
	{
		QMutexLocker lock(&mQueryLock);
		if (mQueryTimer->isActive())
			mQueryTimer->stop();
		delete mQueryTimer;
		mQueryTimer = nullptr;
	}

	mQueries.clear();
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

	if (!force && Axe2 <= mModel && mFirmwareMajorVersion > 5)
	{
		// Axe-Fx II sends preset loaded message, so we can ignore our 
		// unforced calls to DelalyedNameSyncFromAxe.
		// Don't know what firmware version that started in - assume v6+.
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

void
AxeFx3Manager::SendFirmwareVersionQuery()
{
	// request firmware vers (std\ultra): F0 00 01 74 01 08 00 00 F7
	// axefx2: F0 00 01 74 03 08 0A 04 F7
	if (!mMidiOut)
		return;

	// Axe-Fx 2
	// --------
	// send:		F0 00 01 74 03 08 0A 04 F7
	// respond:		F0 00 01 74 03 08 02 00 0C F7	(for 2.0)
	// respond:		F0 00 01 74 03 08 03 02 0f F7	(for 3.2)

	QMutexLocker lock(&mQueryLock);
	const byte firstDataByte = Axe2 <= mModel ? 0x0a : 0;
	byte rawBytes[] = { 0xF0, 0x00, 0x01, 0x74, byte(mModel), 0x08, firstDataByte, 0x00, 0xF7 };
	if (Axe2 <= mModel)
	{
		byte chkSum = (rawBytes[0] ^ rawBytes[1] ^ rawBytes[2] ^ rawBytes[3] ^ rawBytes[4] ^ rawBytes[5] ^ rawBytes[6]) & 0x7f;
		rawBytes[7] = chkSum;
	}
	const Bytes bb(rawBytes, rawBytes + sizeof(rawBytes));
	mMidiOut->MidiOut(bb);
}

void
AxeFx3Manager::ReceiveFirmwareVersionResponse(const byte * bytes, int len)
{
	// response to firmware version request: F0 00 01 74 01 08 0A 03 F7
	if (len < 9)
		return;

	std::string model;
	switch (bytes[4])
	{
	case AxeStd:
	case AxeUltra:
		if (0 == bytes[6] && 0 == bytes[7])
			return; // our request got looped back
		break;
	case Axe2:
	case Axe2XL:
	case Axe2XLPlus:
		if (0xa == bytes[6] && 0x04 == bytes[7])
			return; // our request got looped back
		break;
	}

	switch (bytes[4])
	{
	case AxeStd:
		model = "Standard ";
		mModel = AxeStd;
		break;
	case AxeUltra:
		model = "Ultra ";
		mModel = AxeUltra;
		break;
	case Axe2:
		model = "II ";
		mModel = Axe2;
		break;
	case Axe2XL:
		model = "II XL";
		mModel = Axe2XL;
		break;
	case Axe2XLPlus:
		model = "II XL+";
		mModel = Axe2XLPlus;
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
	if (Axe2 <= mModel)
	{
		if (7 < mFirmwareMajorVersion)
			EnableLooperStatusMonitor(true);

		SyncNameAndEffectsFromAxe();
	}
}

void
AxeFx3Manager::RequestPresetName()
{
	if (!mFirmwareMajorVersion)
		SendFirmwareVersionQuery();

	KillResponseTimer();
	QMutexLocker lock(&mQueryLock);
	if (!mFirmwareMajorVersion)
		return;

	if (Axe2 <= mModel)
	{
		// final 0x00 is a placeholder for the checksum
		byte rawBytes[] = { 0xF0, 0x00, 0x01, 0x74, byte(mModel), 0x0f, 0x00, 0xF7 };
		byte chkSum = (rawBytes[0] ^ rawBytes[1] ^ rawBytes[2] ^ rawBytes[3] ^ rawBytes[4] ^ rawBytes[5]) & 0x7f;
		rawBytes[6] = chkSum;
		const Bytes bb(rawBytes, rawBytes + sizeof(rawBytes));
		mMidiOut->MidiOut(bb);
	}
	else
	{
		const byte rawBytes[] = { 0xF0, 0x00, 0x01, 0x74, byte(mModel), 0x0f, 0xF7 };
		const Bytes bb(rawBytes, rawBytes + sizeof(rawBytes));
		mMidiOut->MidiOut(bb);
	}
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

	KillResponseTimer();
	QMutexLocker lock(&mQueryLock);
	if (!mFirmwareMajorVersion)
		return;

	// default to no fx in patch, but don't update LEDs until later
	for (auto & cur : mAxeEffectInfo)
	{
		if (-1 == cur.mSysexBypassParameterId)
			continue;

		if (-1 == cur.mSysexEffectId)
			continue;

		if (!cur.mPatch)
			continue;

		cur.mEffectIsPresentInAxePatch = false;
	}

	if (Axe2 <= mModel)
	{
		// final 0x00 is a placeholder for the checksum
		byte rawBytes[] = { 0xF0, 0x00, 0x01, 0x74, byte(mModel), 0x0e, 0x00, 0xF7 };
		byte chkSum = (rawBytes[0] ^ rawBytes[1] ^ rawBytes[2] ^ rawBytes[3] ^ rawBytes[4] ^ rawBytes[5]) & 0x7f;
		rawBytes[6] = chkSum;
		const Bytes bb(rawBytes, rawBytes + sizeof(rawBytes));
		mMidiOut->MidiOut(bb);
	}
	else
	{
		const byte rawBytes[] = { 0xF0, 0x00, 0x01, 0x74, byte(mModel), 0x0e, 0xF7 };
		const Bytes bb(rawBytes, rawBytes + sizeof(rawBytes));
		mMidiOut->MidiOut(bb);
	}
}

void
AxeFx3Manager::ReceivePresetEffects(const byte * bytes, int len)
{
	// for each effect, there are 2 bytes for the ID, 2 bytes for the bypass CC and 1 byte for the state
	// 0A 06 05 02 00
	for (int idx = 0; (idx + 5) < len; idx += 5)
	{
		if (false && mTrace)
		{
			const std::string byteDump(::GetAsciiHexStr(&bytes[idx], 5, true) + "\n");
			mTrace->Trace(byteDump);
		}

		AxeEffectBlockInfo * inf = nullptr;
		inf = IdentifyBlockInfoUsingCc(bytes + idx);
		if (!inf)
		{
			inf = IdentifyBlockInfoUsingEffectId(bytes + idx);
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
			const byte kParamVal = bytes[idx + 4];
			const bool notBypassed = (1 == kParamVal);
			const bool isBypassed = (0 == kParamVal);

			if (isBypassed || notBypassed)
			{
				if (inf->mPatch->IsActive() != notBypassed)
					inf->mPatch->UpdateState(mSwitchDisplay, notBypassed);
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

		AxeEffectBlockInfo * inf = nullptr;
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

				if (inf->mXyPatch)
				{
					// X is the active state, Y is inactive
					const bool isX = isActive || isBypassed;
					const bool isY = isActiveY || isBypassedY;
					_ASSERTE(isX ^ isY);
					// Axe-FxII considers X active and Y inactive, but I prefer
					// LED off for X and on for Y.  Original behavior below was to
					// use isX instead of isY.  See AxeTogglePatch::AxeTogglePatch
					// for the other change required for LED inversion of X/Y.
					inf->mXyPatch->UpdateState(mSwitchDisplay, isY);
				}
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
		if (-1 == cur.mSysexBypassParameterId)
			continue;

		if (-1 == cur.mSysexEffectId)
			continue;

		if (!cur.mPatch)
			continue;

		if (!cur.mEffectIsPresentInAxePatch)
		{
			cur.mPatch->Disable(mSwitchDisplay);
			if (cur.mXyPatch)
				cur.mXyPatch->Disable(mSwitchDisplay);
		}
	}
}

void
AxeFx3Manager::ReceivePresetNumber(const byte * bytes, int len)
{
	if (len < 3)
		return;

	mCurrentAxePresetName.clear();
	// 0xdd Preset Number bits 6-0
	// 0xdd Preset Number bits 13-7
	const int presetMs = bytes[0] << 7;
	const int presetLs = bytes[1];
	mCurrentAxePreset = presetMs | presetLs;
}

void
AxeFx3Manager::EnableLooperStatusMonitor(bool enable)
{
	if (Axe2 > mModel || mFirmwareMajorVersion < 6)
		return;

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
	if (mModel < Axe2)
		return;

	std::string name(patch->GetName());
	::NormalizeAxeEffectName(name);
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

	if (Axe2 <= mModel && mFirmwareMajorVersion > 8 && -1 != mCurrentScene)
	{
		char sceneBuf[4];
		::_itoa_s(mCurrentScene + 1, sceneBuf, 10);
		curText += std::string("\nAxe-Fx scene: ") + sceneBuf;
	}

	if (curText.length())
		mMainDisplay->TextOut(curText);
}
