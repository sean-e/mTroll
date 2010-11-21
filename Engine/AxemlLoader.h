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

#ifndef AxemlLoader_h__
#define AxemlLoader_h__

#include <string>

class TiXmlElement;
class ITraceDisplay;


// AxemlLoader
// ----------------------------------------------------------------------------
// Reads default.axeml file that ships with Axe-Edit
//
class AxemlLoader
{
public:
	AxemlLoader(ITraceDisplay * traceDisplay) : mTrace(traceDisplay) { }

	bool Load(const std::string & doc);

private:
	void LoadEffectPool(TiXmlElement* pElem);
	void LoadParameterLists(TiXmlElement* pElem);
	void CheckResults();

	void AddEffect(int id, const std::string & name, const std::string & type);
	void SetEffectBypass(const std::string & type, int bypassId);

private:
	ITraceDisplay * mTrace;
};

#endif // AxemlLoader_h__
