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
class Patch;

struct AxeEffectBlockInfo
{
	std::string		mName;						// Amp 1
	std::string		mNormalizedName;			// amp 1
	std::string		mType;						// Amp
	int				mSysexEffectId;				// unique per name
	int				mSysexEffectIdMs;			//	derived 
	int				mSysexEffectIdLs;			//	derived
	int				mSysexBypassParameterId;	// unique per type
	int				mSysexBypassParameterIdMs;	//	derived
	int				mSysexBypassParameterIdLs;	//	derived
	int				mBypassCC;					// unique per name
	Patch			* mPatch;

	AxeEffectBlockInfo() :
		mSysexEffectId(-1),
		mSysexEffectIdLs(-1),
		mSysexEffectIdMs(-1),
		mSysexBypassParameterId(-1),
		mSysexBypassParameterIdLs(-1),
		mSysexBypassParameterIdMs(-1),
		mBypassCC(0),
		mPatch(NULL)
	{
	}

	AxeEffectBlockInfo(int id, const std::string & name, std::string & normalizedName, const std::string & type, int cc) :
		mName(name),
		mNormalizedName(normalizedName),
		mType(type),
		mSysexEffectId(id),
		mSysexBypassParameterId(-1),
		mSysexBypassParameterIdLs(-1),
		mSysexBypassParameterIdMs(-1),
		mBypassCC(cc),
		mPatch(NULL)
	{
		mSysexEffectIdLs = mSysexEffectId & 0x0000000F;
		mSysexEffectIdMs = (mSysexEffectId >> 4) & 0x0000000F;
	}

	void SetBypass(int id)
	{
		mSysexBypassParameterId = id;
		mSysexBypassParameterIdLs = mSysexBypassParameterId & 0x0000000F;
		mSysexBypassParameterIdMs = (mSysexBypassParameterId >> 4) & 0x0000000F;
	}
};

typedef std::vector<AxeEffectBlockInfo> AxeEffectBlocks;


// AxemlLoader
// ----------------------------------------------------------------------------
// Reads default.axeml file that ships with Axe-Edit
//
class AxemlLoader
{
public:
	AxemlLoader(ITraceDisplay * traceDisplay) : mTrace(traceDisplay) { }

	bool Load(const std::string & doc, AxeEffectBlocks & effects);

private:
	void LoadEffectPool(TiXmlElement* pElem);
	void LoadParameterLists(TiXmlElement* pElem);
	void ReportMissingBypassIds();
	void SetEffectBypass(const std::string & type, int bypassId);

private:
	ITraceDisplay * mTrace;
	AxeEffectBlocks	mEffects;
};

#endif // AxemlLoader_h__
