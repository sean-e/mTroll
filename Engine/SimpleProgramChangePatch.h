/*
 * mTroll MIDI Controller
 * Copyright (C) 2024-2025 Sean Echevarria
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

#ifndef SimpleProgramChangePatch_h__
#define SimpleProgramChangePatch_h__

#include "TwoStatePatch.h"


 // SimpleProgramChangePatch
 // -----------------------------------------------------------------------------
 // responds to SwitchPressed; SwitchReleased does not affect patch state
 // 
class SimpleProgramChangePatch : public TwoStatePatch
{
public:
	SimpleProgramChangePatch(int number,
		const std::string & name,
		IMidiOutPtr midiOut,
		PatchCommands & cmds,
		int midiChannel) :
		TwoStatePatch(number, name, midiOut, cmds, PatchCommands{}, psDisallow),
		mMidiChannel(midiChannel)
	{
		_ASSERTE(-1 != mMidiChannel);
		_ASSERTE(-2 == mMidiChannel || (0 <= mMidiChannel && 16 > mMidiChannel));
	}

	virtual std::string GetPatchTypeStr() const override { return mGroupId.empty() ? "simpleProgramChange" : "groupsimpleProgramChange"; }

	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) override
	{
		__super::SwitchPressed(mainDisplay, switchDisplay);

		// Like NormalPatch, SimpleProgramChangePatch always execs the A commands on switch press.
		ExecCommandsA();

		UpdateDisplays(mainDisplay, switchDisplay);
	}

	virtual int GetDeviceProgramChangeChannel() const override 
	{ 
		if (-2 == mMidiChannel)
			return DynamicMidiCommand::GetDynamicMidiChannel();
		return mMidiChannel; 
	}

private:
	const int mMidiChannel;
};

#endif // SimpleProgramChangePatch_h__
