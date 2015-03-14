/*
 * mTroll MIDI Controller
 * Copyright (C) 2010-2015 Sean Echevarria
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
#include "AxeFxManager.h"
#include "ITraceDisplay.h"
#include "HexStringUtils.h"
#include "IMidiIn.h"
#include "Patch.h"
#include "ISwitchDisplay.h"
#include "AxemlLoader.h"
#include "IMidiOut.h"
#include "IMainDisplay.h"


// Consider: restrict effect bypasses to mEffectIsPresentInAxePatch?

#ifdef _DEBUG
static const int kDbgFlag = 1;
#else
static const int kDbgFlag = 0;
#endif

static void SynonymNormalization(std::string & name);

static const int kDefaultNameSyncTimerInterval = 100;
static const int kDefaultEffectsSyncTimerInterval = 20;

AxeFxManager::AxeFxManager(IMainDisplay * mainDisp, 
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
	mTempoPatch(NULL),
	// mQueryLock(QMutex::Recursive),
	mMidiOut(NULL),
	mFirmwareMajorVersion(0),
	mAxeChannel(ch),
	mModel(mod),
	mLooperState(0),
	mCurrentAxePreset(-1),
	mCurrentScene(-1)
{
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

AxeFxManager::~AxeFxManager()
{
	if (mDelayedNameSyncTimer->isActive())
		mDelayedNameSyncTimer->stop();
	delete mDelayedNameSyncTimer;
	mDelayedNameSyncTimer = NULL;

	if (mDelayedEffectsSyncTimer->isActive())
		mDelayedEffectsSyncTimer->stop();
	delete mDelayedEffectsSyncTimer;
	mDelayedEffectsSyncTimer = NULL;

	QMutexLocker lock(&mQueryLock);
	if (mQueryTimer->isActive())
		mQueryTimer->stop();
	delete mQueryTimer;
	mQueryTimer = NULL;
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
AxeFxManager::SetScenePatch(int scene, Patch * patch)
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
AxeFxManager::SetSyncPatch(Patch * patch, int bypassCc /*= -1*/)
{
	std::string normalizedEffectName(patch->GetName());
	::NormalizeAxeEffectName(normalizedEffectName);
	for (AxeEffectBlocks::iterator it = mAxeEffectInfo.begin(); 
		it != mAxeEffectInfo.end(); 
		++it)
	{
		AxeEffectBlockInfo & cur = *it;
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
		for (AxeEffectBlocks::iterator it = mAxeEffectInfo.begin(); 
			it != mAxeEffectInfo.end(); 
			++it)
		{
			AxeEffectBlockInfo & cur = *it;
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
AxeFxManager::CompleteInit(IMidiOut * midiOut)
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

	const byte kAxeFx[] = { 0xf0, 0x00, 0x01, 0x74 };
	if (::memcmp(bytes, kAxeFx, 4))
		return false;

// 	const byte kAxeFxStdId[]	= { 0xf0, 0x00, 0x01, 0x74, 0x00 };
// 	const byte kAxeFxUltraId[]	= { 0xf0, 0x00, 0x01, 0x74, 0x01 };
// 	const byte kAxeFx2Id[]		= { 0xf0, 0x00, 0x01, 0x74, 0x03 };

	switch (bytes[4])
	{
	case AxeStd:
	case AxeUltra:
	case Axe2:
	case Axe2XL:
	case Axe2XLPlus:
		return true;
	}

	return false;
}

void
AxeFxManager::ReceivedSysex(const byte * bytes, int len)
{
	// http://www.fractalaudio.com/forum/viewtopic.php?f=14&t=21524&start=10
	// http://axefxwiki.guitarlogic.org/index.php?title=Axe-Fx_SysEx_Documentation
	if (!::IsAxeFxSysex(bytes, len))
		return;

	switch (bytes[5])
	{
	case 2:
		if (Axe2 <= bytes[4])
		{
			_ASSERTE(Axe2 == mModel || Axe2XL == mModel || Axe2XLPlus == mModel);
			// ReceiveParamValue hasn't been updated for Axe-FX II
			if (kDbgFlag && mTrace)
			{
				const std::string msg("AxeFx2: received param value?\n");
				mTrace->Trace(msg);
			}
		}
		else
		{
			_ASSERTE(Axe2 > mModel);
			ReceiveParamValue(&bytes[6], len - 6);
		}
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

// this SyncFromAxe helper is not currently being used
AxeEffectBlockInfo *
AxeFxManager::IdentifyBlockInfoUsingBypassId(const byte * bytes)
{
	int effectIdLs;
	int effectIdMs;
	int parameterIdLs;
	int parameterIdMs;

	if (Axe2 <= mModel)
	{
		// TODO: not updated for axe2
		effectIdLs = 0;
		effectIdMs = 0;
		parameterIdLs = 0;
		parameterIdMs = 0;
	}
	else
	{
		effectIdLs = bytes[0];
		effectIdMs = bytes[1];
		parameterIdLs = bytes[2];
		parameterIdMs = bytes[3];
	}

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

AxeEffectBlockInfo *
AxeFxManager::IdentifyBlockInfoUsingCc(const byte * bytes)
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

	for (AxeEffectBlocks::iterator it = mAxeEffectInfo.begin(); 
		it != mAxeEffectInfo.end(); 
		++it)
	{
		AxeEffectBlockInfo & cur = *it;
		if (cur.mSysexEffectId == effectId && cur.mBypassCC == cc)
			return &(*it);
	}

	return NULL;
}

// This method is provided for last chance guess.  Only use if all other methods
// fail.  It can return incorrect information since many effects are available
// in quantity.  This will return the first effect Id match to the exclusion
// of instance id.  It is meant for Feedback Return which is a single instance
// effect that has no default cc for bypass.
AxeEffectBlockInfo *
AxeFxManager::IdentifyBlockInfoUsingEffectId(const byte * bytes)
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

	for (AxeEffectBlocks::iterator it = mAxeEffectInfo.begin(); 
		it != mAxeEffectInfo.end(); 
		++it)
	{
		AxeEffectBlockInfo & cur = *it;
		if (cur.mSysexEffectId == effectId)
			return &(*it);
	}

	return NULL;
}

AxeEffectBlocks::iterator
AxeFxManager::GetBlockInfo(Patch * patch)
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

// this SyncFromAxe helper is not currently being used
void
AxeFxManager::ReceiveParamValue(const byte * bytes, int len)
{
	// TODO: not updated for Axe2/Axe2XL/Axe2XLPlus
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

	KillResponseTimer();

	if (len < 8)
	{
		if (mTrace)
		{
			const std::string msg("truncated param value msg\n");
			mTrace->Trace(msg);
		}

		QMutexLocker lock(&mQueryLock);
		if (mQueries.begin() != mQueries.end())
			mQueries.pop_front();
	}
	else
	{
		AxeEffectBlockInfo * inf = NULL;

		{
			QMutexLocker lock(&mQueryLock);
			std::list<AxeEffectBlockInfo*>::iterator it = mQueries.begin();
			if (it != mQueries.end() &&
				(*it)->mSysexEffectIdLs == bytes[0] && 
				(*it)->mSysexEffectIdMs == bytes[1] &&
				(*it)->mSysexBypassParameterIdLs == bytes[2] &&
				(*it)->mSysexBypassParameterIdMs == bytes[3])
			{
				inf = *mQueries.begin();
				mQueries.pop_front();
			}
		}

		if (!inf)
		{
			// received something we weren't expecting; see if we can look it up
			inf = IdentifyBlockInfoUsingBypassId(bytes);
		}

		if (inf && inf->mPatch)
		{
			if (0 == bytes[5])
			{
				const bool isBypassed = (1 == bytes[4] || 3 == bytes[4]); // bypass param is 1 for bypassed, 0 for not bypassed (except for Amp and Pitch)
				const bool notBypassed = (0 == bytes[4] || 2 == bytes[4]);

				if (isBypassed || notBypassed)
				{
					if (inf->mPatch->IsActive() != notBypassed)
						inf->mPatch->UpdateState(mSwitchDisplay, notBypassed);

					if (0 && mTrace)
					{
						const std::string byteDump(::GetAsciiHexStr(bytes + 4, len - 6, true));
						const std::string asciiDump(::GetAsciiStr(&bytes[6], len - 8));
						std::strstream traceMsg;
						traceMsg << inf->mName << " : " << byteDump.c_str() << " : " << asciiDump.c_str() << std::endl << std::ends;
						mTrace->Trace(std::string(traceMsg.str()));
					}
				}
				else if (mTrace)
				{
					const std::string byteDump(::GetAsciiHexStr(bytes, len - 2, true));
					std::strstream traceMsg;
					traceMsg << "Unrecognized bypass param value for " << inf->mName << " " << byteDump.c_str() << std::endl << std::ends;
					mTrace->Trace(std::string(traceMsg.str()));
				}
			}
			else if (mTrace)
			{
				const std::string byteDump(::GetAsciiHexStr(bytes, len - 2, true));
				std::strstream traceMsg;
				traceMsg << "Unhandled bypass MS param value for " << inf->mName << " " << byteDump.c_str() << std::endl << std::ends;
				mTrace->Trace(std::string(traceMsg.str()));
			}
		}
		else
		{
			if (mTrace)
			{
				const std::string msg("Axe sync error: No inf or patch\n");
				mTrace->Trace(msg);
			}
		}
	}


	// post an event to kick off the next query
	class CreateSendNextQueryTimer : public QEvent
	{
		AxeFxManager *mMgr;

	public:
		CreateSendNextQueryTimer(AxeFxManager * mgr) : 
		  QEvent(User), 
		  mMgr(mgr)
		{
			mMgr->AddRef();
		}

		~CreateSendNextQueryTimer()
		{
			mMgr->RequestNextParamValue();
			mMgr->Release();
		}
	};

	QCoreApplication::postEvent(this, new CreateSendNextQueryTimer(this));
}

void
AxeFxManager::SyncNameAndEffectsFromAxe()
{
	RequestPresetName();
}

void
AxeFxManager::SyncEffectsFromAxe()
{
	RequestPresetEffects();
}

// this SyncFromAxe helper is not currently being used
void
AxeFxManager::SyncPatchFromAxe(Patch * patch)
{
	// We have two methods to sync bypass state.
	// First is to use RequestPresetEffects.
	// Second is to use RequestParamValue.
	if (1)
	{
		// pro: if effect does not exist in preset, we'll know it by lack of return value 
		// pro: less chance of Axe-Fx deadlock?
		// pro or con: edit screen won't open to effect
		// alternatively: axe screen stays where it is at
		RequestPresetEffects();
		return;
	}

	// pro or con: RequestParamValue will cause the Axe to open the edit 
	//   screen of the effect being queried.
	// con: Potential for Axe-Fx deadlock?
	// con: If effect does not exist in preset, get a return value that can't
	//   be used to determine if it doesn't exist
	AxeEffectBlocks::iterator it = GetBlockInfo(patch);
	if (it == mAxeEffectInfo.end())
	{
		if (mTrace)
		{
			const std::string msg("Failed to locate AxeFx block info for AxeToggle patch\n");
			mTrace->Trace(msg);
		}
		return;
	}

	if (-1 == (*it).mSysexBypassParameterId)
		return;

	if (-1 == (*it).mSysexEffectId)
		return;

	if (!(*it).mPatch)
		return;

	if (!(*it).mEffectIsPresentInAxePatch)
	{
		// don't query if effect isn't present in current patch - to lessen
		// chance of axe-fx deadlock
		// opens us up to the possibility of getting a toggle out of sync with
		// any changes made by hand on the face of the unit or via other midi sources.
		// an alternative to consider would be to simply call RequestPresetEffects.
		return;
	}

	{
		QMutexLocker lock(&mQueryLock);
		mQueries.push_back(&(*it));
		if (mQueries.size() > 1)
			return; // should already be processing
	}

	// queue was empty, so kick off processing
	RequestNextParamValue();
}

void
AxeFxManager::KillResponseTimer()
{
	if (!mQueryTimer->isActive())
		return;

	class StopQueryTimer : public QEvent
	{
		AxeFxManager *mMgr;

	public:
		StopQueryTimer(AxeFxManager * mgr) : 
		  QEvent(User), 
		  mMgr(mgr)
		{
			mMgr->AddRef();
		}

		~StopQueryTimer()
		{
			mMgr->mQueryTimer->stop();
			mMgr->Release();
		}
	};

	QCoreApplication::postEvent(this, new StopQueryTimer(this));
}

// this SyncFromAxe helper is not currently being used
void
AxeFxManager::QueryTimedOut()
{
	const clock_t curTime = ::clock();
	if (curTime > mLastTimeout + 5000)
		mTimeoutCnt = 0;
	mLastTimeout = curTime;
	++mTimeoutCnt;

	if (mTrace)
	{
		std::string msg;
		if (mTimeoutCnt > 3)
			msg = ("Multiple sync request timeouts, make sure it hasn't locked up\n");
		else
			msg = ("Axe sync request timed out\n");

		mTrace->Trace(msg);
	}

	if (mTimeoutCnt > 5)
	{
		mTimeoutCnt = 0;
		if (mQueries.size())
		{
			QMutexLocker lock(&mQueryLock);
			mQueries.clear();

			if (mTrace)
			{
				std::string msg("Aborting axe sync\n");
				mTrace->Trace(msg);
			}
		}
	}

	{
		QMutexLocker lock(&mQueryLock);
		if (mQueries.begin() != mQueries.end())
			mQueries.pop_front();
		else
			return;
	}

	RequestNextParamValue();
}

class StartQueryTimer : public QEvent
{
	AxeFxManager *mMgr;

public:
	StartQueryTimer(AxeFxManager * mgr) : 
	  QEvent(User), 
	  mMgr(mgr)
	{
		mMgr->AddRef();
	}

	~StartQueryTimer()
	{
		mMgr->mQueryTimer->start();
		mMgr->Release();
	}
};

void
AxeFxManager::DelayedNameSyncFromAxe(bool force /*= false*/)
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
			AxeFxManager *mMgr;

		public:
			StopDelayedSyncTimer(AxeFxManager * mgr) : 
			  QEvent(User), 
				  mMgr(mgr)
			{
				mMgr->AddRef();
			}

			~StopDelayedSyncTimer()
			{
				mMgr->mDelayedNameSyncTimer->stop();
				mMgr->Release();
			}
		};

		QCoreApplication::postEvent(this, new StopDelayedSyncTimer(this));
	}

	class StartDelayedSyncTimer : public QEvent
	{
		AxeFxManager *mMgr;

	public:
		StartDelayedSyncTimer(AxeFxManager * mgr) : 
		  QEvent(User), 
		  mMgr(mgr)
		{
			mMgr->AddRef();
		}

		~StartDelayedSyncTimer()
		{
			mMgr->mDelayedNameSyncTimer->start();
			mMgr->Release();
		}
	};

	QCoreApplication::postEvent(this, new StartDelayedSyncTimer(this));
}

