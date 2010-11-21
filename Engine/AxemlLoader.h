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
#include <vector>

class TiXmlElement;
class ITraceDisplay;

struct AxeEffect
{
	std::string		mName;			// Amp 1
	std::string		mType;			// Amp
	int				mEffectId;
	int				mEffectIdMs;
	int				mEffectIdLs;
	int				mBypassParameterId;
	int				mBypassParameterIdMs;
	int				mBypassParameterIdLs;

	AxeEffect() :
		mEffectId(-1),
		mEffectIdLs(-1),
		mEffectIdMs(-1),
		mBypassParameterId(-1),
		mBypassParameterIdLs(-1),
		mBypassParameterIdMs(-1)
	{
	}

	AxeEffect(int id, const std::string & name, const std::string & type) :
		mName(name),
		mType(type),
		mEffectId(id),
		mBypassParameterId(-1),
		mBypassParameterIdLs(-1),
		mBypassParameterIdMs(-1)
	{
		mEffectIdLs = mEffectId & 0x0000000F;
		mEffectIdMs = (mEffectId >> 4) & 0x0000000F;
	}

	void SetBypass(int id)
	{
		mBypassParameterId = id;
		mBypassParameterIdLs = mBypassParameterId & 0x0000000F;
		mBypassParameterIdMs = (mBypassParameterId >> 4) & 0x0000000F;
	}
};

typedef std::vector<AxeEffect> AxeEffects;


// AxemlLoader
// ----------------------------------------------------------------------------
// Reads default.axeml file that ships with Axe-Edit
//
class AxemlLoader
{
public:
	AxemlLoader(ITraceDisplay * traceDisplay) : mTrace(traceDisplay) { }

	bool Load(const std::string & doc, AxeEffects & effects);

private:
	void LoadEffectPool(TiXmlElement* pElem);
	void LoadParameterLists(TiXmlElement* pElem);
	void CheckResults();

	void SetEffectBypass(const std::string & type, int bypassId);

private:
	ITraceDisplay * mTrace;
	AxeEffects	mEffects;
};

#endif // AxemlLoader_h__
