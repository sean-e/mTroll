/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2008,2010,2012,2015,2018,2021,2025 Sean Echevarria
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

#ifndef MetaPatch_LoadBank_h__
#define MetaPatch_LoadBank_h__

#include <format>
#include "Patch.h"
#include "MidiControlEngine.h"


class MetaPatch_LoadBank : public Patch
{
public:
	MetaPatch_LoadBank(MidiControlEngine * engine, int number, const std::string & name, int bankNumber) : 
		Patch(number, name),
		mEngine(engine),
		mTargetBankNumber(bankNumber)
	{
		_ASSERTE(mEngine);
	}

	MetaPatch_LoadBank(MidiControlEngine * engine, int number, const std::string & name, const std::string &targetBankName) :
		Patch(number, name),
		mEngine(engine),
		mOptionalTargetBankName(targetBankName)
	{
		_ASSERTE(mEngine);
	}

	virtual std::string GetPatchTypeStr() const override {return "meta: LoadBank";}

	virtual void SwitchPressed(IMainDisplay *, ISwitchDisplay *) override
	{
		// handle bank change on release instead of press so that 
		// release does not get handled by new patchbank
		// (noted after secondary function support implemented)
	}

	virtual void SwitchReleased(IMainDisplay *, ISwitchDisplay *) override
	{
		mEngine->LoadBankByNumber(mTargetBankNumber);
	}

	virtual bool UpdateMainDisplayOnPress() const override {return false;}

	virtual void BankTransitionActivation() override {SwitchReleased(nullptr, nullptr);}
	virtual void BankTransitionDeactivation() override {SwitchReleased(nullptr, nullptr);}

	virtual void CompleteInit(MidiControlEngine * eng, ITraceDisplay * trc) override
	{
		Patch::CompleteInit(eng, trc);

		if (!mOptionalTargetBankName.empty())
		{
			mTargetBankNumber = mEngine->GetBankNumber(mOptionalTargetBankName);
			if (-1 == mTargetBankNumber && trc)
				trc->Trace(std::format("Error: failed to identify bank referenced by LoadBank command; bank name {}\n", mOptionalTargetBankName));
		}

		std::string name(GetName());
		if (name == "meta load bank")
		{
			// update name of auto-gen'd LoadBank patches
			const int bankNum = GetBankNumber();
			std::string bankName = eng->GetBankNameByNum(bankNum);
			if (!bankName.empty())
			{
				name = "[" + bankName + "]";
				SetName(name);
			}
			else if (trc)
				trc->Trace(std::format("Error: failed to identify name of bank referenced by LoadBank command; bank number {}\n", bankNum));
		}
	}

	int GetBankNumber() const { return mTargetBankNumber; }

private:
	MidiControlEngine	*mEngine;
	int					mTargetBankNumber = -1;
	std::string			mOptionalTargetBankName;
};


class MetaPatch_LoadBankRelative : public Patch
{
private:
	MetaPatch_LoadBankRelative();

protected:
	MetaPatch_LoadBankRelative(MidiControlEngine * engine, int number, const std::string & name, int steps) : 
		Patch(number, name),
		mEngine(engine),
		mSteps(steps)
	{
		_ASSERTE(mEngine);
	}

public:
	virtual void SwitchPressed(IMainDisplay *, ISwitchDisplay *) override
	{
		// handle bank change on release instead of press so that 
		// release does not get handled by new patchbank
		// (noted after secondary function support implemented)
	}

	virtual void SwitchReleased(IMainDisplay *, ISwitchDisplay *) override
	{
		mEngine->LoadBankRelative(mSteps);
	}

	virtual bool UpdateMainDisplayOnPress() const override {return false;}

	virtual void BankTransitionActivation() override {SwitchReleased(nullptr, nullptr);}
	virtual void BankTransitionDeactivation() override {SwitchReleased(nullptr, nullptr);}

private:
	MidiControlEngine	* mEngine;
	int					mSteps;
};

class MetaPatch_LoadNextBank : public MetaPatch_LoadBankRelative
{
public:
	MetaPatch_LoadNextBank(MidiControlEngine * engine, int number, const std::string & name) : 
		MetaPatch_LoadBankRelative(engine, number, name, 1)
	{ }

	virtual std::string GetPatchTypeStr() const override {return "meta: LoadNextBank";}

};

class MetaPatch_LoadPreviousBank : public MetaPatch_LoadBankRelative
{
public:
	MetaPatch_LoadPreviousBank(MidiControlEngine * engine, int number, const std::string & name) : 
		MetaPatch_LoadBankRelative(engine, number, name, -1)
	{ }

	virtual std::string GetPatchTypeStr() const override {return "meta: LoadPreviousBank";}
};

#endif // MetaPatch_LoadBank_h__