void
AxeFxManager::DelayedEffectsSyncFromAxe()
{
	if (!mDelayedEffectsSyncTimer)
		return;

	if (mDelayedEffectsSyncTimer->isActive())
	{
		class StopDelayedSyncTimer : public QEvent
		{
			AxeFxManager *mMgr;

		public:
			StopDelayedSyncTimer(AxeFxManager * mgr) : 
			  QEvent(User), 
				  mMgr(mgr)
			{
				mMgr->AddRef();
			}

			~StopDelayedSyncTimer()
			{
				mMgr->mDelayedEffectsSyncTimer->stop();
				mMgr->Release();
			}
		};

		QCoreApplication::postEvent(this, new StopDelayedSyncTimer(this));
	}

	class StartDelayedSyncTimer : public QEvent
	{
		AxeFxManager *mMgr;

	public:
		StartDelayedSyncTimer(AxeFxManager * mgr) : 
		  QEvent(User), 
		  mMgr(mgr)
		{
			mMgr->AddRef();
		}

		~StartDelayedSyncTimer()
		{
			mMgr->mDelayedEffectsSyncTimer->start();
			mMgr->Release();
		}
	};

	QCoreApplication::postEvent(this, new StartDelayedSyncTimer(this));
}

// this SyncFromAxe helper is not currently being used
void
AxeFxManager::RequestNextParamValue()
{
	if (!mMidiOut)
		return;

	if (!mFirmwareMajorVersion)
		SendFirmwareVersionQuery();

	QMutexLocker lock(&mQueryLock);
	if (mQueries.begin() == mQueries.end())
		return;

	if (!mFirmwareMajorVersion)
		return;

/* MIDI_SET_PARAMETER (or GET)
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

	// TODO: not updated for Axe-Fx 2
	const byte rawBytes[] = { 0xF0, 0x00, 0x01, 0x74, byte(mModel), 0x02, 0, 0, 0, 0, 0x0, 0x0, 0x00, 0xF7 };
	Bytes bb(rawBytes, rawBytes + sizeof(rawBytes));
	AxeEffectBlockInfo * next = *mQueries.begin();
	bb[6] = next->mSysexEffectIdLs;
	bb[7] = next->mSysexEffectIdMs;
	bb[8] = next->mSysexBypassParameterIdLs;
	bb[9] = next->mSysexBypassParameterIdMs;

	QCoreApplication::postEvent(this, new StartQueryTimer(this));

	mMidiOut->MidiOut(bb);
}

void
AxeFxManager::SendFirmwareVersionQuery()
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
AxeFxManager::ReceiveFirmwareVersionResponse(const byte * bytes, int len)
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
	if (Axe2 <= mModel && 7 < mFirmwareMajorVersion)
		EnableLooperStatusMonitor(true);
}

void
AxeFxManager::RequestPresetName()
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
AxeFxManager::ReceivePresetName(const byte * bytes, int len)
{
	if (len < 21)
		return;

	std::string patchName((const char *)bytes, 21);
	if (mMainDisplay && patchName.size())
	{
		mCurrentAxePresetName = patchName;
		DisplayPresetStatus();
	}

	RequestPresetEffects();
}

void
AxeFxManager::RequestPresetEffects()
{
	if (!mFirmwareMajorVersion)
		SendFirmwareVersionQuery();

	KillResponseTimer();
	QMutexLocker lock(&mQueryLock);
	if (!mFirmwareMajorVersion)
		return;

	// default to no fx in patch, but don't update LEDs until later
	for (AxeEffectBlocks::iterator it = mAxeEffectInfo.begin(); 
		it != mAxeEffectInfo.end(); 
		++it)
	{
		AxeEffectBlockInfo * cur = &(*it);
		if (-1 == cur->mSysexBypassParameterId)
			continue;

		if (-1 == cur->mSysexEffectId)
			continue;

		if (!cur->mPatch)
			continue;

		cur->mEffectIsPresentInAxePatch = false;
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
AxeFxManager::ReceivePresetEffects(const byte * bytes, int len)
{
	// for each effect, there are 2 bytes for the ID, 2 bytes for the bypass CC and 1 byte for the state
	// 0A 06 05 02 00
	for (int idx = 0; (idx + 5) < len; idx += 5)
	{
		if (0 && mTrace)
		{
			const std::string byteDump(::GetAsciiHexStr(&bytes[idx], 5, true) + "\n");
			mTrace->Trace(byteDump);
		}

		AxeEffectBlockInfo * inf = NULL;
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
AxeFxManager::ReceivePresetEffectsV2(const byte * bytes, int len)
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
		if (0 && kDbgFlag && mTrace)
		{
			const std::string byteDump(::GetAsciiHexStr(&bytes[idx], 5, true) + "\n");
			mTrace->Trace(byteDump);
		}

		AxeEffectBlockInfo * inf = NULL;
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
AxeFxManager::TurnOffLedsForNaEffects()
{
	// turn off LEDs for all effects not in cur preset
	for (AxeEffectBlocks::iterator it = mAxeEffectInfo.begin(); 
		it != mAxeEffectInfo.end(); 
		++it)
	{
		AxeEffectBlockInfo * cur = &(*it);
		if (-1 == cur->mSysexBypassParameterId)
			continue;

		if (-1 == cur->mSysexEffectId)
			continue;

		if (!cur->mPatch)
			continue;

		if (!cur->mEffectIsPresentInAxePatch)
		{
			cur->mPatch->Disable(mSwitchDisplay);
			if (cur->mXyPatch)
				cur->mXyPatch->Disable(mSwitchDisplay);
		}
	}
}

void
AxeFxManager::ReceivePresetNumber(const byte * bytes, int len)
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
AxeFxManager::EnableLooperStatusMonitor(bool enable)
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
	{ 0xF0, 0x00, 0x01, 0x74, byte(mModel), 0x23, enable ? 1 : 0, enable ? 0x24 : 0x25, 0xF7 };
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
AxeFxManager::ReceiveLooperStatus(const byte * bytes, int len)
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
AxeFxManager::SetLooperPatch(Patch * patch)
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
AxeFxManager::ReceiveSceneStatus(const byte * bytes, int len)
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
AxeFxManager::DisplayPresetStatus()
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




void 
NormalizeAxeEffectName(std::string &effectName) 
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

// 	pos = effectName.find(" mute");
// 	if (-1 != pos)
// 		effectName.replace(pos, 5, "");
// 	else
// 	{
// 		pos = effectName.find("mute ");
// 		if (-1 != pos)
// 			effectName.replace(pos, 5, "");
// 	}
// 
// 	pos = effectName.find(" toggle");
// 	if (-1 != pos)
// 		effectName.replace(pos, 7, "");
// 	else
// 	{
// 		pos = effectName.find("toggle ");
// 		if (-1 != pos)
// 			effectName.replace(pos, 7, "");
// 	}
// 
// 	pos = effectName.find(" momentary");
// 	if (-1 != pos)
// 		effectName.replace(pos, 10, "");
// 	else
// 	{
// 		pos = effectName.find("momentary ");
// 		if (-1 != pos)
// 			effectName.replace(pos, 10, "");
// 	}

	SynonymNormalization(effectName);
}

struct DefaultAxeCcs
{
	std::string mParameter;
	int			mCc;
};

// consider: define this list in an external xml file
DefaultAxeCcs kDefaultAxeCcs[] = 
{
	// these don't have defaults (some can't be bypassed)
	{"feedback send", 0},
	{"mixer 1", 0},
	{"mixer 2", 0},
	{"feedback return", 0},
	{"noisegate", 0},

	{"input volume", 10},
	{"out 1 volume", 11},
	{"out 2 volume", 12},
	{"output", 13},
	{"taptempo", 14},
	{"tuner", 15},
	{"external 1", 16},
	{"external 2", 17},
	{"external 3", 18},
	{"external 4", 19},
	{"external 5", 20},
	{"external 6", 21},
	{"external 7", 22},
	{"external 8", 23},

	// extra AxeFx II externals (overlap w/ AxeFx std/ultra loopers)
	{"external 9", 24},
	{"external 10", 25},
	{"external 11", 26},
	{"external 12", 27},

	// 2 loopers in AxeFx Std/Ultra
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

	// single looper in AxeFx II (overlap w/ AxeFx std/ultra loopers)
	{"looper record", 28},
	{"looper play", 29},
	{"looper once", 30},
	{"looper overdub", 31},
	{"looper reverse", 32},
	{"looper", 33},
	// additional looper controls in AxeFx II fw 6
	{"looper half", 120},
	{"looper undo", 121},
	{"looper metronome", 122},

	{"scene increment", 123},
	{"scene decrement", 124},

	{"global preset effect toggle", 34},
	{"scene", 34},	// Axe-Fx II v9 replaces global preset effect toggle
	{"scene 1", 34},
	{"scene 2", 34},
	{"scene 3", 34},
	{"scene 4", 34},
	{"scene 5", 34},
	{"scene 6", 34},
	{"scene 7", 34},
	{"scene 8", 34},
	{"volume increment", 35},
	{"volume decrement", 36},

	{"amp 1", 37},
	{"amp 2", 38},
	{"cabinet 1", 39},
	{"cabinet 2", 40},
	{"chorus 1", 41},
	{"chorus 2", 42},
	{"compressor 1", 43},
	{"compressor 2", 44},
	{"crossover 1", 45},
	{"crossover 2", 46},
	{"delay 1", 47},
	{"delay 2", 48},
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
	{"effects loop", 59},
	{"gate/expander 1", 60},
	{"gate/expander 2", 61},
	{"graphic eq 1", 62},
	{"graphic eq 2", 63},
	{"graphic eq 3", 64},
	{"graphic eq 4", 65},
	{"megatap delay", 66},
	{"multiband comp 1", 67},
	{"multiband comp 2", 68},
	{"multidelay 1", 69},
	{"multidelay 2", 70},
	{"parametric eq 1", 71},
	{"parametric eq 2", 72},
	{"parametric eq 3", 73},
	{"parametric eq 4", 74},
	{"phaser 1", 75},
	{"phaser 2", 76},
	{"pitch 1", 77},
	{"pitch 2", 78},
	{"quad chorus 1", 79},
	{"quad chorus 2", 80},
	{"resonator 1", 81},
	{"resonator 2", 82},
	{"reverb 1", 83},
	{"reverb 2", 84},
	{"ring mod", 85},
	{"rotary 1", 86},
	{"rotary 2", 87},
	{"synth 1", 88},
	{"synth 2", 89},
	{"panner/tremolo 1", 90},
	{"panner/tremolo 2", 91},
	{"vocoder", 92},
	{"vol/pan 1", 93},
	{"vol/pan 2", 94},
	{"vol/pan 3", 95},
	{"vol/pan 4", 96},
	{"wah-wah 1", 97},
	{"wah-wah 2", 98},
	{"tone match", 99},

	// x/y ccs introduced in AxeFx II
	{"amp 1 x/y", 100},
	{"amp 2 x/y", 101},
	{"cabinet 1 x/y", 102},
	{"cabinet 2 x/y", 103},
	{"chorus 1 x/y", 104},
	{"chorus 2 x/y", 105},
	{"delay 1 x/y", 106},
	{"delay 2 x/y", 107},
	{"drive 1 x/y", 108},
	{"drive 2 x/y", 109},
	{"flanger 1 x/y", 110},
	{"flanger 2 x/y", 111},
	{"phaser 1 x/y", 112},
	{"phaser 2 x/y", 113},
	{"pitch 1 x/y", 114},
	{"pitch 2 x/y", 115},
	{"reverb 1 x/y", 116},
	{"reverb 2 x/y", 117},
	{"wah-wah 1 x/y", 118},
	{"wah-wah 2 x/y", 119},

	{"", -1}
};

int
GetDefaultAxeCc(const std::string &effectNameIn, ITraceDisplay * trc) 
{
	std::string effectName(effectNameIn);
	NormalizeAxeEffectName(effectName);

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

void
SynonymNormalization(std::string & name)
{
#define MapName(subst, legit) if (name == subst) { name = legit; return; }

	int pos = name.find('(');
	if (std::string::npos != pos)
	{
		name = name.substr(0, pos);
		while (name.at(name.length() - 1) == ' ')
			name = name.substr(0, name.length() - 1);
	}

	pos = name.find(" xy");
	if (std::string::npos != pos)
		name.replace(pos, 3, " x/y");

	switch (name[0])
	{
	case 'a':
		MapName("amp match", "tone match");
		MapName("arpeggiator 1", "pitch 1");
		MapName("arpeggiator 2", "pitch 2");
		break;
	case 'b':
		MapName("band delay 1", "multidelay 1");
		MapName("band delay 2", "multidelay 2");
		MapName("band dly 1", "multidelay 1");
		MapName("band dly 2", "multidelay 2");
		break;
	case 'c':
		MapName("cab 1", "cabinet 1");
		MapName("cab 2", "cabinet 2");
		MapName("cab 1 x/y", "cabinet 1 x/y");
		MapName("cab 2 x/y", "cabinet 2 x/y");
		MapName("comp 1", "compressor 1");
		MapName("comp 2", "compressor 2");
		MapName("crystals 1", "pitch 1");
		MapName("crystals 2", "pitch 2");
		break;
	case 'd':
		MapName("dec scene", "scene decrement");
		MapName("decrement scene", "scene decrement");
		MapName("delay 1 (reverse)", "delay 1");
		MapName("delay 2 (reverse)", "delay 2");
		MapName("detune 1", "pitch 1");
		MapName("detune 2", "pitch 2");
		MapName("device complete bypass toggle", "output");
		MapName("diff 1", "multidelay 1");
		MapName("diff 2", "multidelay 2");
		MapName("diffuser 1", "multidelay 1");
		MapName("diffuser 2", "multidelay 2");
		MapName("dly 1", "delay 1");
		MapName("dly 2", "delay 2");
		MapName("dly 1 x/y", "delay 1 x/y");
		MapName("dly 2 x/y", "delay 2 x/y");
		break;
	case 'e':
		MapName("eq match", "tone match");
		MapName("eqmatch", "tone match");
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
		MapName("fb ret", "feedback return");
		MapName("fb rtrn", "feedback return");
		MapName("fb return", "feedback return");
		MapName("fdbk ret", "feedback return");
		MapName("fdbk rtrn", "feedback return");
		MapName("fdbk return", "feedback return");
		MapName("feed ret", "feedback return");
		MapName("feed rtrn", "feedback return");
		MapName("feed return", "feedback return");
		MapName("feedback ret", "feedback return");
		MapName("feedback rtrn", "feedback return");

		MapName("fb send", "feedback send");
		MapName("fdbk send", "feedback send");
		MapName("feedsend", "feedback send");
		MapName("feed send", "feedback send");

		MapName("fx loop", "effects loop");
		break;
	case 'g':
		MapName("gate 1", "gate/expander 1");
		MapName("gate 2", "gate/expander 2");
		MapName("graphiceq 1", "graphic eq 1");
		MapName("graphiceq 2", "graphic eq 2");
		MapName("graphiceq 3", "graphic eq 3");
		MapName("graphiceq 4", "graphic eq 4");
		break;
	case 'h':
		MapName("half-speed", "looper half");
		MapName("half speed", "looper half");
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
		MapName("loop 2 rec", "looper 2 record");
		MapName("loop 2 record", "looper 2 record");
		MapName("loop 2 play", "looper 2 play");
		MapName("loop 2 once", "looper 2 once");
		MapName("loop 2 overdub", "looper 2 overdub");
		MapName("loop 2 rev", "looper 2 reverse");
		MapName("loop 2 reverse", "looper 2 reverse");
// 		MapName("looper", "delay 1");
		MapName("looper 1", "delay 1");
		MapName("looper 2", "delay 2");
		MapName("looper half-speed", "looper half");
		MapName("looper half speed", "looper half");
		break;
	case 'm':
		MapName("mdly 1", "multidelay 1");
		MapName("mdly 2", "multidelay 2");
		MapName("megatap", "megatap delay");
		MapName("metronome", "looper metronome");
		MapName("mix 1", "mixer 1");
		MapName("mix 2", "mixer 2");
		MapName("multi comp 1", "multiband comp 1");
		MapName("multi comp 2", "multiband comp 2");
		MapName("multi delay 1", "multidelay 1");
		MapName("multi delay 2", "multidelay 2");
		break;
	case 'n':
		MapName("next scene", "scene increment");
		MapName("noise gate", "noisegate");
		break;
	case 'o':
		MapName("octave 1", "pitch 1");
		MapName("octave 2", "pitch 2");
		MapName("out", "output");
		MapName("out 1 vol", "out 1 volume");
		MapName("out 2 vol", "out 2 volume");
		MapName("output 1 vol", "out 1 volume");
		MapName("output 2 vol", "out 2 volume");
		break;
	case 'p':
		MapName("pan/trem 1", "panner/tremolo 1");
		MapName("pan/trem 2", "panner/tremolo 2");
		MapName("para eq 1", "parametric eq 1");
		MapName("para eq 2", "parametric eq 2");
		MapName("para eq 3", "parametric eq 3");
		MapName("para eq 4", "parametric eq 4");
		MapName("pitch 1 (whammy)", "pitch 1");
		MapName("pitch 2 (whammy)", "pitch 2");
		MapName("plex 1", "multidelay 1");
		MapName("plex 2", "multidelay 2");
		MapName("plex delay 1", "multidelay 1");
		MapName("plex delay 2", "multidelay 2");
		MapName("plex detune 1", "multidelay 1");
		MapName("plex detune 2", "multidelay 2");
		MapName("plex dly 1", "multidelay 1");
		MapName("plex dly 2", "multidelay 2");
		MapName("plex shift 1", "multidelay 1");
		MapName("plex shift 2", "multidelay 2");
		MapName("previous scene", "scene decrement");
		break;
	case 'q':
		MapName("quad 1", "quad chorus 1");
		MapName("quad 2", "quad chorus 2");
		MapName("quad delay 1", "multidelay 1");
		MapName("quad delay 2", "multidelay 2");
		MapName("quad dly 1", "multidelay 1");
		MapName("quad dly 2", "multidelay 2");
		MapName("quad series 1", "multidelay 1");
		MapName("quad series 2", "multidelay 2");
		MapName("quad tap 1", "multidelay 1");
		MapName("quad tap 2", "multidelay 2");
		MapName("quadchorus 1", "quad chorus 1");
		MapName("quadchorus 2", "quad chorus 2");
		break;
	case 'r':
		MapName("return", "feedback return");
		MapName("reverse 1", "delay 1");
		MapName("reverse 2", "delay 2");
		MapName("reverse delay 1", "delay 1");
		MapName("reverse delay 2", "delay 2");
		MapName("reverse dly 1", "delay 1");
		MapName("reverse dly 2", "delay 2");
		MapName("rhythm tap 1", "multidelay 1");
		MapName("rhythm tap 2", "multidelay 2");
		MapName("ring modulator", "ring mod");
		MapName("ringmod", "ring mod");
		break;
	case 's':
		MapName("scene inc", "scene increment");
		MapName("scene dec", "scene decrement");
		MapName("send", "feedback send");
		MapName("shift 1", "pitch 1");
		MapName("shift 2", "pitch 2");
		break;
	case 't':
		MapName("tap", "taptempo");
		MapName("tap tempo", "taptempo");
		MapName("ten tap 1", "multidelay 1");
		MapName("ten tap 2", "multidelay 2");
		MapName("tone", "tone match");
		MapName("trem 1", "panner/tremolo 1");
		MapName("trem 2", "panner/tremolo 2");
		MapName("tremolo 1", "panner/tremolo 1");
		MapName("tremolo 2", "panner/tremolo 2");
		break;
	case 'v':
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
		break;
	case 'w':
		MapName("wah 1", "wah-wah 1");
		MapName("wah 2", "wah-wah 2");
		MapName("wah 1 x/y", "wah-wah 1 x/y");
		MapName("wah 2 x/y", "wah-wah 2 x/y");
		MapName("wahwah 1", "wah-wah 1");
		MapName("wahwah 2", "wah-wah 2");
		MapName("wahwah 1 x/y", "wah-wah 1 x/y");
		MapName("wahwah 2 x/y", "wah-wah 2 x/y");
		MapName("whammy 1", "pitch 1");
		MapName("whammy 2", "pitch 2");
		break;
	case 'x':
		MapName("x-over 1", "crossover 1");
		MapName("x-over 2", "crossover 2");
		break;
	}
}
