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
	AxeFxManager(ISwitchDisplay * switchDisp, ITraceDisplay * pTrace);
	virtual ~AxeFxManager();

	// IMidiInSubscriber
	virtual void ReceivedData(byte b1, byte b2, byte b3);
	virtual void ReceivedSysex(byte * bytes, int len);
	virtual void Closed(IMidiIn * midIn);

	void AddRef();
	void Release();

	void SetTempoPatch(Patch * patch) { mTempoPatch = patch; }

private:
	int				mRefCnt;
	ITraceDisplay	* mTrace;
	ISwitchDisplay	* mSwitchDisplay;
	Patch			* mTempoPatch;
	// TODO: need tempo patch (tempo attribute or tempo patch type?)
	// need list of IA patches
	// need queue (and lock) for outgoing queries
	// need timer for timeout on query response
	// need timer for tempo indicator
	// axefx patch type
	//		poll after program changes on axe ch?
	//		http://www.fractalaudio.com/forum/viewtopic.php?f=14&t=21524&start=10

};

#endif // AxeFxManager_h__
