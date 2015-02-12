/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2012,2014-2015 Sean Echevarria
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

#ifndef Patch_h__
#define Patch_h__

#include <string>
#include <set>
#include "ExpressionPedals.h"

class IMainDisplay;
class ISwitchDisplay;
class IMidiOut;
class MidiControlEngine;


class Patch
{
public:
	virtual ~Patch();

	ExpressionPedals & GetPedals() { return mPedals; }
	virtual void OverridePedals(bool overridePedals) { mOverridePedals = overridePedals; }

	void AssignSwitch(int switchNumber, ISwitchDisplay * switchDisplay);
	void ClearSwitch(ISwitchDisplay * switchDisplay);
	void RemoveSwitch(int switchNumber, ISwitchDisplay * switchDisplay);

	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) = 0;
	virtual void SwitchReleased(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) { }

	void UpdateDisplays(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) const;
	const std::string & GetName() const { return mName; }
	void SetName(const std::string& name) { mName = name; }
	virtual bool HasDisplayText() const { return false; }
	virtual const std::string & GetDisplayText() const { return mName; }
	int GetNumber() const {return mNumber;}
	bool IsActive() const {return mPatchIsActive;}
	// used to drive led display without exec
	void ActivateSwitchDisplay(ISwitchDisplay * switchDisplay, bool activate) const;
	// used to update state of patch without exec
	virtual void UpdateState(ISwitchDisplay * switchDisplay, bool active);
	virtual void Disable(ISwitchDisplay * switchDisplay);
	virtual bool UpdateMainDisplayOnPress() const {return true;}
	virtual void CompleteInit(MidiControlEngine * eng, ITraceDisplay * trc) { }

	virtual void Deactivate(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);

	virtual std::string GetPatchTypeStr() const = 0;
	virtual bool IsPatchVolatile() const {return false;} // load of one volatile patch affects loaded volatile patch (typically normal mode patches)
	virtual void DeactivateVolatilePatch() { }

	virtual void BankTransitionActivation() = 0;
	virtual void BankTransitionDeactivation() = 0;

protected:
	Patch(int number, const std::string & name, IMidiOut * midiOut = NULL);

private:
	Patch();
	Patch(const Patch &);

protected:
	ExpressionPedals		mPedals;
	bool					mPatchIsActive;
	bool					mPatchSupportsDisabledState;
	bool					mPatchIsDisabled;
	bool					mOverridePedals;

private:
	const int				mNumber;	// unique across patches
	std::string				mName;
	std::set<int>			mSwitchNumbers;
};

extern ExpressionPedals * gActivePatchPedals;

#endif // Patch_h__
