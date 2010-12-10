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

#ifndef AxeFxManager_h__
#define AxeFxManager_h__

#include <QObject>
#include <qmutex.h>
#include <time.h>
#include <set>
#include "IMidiInSubscriber.h"
#include "AxemlLoader.h"

class ITraceDisplay;
class ISwitchDisplay;
class Patch;
class IMidiOut;
class QTimer;


// AxeFxManager
// ----------------------------------------------------------------------------
// Manages extended Axe-Fx support
//
class AxeFxManager : public QObject, public IMidiInSubscriber
{
	Q_OBJECT;
	friend class StartQueryTimer;
public:
	AxeFxManager(ISwitchDisplay * switchDisp, ITraceDisplay * pTrace, const std::string & appPath);
	virtual ~AxeFxManager();

	// IMidiInSubscriber
	virtual void ReceivedData(byte b1, byte b2, byte b3);
	virtual void ReceivedSysex(const byte * bytes, int len);
	virtual void Closed(IMidiIn * midIn);

	void AddRef();
	void Release();

	void CompleteInit(IMidiOut * midiOut);
	void SetTempoPatch(Patch * patch);
	bool SetSyncPatch(Patch * patch);
	void SyncFromAxe(Patch * patch);
	void SyncAllFromAxe();

private:
	AxeEffectBlockInfo * IdentifyBlockInfo(const byte * bytes);
	AxeEffectBlocks::iterator GetBlockInfo(Patch * patch);
	void ReceiveParamValue(const byte * bytes, int len);
	void StartReceivePatchDump(const byte * bytes, int len);
	void ContinueReceivePatchDump(const byte * bytes, int len);
	void InitiateSyncFromAxe();
	void RequestEditBufferDump();
	void SendNextQuery();
	void SendFirmwareVersionQuery();
	void KillResponseTimer();

private slots:
	void QueryTimedOut();

private:
	int				mRefCnt;
	ITraceDisplay	* mTrace;
	ISwitchDisplay	* mSwitchDisplay;
	IMidiOut		* mMidiOut;
	Patch			* mTempoPatch;
	AxeEffectBlocks	mAxeEffectInfo;
	QMutex			mQueryLock;
	std::list<AxeEffectBlockInfo *> mQueries;
	QTimer			* mQueryTimer;
	int				mTimeoutCnt;
	clock_t			mLastTimeout;
	bool			mCheckedFirmware;
	byte			mModel;
	std::set<int>	mEditBufferEffectBlocks; // at last update
	int				mPatchDumpBytesReceived;
};

int GetDefaultAxeCc(const std::string &effectName, ITraceDisplay * trc);
void NormalizeAxeEffectName(std::string & effectName);

#endif // AxeFxManager_h__
