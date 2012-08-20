/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2008,2012 Sean Echevarria
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

#ifndef MetaPatch_BankHistory_h__
#define MetaPatch_BankHistory_h__

#include "Patch.h"
#include "MidiControlEngine.h"


class MetaPatch_BankHistoryBackward : public Patch
{
public:
	MetaPatch_BankHistoryBackward(MidiControlEngine * engine, int number, const std::string & name) : 
		Patch(number, name),
		mEngine(engine)
	{
		_ASSERTE(mEngine);
	}

	virtual std::string GetPatchTypeStr() const {return "meta: BankHistoryBackward";}

	virtual void SwitchPressed(IMainDisplay *, ISwitchDisplay *)
	{
		// handle bank change on release instead of press so that 
		// release does not get handled by new patchbank
		// (noted after secondary function support implemented)
	}

	virtual void SwitchReleased(IMainDisplay *, ISwitchDisplay *)
	{
		mEngine->HistoryBackward();
	}

	virtual bool UpdateMainDisplayOnPress() const {return false;}

	virtual void BankTransitionActivation() {SwitchReleased(NULL, NULL);}
	virtual void BankTransitionDeactivation() {SwitchReleased(NULL, NULL);}

private:
	MidiControlEngine	* mEngine;
};


class MetaPatch_BankHistoryForward : public Patch
{
public:
	MetaPatch_BankHistoryForward(MidiControlEngine * engine, int number, const std::string & name) : 
		Patch(number, name),
		mEngine(engine)
	{
		_ASSERTE(mEngine);
	}

	virtual std::string GetPatchTypeStr() const {return "meta: BankHistoryForward";}

	virtual void SwitchPressed(IMainDisplay *, ISwitchDisplay *)
	{
		// handle bank change on release instead of press so that 
		// release does not get handled by new patchbank
		// (noted after secondary function support implemented)
	}

	virtual void SwitchReleased(IMainDisplay *, ISwitchDisplay *)
	{
		mEngine->HistoryForward();
	}

	virtual bool UpdateMainDisplayOnPress() const {return false;}

	virtual void BankTransitionActivation() {SwitchReleased(NULL, NULL);}
	virtual void BankTransitionDeactivation() {SwitchReleased(NULL, NULL);}

private:
	MidiControlEngine	* mEngine;
};


class MetaPatch_BankHistoryRecall : public Patch
{
public:
	MetaPatch_BankHistoryRecall(MidiControlEngine * engine, int number, const std::string & name) : 
		Patch(number, name),
		mEngine(engine)
	{
		_ASSERTE(mEngine);
	}

	virtual std::string GetPatchTypeStr() const {return "meta: BankHistoryRecall";}

	virtual void SwitchPressed(IMainDisplay *, ISwitchDisplay *)
	{
		// handle bank change on release instead of press so that 
		// release does not get handled by new patchbank
		// (noted after secondary function support implemented)
	}

	virtual void SwitchReleased(IMainDisplay *, ISwitchDisplay *)
	{
		mEngine->HistoryRecall();
	}

	virtual bool UpdateMainDisplayOnPress() const {return false;}

	virtual void BankTransitionActivation() {SwitchReleased(NULL, NULL);}
	virtual void BankTransitionDeactivation() {SwitchReleased(NULL, NULL);}

private:
	MidiControlEngine	* mEngine;
};


#endif // MetaPatch_BankHistory_h__
