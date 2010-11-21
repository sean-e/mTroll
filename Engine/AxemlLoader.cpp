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

#include "AxemlLoader.h"
#include "../tinyxml/tinyxml.h"
#include "ITraceDisplay.h"

/*
 *	Bottom of page: http://www.fractalaudio.com/forum/viewtopic.php?f=14&t=9077&hilit=effect+block+ids&start=40
 *	Load default.axeml
 *		Read CONFIG/EffectPool 
 *			for each EffectPoolInstance: save type, name and id (1 id per type instance)
 *		Read CONFIG/EffectParameterLists
 *			for each EffectParameters: check name and cross-ref against EffectPool name (name="Amp")
 *				for each EffectParameter: check name; if ends in _BYPASS (but not _BYPASSMODE) keep id

	<CONFIG revision="1" model="1" majorVersion="10" minorVersion="1" name="default">
		<EffectPool>
			<EffectPoolInstance id="106" name="Amp 1" type="Amp"/>
		</EffectPool>
		<EffectParameterLists>
			<EffectParameters name="Amp">
				<EffectParameter id="0" name="DISTORT_TYPE" modifierID="0"/>
			</EffectParameters>
		</EffectParameterLists>
	</CONFIG>
 */

bool
AxemlLoader::Load(const std::string & axeFile, AxeEffects & effects)
{
	TiXmlDocument doc(axeFile);
	if (!doc.LoadFile()) 
	{
		if (mTrace)
		{
			std::string msg("Failed to load AxeFx axeml file\n");
			mTrace->Trace(msg);
		}
		return false;
	}

	TiXmlHandle hDoc(&doc);
	TiXmlElement* pElem = hDoc.FirstChildElement().Element();
	if (!pElem || pElem->ValueStr() != "CONFIG")
	{
		if (mTrace)
		{
			std::string msg("Invalid AxeFx axeml file\n");
			mTrace->Trace(msg);
		}
		return false;
	}

	TiXmlHandle hRoot(NULL);
	hRoot = TiXmlHandle(pElem);
	pElem = hRoot.FirstChild("EffectPool").FirstChildElement().Element();
	if (!pElem)
	{
		if (mTrace)
		{
			std::string msg("AxeFx axeml file missing EffectPool\n");
			mTrace->Trace(msg);
		}
		return false;
	}
	LoadEffectPool(pElem);

	pElem = hRoot.FirstChild("EffectParameterLists").FirstChildElement().Element();
	if (!pElem)
	{
		if (mTrace)
		{
			std::string msg("AxeFx axeml file missing EffectParameterLists\n");
			mTrace->Trace(msg);
		}
		return false;
	}
	LoadParameterLists(pElem);

	CheckResults();
	mEffects.swap(effects);
	return true;
}

void
AxemlLoader::LoadEffectPool(TiXmlElement* pElem)
{
	// <EffectPoolInstance id="106" name="Amp 1" type="Amp"/>
	for ( ; pElem; pElem = pElem->NextSiblingElement())
	{
		if (pElem->ValueStr() != "EffectPoolInstance")
			continue;

		int effectId = -1;
		std::string effectName, effectType;

		// QueryValueAttribute does not work with string when there are 
		// spaces (truncated at whitespace); use Attribute instead
		if (pElem->Attribute("name"))
			effectName = pElem->Attribute("name");

		pElem->QueryIntAttribute("id", &effectId);
		pElem->QueryValueAttribute("type", &effectType);
		if (effectId == -1 || effectName.empty() || effectType.empty() || 
			effectType == "Dummy" || effectType == "Controllers")
		{
			continue;
		}

		mEffects.push_back(AxeEffect(effectId, effectName, effectType));
	}
}

void
AxemlLoader::LoadParameterLists(TiXmlElement* pElem)
{
	// 	<EffectParameters name="Amp">
	// 		<EffectParameter id="0" name="DISTORT_TYPE" modifierID="0"/>
	// 	</EffectParameters>

	for ( ; pElem; pElem = pElem->NextSiblingElement())
	{
		if (pElem->ValueStr() != "EffectParameters")
			continue;

		std::string effectType;
		pElem->QueryValueAttribute("name", &effectType);
		if (effectType.empty())
			continue;

		bool foundBypass = false;
		TiXmlHandle hRoot(NULL);
		hRoot = TiXmlHandle(pElem);
		for (TiXmlElement *childElem = hRoot.FirstChildElement().Element(); 
			 childElem; 
			 childElem = childElem->NextSiblingElement())
		{
			if (childElem->ValueStr() != "EffectParameter")
				continue;

			std::string paramName;
			childElem->QueryValueAttribute("name", &paramName);
			if (paramName.empty())
				continue;

			// for each EffectParameter: check name; if ends in _BYPASS (but not _BYPASSMODE) keep id
			const std::string kBypass("_BYPASS");
			int pos = paramName.find(kBypass);
			if (-1 == pos)
			{
				// _FLAGS instead of _BYPASS for:
				// Amp Chorus Flanger Pitch
				const std::string kFlags("_FLAGS");
				pos = paramName.find(kFlags);
				if (-1 == pos || pos != paramName.length() - kFlags.length())
					continue;
			}
			else if (pos != paramName.length() - kBypass.length())
				continue;
			else
				foundBypass = true;

			int paramId = -1;
			childElem->QueryIntAttribute("id", &paramId);
			if (paramId == -1)
				continue;

			SetEffectBypass(effectType, paramId);
			if (foundBypass)
				break; // continuing looking if we found _FLAGS, just in case there is _FLAGS and _BYPASS
		}
	}
}

void
AxemlLoader::SetEffectBypass(const std::string & type, int bypassId)
{
	for (AxeEffects::iterator it = mEffects.begin(); it != mEffects.end(); ++it)
	{
		AxeEffect & cur = *it;
		if (cur.mType == type)
			cur.SetBypass(bypassId);
		// don't break because there can be multiple instances in the EffectPool
	}
}

void
AxemlLoader::CheckResults()
{
	for (AxeEffects::iterator it = mEffects.begin(); it != mEffects.end(); ++it)
	{
		AxeEffect & cur = *it;
		if (cur.mType == "FeedbackSend" || cur.mType == "Mixer")
			continue; // these can't be bypassed

		if (-1 == cur.mEffectId)
		{
			if (mTrace)
			{
				std::string msg("AxeFx EffectID is missing from " + cur.mName + "\n");
				mTrace->Trace(msg);
			}
		}
		if (-1 == cur.mBypassParameterId)
		{
			if (mTrace)
			{
				std::string msg("AxeFx Bypass paramenter ID is missing from " + cur.mName + "\n");
				mTrace->Trace(msg);
			}
		}
	}
}
