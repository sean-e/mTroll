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

static std::string NormalizeAxe3EffectName(const std::string &effectName);
static void Axe3SynonymNormalization(std::string & name);

struct Axe3EffectBlockInfo
{
	constexpr static int kMaxChannels = 7;
	// state that is basically const
	std::string			mName;						// Amp 1
	std::string			mNormalizedName;			// amp 1
	int					mSysexEffectId;				// unique per name
	int					mSysexEffectIdMs;			//	derived 
	int					mSysexEffectIdLs;			//	derived
	PatchPtr			mPatch;						// the patch assigned to this effectId
	PatchPtr			mChannelSelectPatches[kMaxChannels]; // optional channel select patches for this effectId

	// basically const, but requires deferred runtime init from the device
	int					mMaxChannels = 0;

	// state that changes at runtime dependent upon active preset and scene
	int					mCurrentChannel = 0;
	bool				mEffectIsPresentInAxePatch = true;

	Axe3EffectBlockInfo() :
		mSysexEffectId(-1),
		mSysexEffectIdLs(-1),
		mSysexEffectIdMs(-1)
	{
	}

	Axe3EffectBlockInfo(int id, const std::string & name) :
		mName(name),
		mNormalizedName(::NormalizeAxe3EffectName(name)),
		mSysexEffectId(id),
		mSysexEffectIdLs(id & 0x0000000F),
		mSysexEffectIdMs((id >> 4) & 0x0000000F)
	{
	}

