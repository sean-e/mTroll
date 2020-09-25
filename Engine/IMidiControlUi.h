/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2008,2010,2015,2020 Sean Echevarria
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

#ifndef IMidiControlUi_h__
#define IMidiControlUi_h__

#include <string>


// IMidiControlUi
// ----------------------------------------------------------------------------
// use to load xml based ui settings
//
class IMidiControlUi
{
public:
	virtual void	AddSwitchMapping(int switchNumber, int row, int col) = 0;

	virtual void	SetSwitchLedConfig(int width, int height, int vOffset, int hOffset, unsigned int offColor, int uiColorOffset) = 0;
	virtual void	CreateSwitchLed(int id, int top, int left) = 0;

	virtual void	SetSwitchConfig(int width, int height, int vOffset, int hOffset, const std::string & fontName, int fontHeight, bool bold, unsigned int fgColor) = 0;
	virtual void	CreateSwitch(int id, const std::string & label, int top, int left) = 0;

	virtual void	SetSwitchTextDisplayConfig(int width, int height, int vOffset, int hOffset, const std::string & fontName, int fontHeight, bool bold, unsigned int bgColor, unsigned int fgColor) = 0;
	virtual void	CreateSwitchTextDisplay(int id, int top, int left) = 0;
	virtual void	CreateSwitchTextDisplay(int id, int top, int left, int width) = 0;
	virtual void	CreateSwitchTextDisplay(int id, int top, int left, int width, int height) = 0;

	virtual void	CreateMainDisplay(int top, int left, int width, int height, const std::string & fontName, int fontHeight, bool bold, unsigned int bgColor, unsigned int fgColor) = 0;
	virtual void	CreateTraceDisplay(int top, int left, int width, int height, const std::string & fontName, int fontHeight, bool bold) = 0;
	virtual void	CreateStaticLabel(const std::string & label, int top, int left, int width, int height, const std::string & fontName, int fontHeight, bool bold, unsigned int bgColor, unsigned int fgColor) = 0;

	virtual void	EnableAutoGrid() = 0;
	virtual void	CreateAssemblyInGrid(int id, int row, int col, int colSpan, const std::string & label, bool createTextDisplay, bool createSwitch, bool createLed) = 0;
	virtual void	CreateMainDisplayInGrid(int row, int col, int colSpan, const std::string & fontName, int fontHeight, bool bold, unsigned int bgColor, unsigned int fgColor, int minHeight) = 0;
	virtual void	CreateTraceDisplayInGrid(int row, int col, int colSpan, const std::string & fontName, int fontHeight, bool bold) = 0;

	virtual void	SetMainSize(int width, int height) = 0;
	virtual void	SetHardwareLedIntensity(short brightness) = 0;
	virtual void	SetColors(unsigned int backgroundColor, unsigned int frameHighlightColor) = 0;
};

#endif // IMidiControlUi_h__
