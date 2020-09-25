/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2008,2010,2014,2020 Sean Echevarria
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

#ifndef ISwitchDisplay_h__
#define ISwitchDisplay_h__

#include <string>
#include <memory>
#include <array>

class Patch;

using PatchPtr = std::shared_ptr<Patch>;


// ISwitchDisplay
// ----------------------------------------------------------------------------
// use to set / clear LEDs associated with the switches.
//
class ISwitchDisplay
{
public:
	virtual void SetSwitchDisplay(int switchNumber, unsigned int color) = 0;
	virtual void TurnOffSwitchDisplay(int switchNumber) = 0;
	virtual void ForceSwitchDisplay(int switchNumber, unsigned int color) = 0;
	virtual void DimSwitchDisplay(int switchNumber, unsigned int ledColor) = 0;
	virtual void SetSwitchText(int switchNumber, const std::string & txt) = 0;
	virtual void ClearSwitchText(int switchNumber) = 0;
	virtual void SetIndicatorThreadSafe(bool isOn, PatchPtr patch, int time) = 0;
	virtual void TestLeds(int testPattern) = 0;
	virtual void EnableDisplayUpdate(bool enable) = 0;
	virtual void UpdatePresetColors(std::array<unsigned int, 32> &presetColors) = 0;
};

#endif // ISwitchDisplay_h__
