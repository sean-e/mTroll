/*
 * mTroll MIDI Controller
 * Copyright (C) 2010-2014,2018,2020 Sean Echevarria
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

#ifndef AxeFxManager_h__
#define AxeFxManager_h__

#include <QObject>
#include <qmutex.h>
#include <time.h>
#include <set>
#include <memory>
#include "IMidiInSubscriber.h"
#include "AxemlLoader.h"
#include "IAxeFx.h"

class IMainDisplay;
class ITraceDisplay;
class ISwitchDisplay;
class Patch;
class IMidiOut;
class QTimer;
class AxeFxManager;

using PatchPtr = std::shared_ptr<Patch>;
using AxeFxManagerPtr = std::shared_ptr<AxeFxManager>;
using IMidiOutPtr = std::shared_ptr<IMidiOut>;

// AxeFxManager
// ----------------------------------------------------------------------------
// Manages extended Axe-Fx support
//
class AxeFxManager : 
	public QObject, 
	public IMidiInSubscriber,
	public IAxeFx
{
	Q_OBJECT;
	friend class StartQueryTimer;
public:
	AxeFxManager(IMainDisplay * mainDisp, ISwitchDisplay * switchDisp, ITraceDisplay * pTrace, const std::string & appPath, int ch, AxeFxModel m);
	virtual ~AxeFxManager();

	// IMidiInSubscriber
	virtual void ReceivedData(byte b1, byte b2, byte b3) override;
	virtual void ReceivedSysex(const byte * bytes, int len) override;
	virtual void Closed(IMidiInPtr midIn) override;

	void CompleteInit(IMidiOutPtr midiOut);
	void SubscribeToMidiIn(IMidiInPtr midiIn);

	// IAxeFx
	void SetTempoPatch(PatchPtr patch) override;
	void SetScenePatch(int scene, PatchPtr patch) override;
	bool SetLooperPatch(PatchPtr patch) override;
	bool SetSyncPatch(PatchPtr patch, int bypassCc = -1) override;
	bool SetSyncPatch(PatchPtr patch, int effectId, int channel) override;
	int GetChannel() const override { return mAxeChannel; }
	void SyncPatchFromAxe(PatchPtr patch);
	AxeFxModel GetModel() const override { return mModel; }
	void Shutdown() override;
	void UpdateSceneStatus(int newScene, bool internalUpdate) override;
	void ForceRefreshAxeState() override;

	// delayed requests for sync
	void DelayedNameSyncFromAxe(bool force = false) override;
	void DelayedEffectsSyncFromAxe() override;

	int GetDefaultAxeCc(const std::string &effectName);

public slots:
	// immediate requests for sync (called by the delayed requests)
	void SyncNameAndEffectsFromAxe();
	void SyncEffectsFromAxe();

private:
	// basically an overload of IMidiInSubscriber::shared_from_this() but returning 
	// AxeFxManagerPtr instead of IMidiInSubscriberPtr
	AxeFxManagerPtr GetSharedThis()
	{
		return std::dynamic_pointer_cast<AxeFxManager>(IMidiInSubscriber::shared_from_this());
	}

	AxeEffectBlockInfo * IdentifyBlockInfoUsingBypassId(const byte * bytes);
	AxeEffectBlockInfo * IdentifyBlockInfoUsingCc(const byte * bytes);
	AxeEffectBlockInfo * IdentifyBlockInfoUsingEffectId(const byte * bytes);
	AxeEffectBlocks::iterator GetBlockInfo(PatchPtr patch);
	void SendFirmwareVersionQuery();
	void ReceiveFirmwareVersionResponse(const byte * bytes, int len);

	void EnableLooperStatusMonitor(bool enable);
	void ReceiveLooperStatus(const byte * bytes, int len);
	void ReceivePresetNumber(const byte * bytes, int len);
	void ReceiveSceneStatus(const byte * bytes, int len);

	void RequestPresetName();
	void ReceivePresetName(const byte * bytes, int len);
	void DisplayPresetStatus();

	void RequestPresetEffects();
	void ReceivePresetEffects(const byte * bytes, int len);
	void ReceivePresetEffectsV2(const byte * bytes, int len);
	void TurnOffLedsForNaEffects();

	void RequestNextParamValue();
	void ReceiveParamValue(const byte * bytes, int len);
	void KillResponseTimer();

private slots:
	void QueryTimedOut();

private:
	int				mAxeChannel;
	IMainDisplay	* mMainDisplay;
	ITraceDisplay	* mTrace;
	ISwitchDisplay	* mSwitchDisplay;
	IMidiOutPtr		mMidiOut;
	PatchPtr		mTempoPatch;
	enum { AxeScenes = 8 };
	PatchPtr		mScenes[AxeScenes];
	enum LoopPatchIdx { loopPatchRecord, loopPatchPlay, loopPatchPlayOnce, loopPatchUndo, loopPatchOverdub, loopPatchReverse, loopPatchHalf, loopPatchCnt };
	PatchPtr		mLooperPatches[loopPatchCnt];
	AxeEffectBlocks	mAxeEffectInfo;
	QMutex			mQueryLock;
	std::list<AxeEffectBlockInfo *> mQueries;
	QTimer			* mQueryTimer;
	QTimer			* mDelayedNameSyncTimer;
	QTimer			* mDelayedEffectsSyncTimer;
	int				mTimeoutCnt;
	clock_t			mLastTimeout;
	int				mFirmwareMajorVersion;
	AxeFxModel		mModel;
	std::set<int>	mEditBufferEffectBlocks; // at last update
	int				mLooperState;
	int				mCurrentScene;
	int				mCurrentAxePreset;
	std::string		mCurrentAxePresetName;
};

int GetDefaultAxeCc(const std::string &effectName, ITraceDisplay * trc);
void NormalizeAxeEffectName(std::string & effectName);

#endif // AxeFxManager_h__