	void UpdateChannelStatus(ISwitchDisplay * switchDisplay, int maxChannels, int currentChannel)
	{
		mEffectIsPresentInAxePatch = true;

		_ASSERTE(maxChannels <= kMaxChannels);
		if (maxChannels <= kMaxChannels)
			mMaxChannels = maxChannels;
		else
			maxChannels = kMaxChannels;

		_ASSERTE(currentChannel <= maxChannels);
		if (currentChannel <= maxChannels)
			mCurrentChannel = currentChannel;
		else
			mCurrentChannel = 0;

		for (int idx = 0; idx < mMaxChannels; ++idx)
		{
			PatchPtr &cur = mChannelSelectPatches[idx];
			if (!cur)
				continue;

			if (idx == currentChannel)
			{
				// enable the current channel
				cur->UpdateState(switchDisplay, true);
			}
			else
			{
				// disable the inactive channels
				cur->UpdateState(switchDisplay, false);
			}
		}
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

	mDelayedNameSyncTimer = new QTimer(this);
	connect(mDelayedNameSyncTimer, &QTimer::timeout, this, &AxeFx3Manager::SyncNameAndEffectsFromAxe);
	mDelayedNameSyncTimer->setSingleShot(true);
	mDelayedNameSyncTimer->setInterval(kDefaultNameSyncTimerInterval);

	mDelayedEffectsSyncTimer = new QTimer(this);
	connect(mDelayedEffectsSyncTimer, &QTimer::timeout, this, &AxeFx3Manager::SyncEffectsFromAxe);
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

	if (mScenePatches[scene - 1] && mTrace)
	{
		std::string msg("Warning: multiple Axe-Fx patches for same scene\n");
		mTrace->Trace(msg);
	}

	mScenePatches[scene - 1] = patch;
}

bool
AxeFx3Manager::SetSyncPatch(PatchPtr patch, int effectId, int channel)
{
	std::string normalizedEffectName(::NormalizeAxe3EffectName(patch->GetName()));
	for (Axe3EffectBlockInfo & cur : mAxeEffectInfo)
	{
		if (cur.mNormalizedName == normalizedEffectName)
		{
			if (cur.mPatch && mTrace)
			{
				std::string msg("Warning: multiple Axe-Fx patches for " + patch->GetName() + "\n");
				mTrace->Trace(msg);
			}

			cur.mPatch = patch;
			return true;
		}
	}

	if (effectId)
	{
		_ASSERTE(channel < Axe3EffectBlockInfo::kMaxChannels);
		for (Axe3EffectBlockInfo & cur : mAxeEffectInfo)
		{
			if (cur.mSysexEffectId == effectId)
			{
				if (cur.mChannelSelectPatches[channel] && mTrace)
				{
					std::string msg("Warning: multiple Axe-Fx patches for block channel select '" + patch->GetName() + "'\n");
					mTrace->Trace(msg);
				}

				cur.mChannelSelectPatches[channel] = patch;
				return true;
			}
		}

		if (mTrace)
		{
			std::string msg("Warning: no Axe-Fx effect found to sync for block channel select '" + patch->GetName() + "'\n");
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

bool
AxeFx3Manager::SetSyncPatch(PatchPtr patch, int bypassCc)
{
	_ASSERTE(!"this shouldn't be called");
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
// 	case 2:
// 		return;
// 	case 4:
// 		return;
	case 8:
		if (mFirmwareMajorVersion)
			return;
		ReceiveFirmwareVersionResponse(bytes, len);
		return;
	case 0xd:
		if (len > 8)
		{
			ReceivePresetNumber(&bytes[6], len - 6);
			ReceivePresetName(&bytes[8], len - 8);
			RequestSceneName();
		}
		return;
	case 0xe:
		if (len > 7)
		{
			ReceiveSceneName(&bytes[7], len - 7);
			ReceiveSceneStatus(&bytes[6], len - 6);
			DisplayPresetStatus();
			RequestStatusDump();
		}
		return;
	case 0x0f:
		// looper status
		ReceiveLooperState(&bytes[6], len - 6);
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
		// #axe3TunerSupport
		// tuner info
		// nn ss cc
		// nn = note 0-11
		// ss = string 0-5, 0 = low E
		// cc = cents offset binary, 63 = 0, 62 = -1, 64 = +1
		return;
	case 0x13:
		ReceiveStatusDump(&bytes[6], len - 6);
		return;
	case 0x14:
		// set/get tempo
		// F0 00 01 74 10 14 dd dd cs F7
		//	where dd dd is the desired tempo as two 7 - bit MIDI bytes, LS first.
		//	To query the tempo let dd dd = 7F 7F.
		return;
// 		// preset loaded
// 		ReceivePresetNumber(&bytes[6], len - 6);
// 		DelayedNameSyncFromAxe(true);
// 		return;
// 	case 0x20:
// 		// routing grid layout
// 		return;
// 	case 0x21:
// 		// channel change or scene selected
// 		DelayedEffectsSyncFromAxe();
// 		return;
// 	case 0x29:
// 		// scene status/update
// 		ReceiveSceneStatus(&bytes[6], len - 6);
// 		return;
	case 0x64:
		// indicates an error or unsupported message
		if (kDbgFlag && mTrace)
		{
// 			if ((len - 5) > 2 && bytes[6] == 0x23)
// 			{
// 				// ignore looper status monitor ack.
// 				// the message to enable the monitor is required, the status messages are not 
// 				// otherwise sent; but no idea why 0x64 is sent as ack.
// 				break;
// 			}
// 			else
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
AxeFx3Manager::GetBlockInfoByEffectId(const byte * bytes)
{
	int effectId;
	const int effectIdLs = bytes[0] >> 3;
	const int effectIdMs = (bytes[1] << 4) & 0xff;
	effectId = effectIdMs | effectIdLs;

	for (auto & cur : mAxeEffectInfo)
	{
		if (cur.mSysexEffectId == effectId)
			return &cur;
	}

	return nullptr;
}

Axe3EffectBlockInfo *
AxeFx3Manager::GetBlockInfoByName(const std::string& normalizedEffectName)
{
	for (auto & cur : mAxeEffectInfo)
	{
		if (cur.mNormalizedName == normalizedEffectName)
			return &cur;
	}

	return nullptr;
}

void
AxeFx3Manager::SyncNameAndEffectsFromAxe()
{
	RequestPresetName();
}

void
AxeFx3Manager::SyncEffectsFromAxe()
{
	RequestStatusDump();
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

	for (auto & cur : mScenePatches)
		cur = nullptr;
}

void
AxeFx3Manager::DelayedNameSyncFromAxe(bool force /*= false*/)
{
	if (!mDelayedNameSyncTimer)
		return;

	if (!force)
	{
		// #axe3FinishThis -- probably incorrect for 3
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
	std::string name(::NormalizeAxe3EffectName(commandName));

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
	Axe3EffectBlockInfo *fx = GetBlockInfoByName(name);
	if (!fx)
	{
		if (-1 != name.find("looper"))
		{
			Bytes bb{ 0xF0, 0x00, 0x01, 0x74, byte(mModel), 0x0f };
			enum class AxeFx3LooperButton
			{
				Record = 0,
				Play,
				Undo,
				Once,
				Reverse,
				HalfSpeed,
				None
			};

			// send same button for both enable and disable?
			// it's a virtual button press, so press again to release?
			AxeFx3LooperButton b = AxeFx3LooperButton::None;
			if (-1 != name.find("rec"))
				b = AxeFx3LooperButton::Record;
			else if (-1 != name.find("play"))
				b = AxeFx3LooperButton::Play;
			else if (-1 != name.find("undo"))
				b = AxeFx3LooperButton::Undo;
			else if (-1 != name.find("once"))
				b = AxeFx3LooperButton::Once;
			else if (-1 != name.find("rev"))
				b = AxeFx3LooperButton::Reverse;
			else if (-1 != name.find("half"))
				b = AxeFx3LooperButton::HalfSpeed;
			else
				return emptyCommand;

			bb.push_back((int)b);
			AppendChecksumAndTerminate(bb);
			return bb;
		}

		return emptyCommand;
	}

	if (!fx->mSysexEffectId)
		return emptyCommand;

	// else, is an effect bypass command (command 0x0A)
	Bytes bytes{ 0xf0, 0x00, 0x01, 0x74, 0x10, 0x0A };
	bytes.push_back(fx->mSysexEffectIdLs);
	bytes.push_back(fx->mSysexEffectIdMs);
	bytes.push_back(enable ? 0 : 1);
	AppendChecksumAndTerminate(bytes);
	return bytes;
}

Bytes
AxeFx3Manager::GetSceneSelectCommandString(int scene)
{
	const Bytes emptyCommand;
	if (!scene || scene > AxeScenes)
		return emptyCommand;

	Bytes bytes{ 0xf0, 0x00, 0x01, 0x74, 0x10, 0x0c };
	bytes.push_back(scene - 1);
	AppendChecksumAndTerminate(bytes);
	return bytes;
}

Bytes
AxeFx3Manager::GetBlockChannelSelectCommandString(
	const std::string& effectBlockStr, 
	const std::string& channelStr, 
	int &effectId, 
	int &channel)
{
	const Bytes emptyCommand;
	effectId = channel = 0;
	if (effectBlockStr.empty())
		return emptyCommand;

	// get normalizedName
	std::string name(::NormalizeAxe3EffectName(effectBlockStr));

	// lookup name to get effect ID
	Axe3EffectBlockInfo *fx = GetBlockInfoByName(name);
	if (!fx)
		return emptyCommand;

	if (!fx->mSysexEffectId)
		return emptyCommand;

	effectId = fx->mSysexEffectId;
	channel = channelStr[0] - 'A';

	Bytes bytes{ 0xf0, 0x00, 0x01, 0x74, 0x10, 0x0B };
	bytes.push_back(fx->mSysexEffectIdLs);
	bytes.push_back(fx->mSysexEffectIdMs);
	bytes.push_back(channel);
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
	RequestLooperState();
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

	Bytes bb{ 0xF0, 0x00, 0x01, 0x74, byte(mModel), 0x0d, 0x7f, 0x7f };
	AppendChecksumAndTerminate(bb);
	mMidiOut->MidiOut(bb);
}

void
AxeFx3Manager::ReceivePresetName(const byte * bytes, int len)
{
	if (len < 32)
		return;

	std::string patchName((const char *)bytes, 32);
	if (mMainDisplay && !patchName.empty())
	{
		mCurrentAxePresetName = patchName;
		mCurrentAxeSceneName.clear();
// 		DisplayPresetStatus();
	}
}

void
AxeFx3Manager::RequestSceneName()
{
	if (!mFirmwareMajorVersion)
		SendFirmwareVersionQuery();

	QMutexLocker lock(&mQueryLock);
	if (!mFirmwareMajorVersion)
		return;

	Bytes bb{ 0xF0, 0x00, 0x01, 0x74, byte(mModel), 0x0e, 0x7f };
	AppendChecksumAndTerminate(bb);
	mMidiOut->MidiOut(bb);
}

void
AxeFx3Manager::ReceiveSceneName(const byte * bytes, int len)
{
	if (len < 32)
		return;

	std::string sceneName((const char *)bytes, 32);
	mCurrentAxeSceneName = sceneName;
}

void
AxeFx3Manager::RequestStatusDump()
{
	if (!mFirmwareMajorVersion)
		SendFirmwareVersionQuery();

	QMutexLocker lock(&mQueryLock);
	if (!mFirmwareMajorVersion)
		return;

	// default to no fx in patch, but don't update LEDs until later
	for (auto & cur : mAxeEffectInfo)
	{
		if (-1 == cur.mSysexEffectId)
			continue;

		if (!cur.mPatch)
			continue;

		cur.mEffectIsPresentInAxePatch = false;
	}

	Bytes bb{ 0xF0, 0x00, 0x01, 0x74, byte(mModel), 0x13 };
	AppendChecksumAndTerminate(bb);
	mMidiOut->MidiOut(bb);
}

void
AxeFx3Manager::ReceiveStatusDump(const byte * bytes, int len)
{
	// for each effect there is a packet of 3 bytes:
	// effect ID 2 bytes: LS MS
	// data 1 byte: 
	//	bit 0 bypass state: 0 = engaged, 1 = bypassed
	//	bit 1-3 channel (0-7)
	//	bit 6-4: max number of channels supported (0-7)
	constexpr int kEffectPacketLen = 3;
	for (int idx = 0; (idx + kEffectPacketLen) < len; idx += kEffectPacketLen)
	{
		if (false && kDbgFlag && mTrace)
		{
			const std::string byteDump(::GetAsciiHexStr(&bytes[idx], kEffectPacketLen, true) + "\n");
			mTrace->Trace(byteDump);
		}

		Axe3EffectBlockInfo * inf = GetBlockInfoByEffectId(bytes + idx);
		if (inf && mTrace /*&& inf->mNormalizedName != "feedback return"*/)
		{
			std::strstream traceMsg;
			traceMsg << "Axe sync warning: potentially unexpected sync for  " << inf->mName << " " << std::endl << std::ends;
			mTrace->Trace(std::string(traceMsg.str()));
		}

		if (inf && inf->mPatch)
		{
			const byte dd = bytes[idx + 2];
			inf->UpdateChannelStatus(mSwitchDisplay, (dd >> 4) & 0x7, (dd >> 1) & 0x7);
			inf->mPatch->UpdateState(mSwitchDisplay, !(dd & 0x1));
		}
		else
		{
			if (mTrace && !inf)
			{
				const std::string msg("Axe sync error: No inf for ");
				mTrace->Trace(msg);
				const std::string byteDump(::GetAsciiHexStr(&bytes[idx], kEffectPacketLen, true) + "\n");
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
		if (-1 == cur.mSysexEffectId)
			continue;

		if (!cur.mPatch)
			continue;

		if (!cur.mEffectIsPresentInAxePatch)
		{
			cur.mPatch->Disable(mSwitchDisplay);
			for (const auto &p : cur.mChannelSelectPatches)
				if (p)
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
	const int presetLs = bytes[0];
	const int presetMs = bytes[1] << 7;
	mCurrentAxePreset = presetMs | presetLs;
}

void
AxeFx3Manager::RequestLooperState()
{
	QMutexLocker lock(&mQueryLock);
	Bytes bb{ 0xF0, 0x00, 0x01, 0x74, byte(mModel), 0x0f, 0x7f };
	AppendChecksumAndTerminate(bb);
	mMidiOut->MidiOut(bb);
}

enum class AxeFx3LooperState
{ 
	// independent states
	loopStateRecord		= 1 << 0, 
	loopStatePlay		= 1 << 1,

	// state modifiers
	loopStateOverdub	= 1 << 2, 
	loopStatePlayOnce	= 1 << 3, 
	loopStateReverse	= 1 << 4, 
	loopStateHalfSpeed	= 1 << 5
};

static std::string
GetLooperStateDesc(int loopState)
{
	std::string stateStr;
	if (loopState & int(AxeFx3LooperState::loopStateRecord))
		stateStr.append("recording");
	else if (loopState & int(AxeFx3LooperState::loopStatePlay))
		stateStr.append("playing");
	else
		stateStr.append("stopped");

	if (loopState & int(AxeFx3LooperState::loopStatePlayOnce))
		stateStr.append(", once");
	if (loopState & int(AxeFx3LooperState::loopStateOverdub))
		stateStr.append(", overdub");
	if (loopState & int(AxeFx3LooperState::loopStateReverse))
		stateStr.append(", reverse");
	if (loopState & int(AxeFx3LooperState::loopStateHalfSpeed))
		stateStr.append(", 1/2 speed");

	return stateStr;
}

void
AxeFx3Manager::ReceiveLooperState(const byte * bytes, int len)
{
	if (len < 2)
		return;

	const int newLoopState = bytes[0];
	if (mLooperState == newLoopState)
		return;

	if ((mLooperState & int(AxeFx3LooperState::loopStateRecord)) != (newLoopState & int(AxeFx3LooperState::loopStateRecord)))
	{
		if (mLooperPatches[loopPatchRecord])
		{
			if ((mLooperState & int(AxeFx3LooperState::loopStateRecord)) && !(newLoopState & int(AxeFx3LooperState::loopStateRecord)))
				mLooperPatches[loopPatchRecord]->UpdateState(mSwitchDisplay, false);
			else
				mLooperPatches[loopPatchRecord]->UpdateState(mSwitchDisplay, true);
		}
	}

	if ((mLooperState & int(AxeFx3LooperState::loopStatePlay)) != (newLoopState & int(AxeFx3LooperState::loopStatePlay)))
	{
		if (mLooperPatches[loopPatchPlay])
		{
			if ((mLooperState & int(AxeFx3LooperState::loopStatePlay)) && !(newLoopState & int(AxeFx3LooperState::loopStatePlay)))
				mLooperPatches[loopPatchPlay]->UpdateState(mSwitchDisplay, false);
			else
				mLooperPatches[loopPatchPlay]->UpdateState(mSwitchDisplay, true);
		}
	}

	if ((mLooperState & int(AxeFx3LooperState::loopStatePlayOnce)) != (newLoopState & int(AxeFx3LooperState::loopStatePlayOnce)))
	{
		if (mLooperPatches[loopPatchPlayOnce])
		{
			if ((mLooperState & int(AxeFx3LooperState::loopStatePlayOnce)) && !(newLoopState & int(AxeFx3LooperState::loopStatePlayOnce)))
				mLooperPatches[loopPatchPlayOnce]->UpdateState(mSwitchDisplay, false);
			else
				mLooperPatches[loopPatchPlayOnce]->UpdateState(mSwitchDisplay, true);
		}
	}

	if ((mLooperState & int(AxeFx3LooperState::loopStateReverse)) != (newLoopState & int(AxeFx3LooperState::loopStateReverse)))
	{
		if (mLooperPatches[loopPatchReverse])
		{
			if ((mLooperState & int(AxeFx3LooperState::loopStateReverse)) && !(newLoopState & int(AxeFx3LooperState::loopStateReverse)))
				mLooperPatches[loopPatchReverse]->UpdateState(mSwitchDisplay, false);
			else
				mLooperPatches[loopPatchReverse]->UpdateState(mSwitchDisplay, true);
		}
	}

	if ((mLooperState & int(AxeFx3LooperState::loopStateHalfSpeed)) != (newLoopState & int(AxeFx3LooperState::loopStateHalfSpeed)))
	{
		if (mLooperPatches[loopPatchHalf])
		{
			if ((mLooperState & int(AxeFx3LooperState::loopStateHalfSpeed)) && !(newLoopState & int(AxeFx3LooperState::loopStateHalfSpeed)))
				mLooperPatches[loopPatchHalf]->UpdateState(mSwitchDisplay, false);
			else
				mLooperPatches[loopPatchHalf]->UpdateState(mSwitchDisplay, true);
		}
	}

	mLooperState = newLoopState;

	if (mMainDisplay)
	{
		std::strstream traceMsg;
		traceMsg << "Axe-Fx looper: " << GetLooperStateDesc(mLooperState) << std::endl << std::ends;
		mMainDisplay->TransientTextOut(std::string(traceMsg.str()));
	}
}

bool
AxeFx3Manager::SetLooperPatch(PatchPtr patch)
{
	std::string name(::NormalizeAxe3EffectName(patch->GetName()));
	LoopPatchIdx idx = loopPatchCnt;
	if (-1 != name.find("looper record"))
		idx = loopPatchRecord;
	else if (-1 != name.find("looper play"))
		idx = loopPatchPlay;
	else if (-1 != name.find("looper once"))
		idx = loopPatchPlayOnce;
	else if (-1 != name.find("looper undo"))
		idx = loopPatchUndo;
	else if (-1 != name.find("looper reverse"))
		idx = loopPatchReverse;
	else if (-1 != name.find("looper half"))
		idx = loopPatchHalf;
	else
	{
// 		if (mTrace)
// 		{
// 			std::string msg("Warning: unknown looper patch\n");
// 			mTrace->Trace(msg);
// 		}
		return false;
	}

	if (mLooperPatches[idx] && mTrace)
	{
		std::string msg("Warning: multiple Axe-Fx patches for same looper command\n");
		mTrace->Trace(msg);
	}

	mLooperPatches[idx] = patch;
	return true;
}

void
AxeFx3Manager::ReceiveSceneStatus(const byte * bytes, int len)
{
	if (len < 1)
		return;

	int newScene = bytes[0];
	UpdateSceneStatus(newScene, false);
}

void
AxeFx3Manager::UpdateSceneStatus(int newScene, bool internalUpdate)
{
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

	if (mCurrentScene > -1 && mScenePatches[mCurrentScene])
		mScenePatches[mCurrentScene]->UpdateState(mSwitchDisplay, false);

	mCurrentScene = newScene;

	if (mScenePatches[mCurrentScene])
		mScenePatches[mCurrentScene]->UpdateState(mSwitchDisplay, true);

	if (internalUpdate)
		RequestSceneName();

// 	if (mCurrentAxePresetName.empty())
// 		RequestPresetName();
// 	else
// 		DisplayPresetStatus();
}

void
AxeFx3Manager::DisplayPresetStatus()
{
	if (!mMainDisplay)
		return;

	const std::string kPrefix("AxeFx pre: ");
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
		curText += std::string("\nAxeFx scn: ") + sceneBuf;
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
		{ FractalAudio::AxeFx3::ID_LOOPER1, "Looper" },
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

std::string
NormalizeAxe3EffectName(const std::string &effectNameIn)
{
	std::string effectName(effectNameIn);
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
	return effectName;
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
		MapName("loop 1 rec", "looper record");
		MapName("loop 1 record", "looper record");
		MapName("loop 1 play", "looper play");
		MapName("loop 1 once", "looper once");
		MapName("loop 1 overdub", "looper overdub");
		MapName("loop 1 rev", "looper reverse");
		MapName("loop 1 reverse", "looper reverse");
		MapName("looper 1 rec", "looper record");
		MapName("looper 1 record", "looper record");
		MapName("looper 1 play", "looper play");
		MapName("looper 1 once", "looper once");
		MapName("looper 1 overdub", "looper overdub");
		MapName("looper 1 rev", "looper reverse");
		MapName("looper 1 reverse", "looper reverse");
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
		MapName("looper half-speed", "looper half");
		MapName("looper half speed", "looper half");
		MapName("looper 1 half-speed", "looper half");
		MapName("looper 1 half speed", "looper half");
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
		MapName("multi-delay 1", "multidelay 1");
		MapName("multi-delay 2", "multidelay 2");
		MapName("multi-delay 3", "multidelay 3");
		MapName("multi-delay 4", "multidelay 4");
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
