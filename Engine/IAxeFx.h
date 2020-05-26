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

#ifndef IAxeFx_h__
#define IAxeFx_h__

#include <memory>
#include "AxeFxModel.h"

class Patch;
using PatchPtr = std::shared_ptr<Patch>;

__interface IAxeFx
{
	// info
	virtual AxeFxModel GetModel() const;
	virtual int GetChannel() const;

	// engine load init
	virtual void SetScenePatch(int scene, PatchPtr patch);
	virtual void SetTempoPatch(PatchPtr patch);
	virtual void SetLooperPatch(PatchPtr patch);
	virtual bool SetSyncPatch(PatchPtr patch, int bypassCc = -1);

	// runtime
	virtual void SyncNameAndEffectsFromAxe();
	virtual void DelayedNameSyncFromAxe(bool force = false);
	virtual void DelayedEffectsSyncFromAxe();
	virtual void Shutdown();
};

using IAxeFxPtr = std::shared_ptr<IAxeFx>;

#endif // IAxeFx_h__