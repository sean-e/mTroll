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

#ifndef MetaPatch_SyncAxeFx_h__
#define MetaPatch_SyncAxeFx_h__

#include "Patch.h"
#include "AxeFxManager.h"


class MetaPatch_SyncAxeFx : public Patch
{
public:
	MetaPatch_SyncAxeFx(AxeFxManager * axe, int number, const std::string & name) : 
		Patch(number, name),
		mAxe(axe)
	{
		_ASSERTE(mAxe);
		if (mAxe)
			mAxe->AddRef();
	}

	~MetaPatch_SyncAxeFx()
	{
		if (mAxe)
			mAxe->Release();
	}

	virtual std::string GetPatchTypeStr() const {return "meta: syncAxeFx";}

	virtual void SwitchPressed(IMainDisplay *, ISwitchDisplay *)
	{
		if (mAxe)
			mAxe->SyncFromAxe();
	}

	virtual void BankTransitionActivation() {SwitchPressed(NULL, NULL);}
	virtual void BankTransitionDeactivation() {SwitchPressed(NULL, NULL);}

private:
	AxeFxManager	* mAxe;
};

#endif // MetaPatch_SyncAxeFx_h__
