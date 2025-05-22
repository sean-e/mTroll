/*
 * mTroll MIDI Controller
 * Copyright (C) 2020-2021,2025 Sean Echevarria
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

#ifndef AxeFx3Manager_h__
#define AxeFx3Manager_h__

#include <QObject>
#include <qmutex.h>
#include <time.h>
#include <set>
#include <memory>
#include "IMidiInSubscriber.h"
#include "AxemlLoader.h"
#include "IAxeFx.h"
#include "HexStringUtils.h"
#include "MidiControlEngine.h"

class IMainDisplay;
class ITraceDisplay;
class ISwitchDisplay;
class Patch;
class IMidiOut;
class QTimer;
class AxeFx3Manager;
struct Axe3EffectBlockInfo;

using PatchPtr = std::shared_ptr<Patch>;
using AxeFx3ManagerPtr = std::shared_ptr<AxeFx3Manager>;
using IMidiOutPtr = std::shared_ptr<IMidiOut>;
using Axe3EffectBlocks = std::vector<Axe3EffectBlockInfo>;

// AxeFx3Manager
// ----------------------------------------------------------------------------
// Manages extended Axe-Fx III  support
//
class AxeFx3Manager :
	public QObject,
	public IMidiInSubscriber,
	public IAxeFx
{
	Q_OBJECT;
	friend class StartQueryTimer;
public:
	AxeFx3Manager(IMainDisplay * mainDisp, ISwitchDisplay * switchDisp, ITraceDisplay * pTrace, const std::string & appPath, int ch, AxeFxModel m);
	virtual ~AxeFx3Manager();

	// IMidiInSubscriber
	virtual void ReceivedData(byte b1, byte b2, byte b3) override;
	virtual bool ReceivedSysex(const byte * bytes, int len) override;
	virtual void Closed(IMidiInPtr midIn) override;

	void CompleteInit(MidiControlEnginePtr eng, IMidiOutPtr midiOut);
	void SubscribeToMidiIn(IMidiInPtr midiIn);

	// IAxeFx
	void SetTempoPatch(PatchPtr patch) override;
	void SetScenePatch(int scene, PatchPtr patch) override;
	bool SetLooperPatch(PatchPtr patch) override;
	bool SetSyncPatch(PatchPtr patch, int bypassCc) override;
	bool SetSyncPatch(PatchPtr patch, int effectId, int channel) override;
	int GetChannel() const override { return mAxeChannel; }
	AxeFxModel GetModel() const override { return mModel; }
	void ReloadCurrentPreset() override;
	void IncrementPreset() override;
	void DecrementPreset() override;
	void IncrementScene() override;
	void DecrementScene() override;
	void Shutdown() override;
	void UpdateSceneStatus(int newScene, bool internalUpdate) override;
	void ForceRefreshAxeState() override;

	// delayed requests for sync
	void DelayedNameSyncFromAxe(bool force = false) override;
	void DelayedEffectsSyncFromAxe() override;
	void DelayedLooperSyncFromAxe();

	Bytes GetCommandString(const std::string& commandName, bool enable);
	Bytes GetSceneSelectCommandString(int scene);
	Bytes GetBlockChannelSelectCommandString(const std::string& effectBlockStr, const std::string& channelStr, int &effectId, int &channel);

public slots:
	// immediate requests for sync (called by the delayed requests)
	void SyncNameAndEffectsFromAxe();
	void SyncEffectsFromAxe();
	void SyncLooperFromAxe();
	void PollingSyncTimerFired();

private:
	// basically an overload of IMidiInSubscriber::shared_from_this() but returning 
	// AxeFx3ManagerPtr instead of IMidiInSubscriberPtr
	AxeFx3ManagerPtr GetSharedThis()
	{
		return std::dynamic_pointer_cast<AxeFx3Manager>(IMidiInSubscriber::shared_from_this());
	}

	void LoadEffectPool();
	Axe3EffectBlockInfo* GetBlockInfoByEffectId(const byte * bytes);
	Axe3EffectBlockInfo* GetBlockInfoByName(const std::string& normalizedEffectName);
	void SendFirmwareVersionQuery();
	void ReceiveFirmwareVersionResponse(const byte * bytes, int len);

	void RequestLooperState();
	void ReceiveLooperState(byte loopState);
	void ResetLooperState();
	void ReceivePresetNumber(const byte * bytes, int len);
	void ReceiveSceneStatus(const byte * bytes, int len);

	void RequestPresetName(bool silentRequest = false);
	void ReceivePresetName(const byte * bytes, int len);
	static constexpr byte kQueryCurrentScene = 0x7f;
	void RequestSceneName(int sceneNumber = kQueryCurrentScene);
	void ReceiveSceneName(const byte * bytes, int len);
	void ReceiveSceneName(int sceneNumber, const byte * bytes, int len);
	void DisplayPresetStatus();

	void RequestStatusDump();
	void ReceiveStatusDump(const byte * bytes, int len);
	void RequestProgramChange(int offset);
	void RequestSceneChange(int offset);
	void TurnOffLedsForNaEffects();

	static void AppendChecksumAndTerminate(Bytes &data);

private:
	int				mAxeChannel;
	MidiControlEnginePtr mEngine;
	IMainDisplay	* mMainDisplay;
	ITraceDisplay	* mTrace;
	ISwitchDisplay	* mSwitchDisplay;
	IMidiOutPtr		mMidiOut;
	PatchPtr		mTempoPatch;
	enum { AxeScenes = 8 };
	PatchPtr		mScenePatches[AxeScenes];
	std::string		mOriginalScenePresetNames[AxeScenes];
	enum LoopPatchIdx { loopPatchRecord, loopPatchPlay, loopPatchPlayOnce, loopPatchUndo, loopPatchReverse, loopPatchHalf, loopPatchCnt };
	PatchPtr		mLooperPatches[loopPatchCnt];
	Axe3EffectBlocks mAxeEffectInfo;
	QMutex			mQueryLock;
	QTimer			* mDelayedNameSyncTimer;
	QTimer			* mDelayedEffectsSyncTimer;
	QTimer			* mDelayedLooperSyncTimer;
	QTimer			* mPollingSyncTimer;
	clock_t			mLastTimeout = 0;
	int				mFirmwareMajorVersion = 0;
	AxeFxModel		mModel;
	int				mLooperState = 0;
	int				mCurrentScene = -1;
	int				mCurrentAxePreset = -1;
	std::string		mCurrentAxePresetName;
	std::string		mCurrentAxeSceneName;
	std::atomic_int	mPendingPresetRequests = 0;
	int				mLooperBlockIsPresent = -1;
	bool			mLooperStatusRequested = false;
	bool			mLooperOnceIsRunning = false;

	// state for chained scene name requests that occur when presets change
	int				mSceneNameRequestIdx = -1;
	int				mCurrentAxePresetWhenSceneNamesRequested = -1;
	std::string		mCurrentAxePresetNameWhenSceneNamesRequested;
};

enum class AxeFx3MessageIds
{
	EditorSyncMsg = 0x01,

	FirmwareVersion = 0x08,

	EffectBypass = 0x0a,
	EffectChannel = 0x0b,
	Scene = 0x0c,
	PresetName = 0x0d,
	SceneName = 0x0e,
	LooperState = 0x0f,
	TapTempo = 0x10,
	Tuner = 0x11,
	EditorTunerMsg = 0x12,
	StatusDump = 0x13,
	Tempo = 0x14,

	EditorSyncMsg2 = 0x47,

	Ack = 0x64,

	EditorAmpMsg1 = 0x74,			// Editor command: click on amp
	EditorAmpMsg2 = 0x75,			// Editor command: click on amp
	EditorAmpMsg3 = 0x76,			// Editor command: click on amp
	PresetExportRequestAck = 0x77,	// Editor command: Export preset
	PresetExportResponse = 0x78,	// Editor command: Export preset
	PresetExportResponse2 = 0x79	// Editor command: Export preset
};

#endif // AxeFx3Manager_h__
