/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2008 Sean Echevarria
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

#ifndef MetaPatch_BankNav_h__
#define MetaPatch_BankNav_h__

#include "Patch.h"
#include "MidiControlEngine.h"


class MetaPatch_BankNavNext : public Patch
{
public:
	MetaPatch_BankNavNext(MidiControlEngine * engine, int number, const std::string & name) : 
		Patch(number, name),
		mEngine(engine)
	{
		_ASSERTE(mEngine);
	}

	virtual std::string GetPatchTypeStr() const {return "meta: BankNavNext";}

	virtual void SwitchPressed(IMainDisplay *, ISwitchDisplay *)
	{
		mEngine->EnterNavMode();
	}

	virtual bool UpdateMainDisplayOnPress() const {return false;}

	virtual void BankTransitionActivation() {}
	virtual void BankTransitionDeactivation() {}

private:
	MidiControlEngine	* mEngine;
};


class MetaPatch_BankNavPrevious : public Patch
{
public:
	MetaPatch_BankNavPrevious(MidiControlEngine * engine, int number, const std::string & name) : 
		Patch(number, name),
		mEngine(engine)
	{
		_ASSERTE(mEngine);
	}

	virtual std::string GetPatchTypeStr() const {return "meta: BankNavPrevious";}

	virtual void SwitchPressed(IMainDisplay *, ISwitchDisplay *)
	{
		mEngine->EnterNavMode();
	}

	virtual bool UpdateMainDisplayOnPress() const {return false;}

	virtual void BankTransitionActivation() {}
	virtual void BankTransitionDeactivation() {}

private:
	MidiControlEngine	* mEngine;
};

#endif // MetaPatch_BankNav_h__
