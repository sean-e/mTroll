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

#include "IMidiInSubscriber.h"
#include "AxemlLoader.h"

class ITraceDisplay;
class ISwitchDisplay;
class Patch;
typedef unsigned char byte;


// AxeFxManager
// ----------------------------------------------------------------------------
// Manages extended Axe-Fx support
//
class AxeFxManager : public IMidiInSubscriber
{
public:
	AxeFxManager(ISwitchDisplay * switchDisp, ITraceDisplay * pTrace, std::string appPath);
	virtual ~AxeFxManager();

	// IMidiInSubscriber
	virtual void ReceivedData(byte b1, byte b2, byte b3);
	virtual void ReceivedSysex(const byte * bytes, int len);
	virtual void Closed(IMidiIn * midIn);

	void AddRef();
	void Release();

	void CompleteInit();
	void SetTempoPatch(Patch * patch);
	void SetSyncPatch(const std::string &effectName, Patch * patch);

private:
	void ReceiveParamValue(const byte * bytes, int len);

private:
	int				mRefCnt;
	ITraceDisplay	* mTrace;
	ISwitchDisplay	* mSwitchDisplay;
	Patch			* mTempoPatch;
	AxeEffectBlocks	mAxeEffectInfo;
	// TODO: 
	// what indexes are needed here?
	// axefx patch type or attribute?
	// need queue (and lock) for outgoing queries
	// need timer for timeout on query response
	//		poll after program changes on axe ch?
	//		http://www.fractalaudio.com/forum/viewtopic.php?f=14&t=21524&start=10
	// difference between disabled and not present?
	// consider mapping midi port to device name so not necessary to do so for every patch (like default ch)
};

int GetDefaultAxeCc(const std::string &effectName, ITraceDisplay * trc);

#endif // AxeFxManager_h__
