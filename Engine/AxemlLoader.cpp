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
AxemlLoader::Load(const std::string & axeFile)
{
	TiXmlDocument doc(axeFile);
	if (!doc.LoadFile()) 
		return false;

	TiXmlHandle hDoc(&doc);
	TiXmlElement* pElem = hDoc.FirstChildElement().Element();
	if (!pElem || pElem->ValueStr() != "CONFIG")
		return false;

	TiXmlHandle hRoot(NULL);
	hRoot = TiXmlHandle(pElem);
	pElem = hRoot.FirstChild("EffectPool").FirstChildElement().Element();
	if (!pElem)
		return false;
	LoadEffectPool(pElem);

	pElem = hRoot.FirstChild("EffectParameterLists").FirstChildElement().Element();
	if (!pElem)
		return false;
	LoadParameterLists(pElem);

	CheckResults();
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

		// TODO: fix read of name with space

		pElem->QueryIntAttribute("id", &effectId);
		pElem->QueryValueAttribute("name", &effectName);
		pElem->QueryValueAttribute("type", &effectType);
		if (effectId == -1 || effectName.empty() || effectType.empty() || 
			effectType == "Dummy" || effectType == "Controllers")
		{
			continue;
		}

		AddEffect(effectId, effectName, effectType);
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
			if (-1 == pos || pos != paramName.length() - kBypass.length())
				continue;

			int paramId = -1;
			childElem->QueryIntAttribute("id", &paramId);
			if (paramId == -1)
				continue;

			SetEffectBypass(effectType, paramId);
			break;
		}
	}
}

void
AxemlLoader::AddEffect(int id, const std::string & name, const std::string & type)
{
	// TODO:
	// save id as int and as 2 nibbles?
	if (mTrace)
	{
		std::string msg(name + " " + type + "\n");
		mTrace->Trace(msg);
	}
}

void
AxemlLoader::SetEffectBypass(const std::string & type, int bypassId)
{
	// TODO:
	// save id as int and as 2 nibbles?
	if (mTrace)
	{
		std::string msg(type + "\n");
		mTrace->Trace(msg);
	}
}

void
AxemlLoader::CheckResults()
{
}
