/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2008,2010,2015 Sean Echevarria
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

#include "UiLoader.h"
#include "IMidiControlUi.h"
#include "../tinyxml/tinyxml.h"


UiLoader::UiLoader(IMidiControlUi * theUi,
				   const std::string & settingsFile) :
	mUi(theUi),
	mPreviousAssemblyHpos(0),
	mPreviousAssemblyVpos(0)
{
	TiXmlDocument doc(settingsFile);
	if (!doc.LoadFile()) 
		return;

	TiXmlHandle hDoc(&doc);
	TiXmlElement* pElem = hDoc.FirstChildElement().Element();
	// should always have a valid root but handle gracefully if it doesn't
	if (!pElem) 
		return;

	if (pElem->ValueStr() != "MidiControlUiSettings")
		return;

	TiXmlHandle hRoot(NULL);
	hRoot = TiXmlHandle(pElem);

	LoadFrameInfo(hRoot.ToElement());

	pElem = hRoot.FirstChild("switchMappings").Element();
	if (pElem)
		LoadSwitchMappings(pElem);

	pElem = hRoot.FirstChild("switchAssemblyConfig").Element();
	if (pElem)
	{
		LoadAssembyConfig(pElem);
		pElem = hRoot.FirstChild("switchAssemblies").Element();
		if (pElem)
			LoadSwitchAssemblies(pElem);
	}

	LoadOtherStuffAndFinalize(hRoot.ToElement());
}

bool
UiLoader::LoadAssembyConfig(TiXmlElement * pElem)
{
/*
  <switchAssemblyConfig>
    <switch font-name="Tahoma" font-weight="bold" font-height="10" width="75" height="23" foregroundColor="bbbbbb" />
    <switchTextDisplay font-name="Tahoma" font-height="12" foregroundColor="65280" backgroundColor="0" width="105" height="23" />
    <switchLed width="26" height="10" onColor="65280" offColor="256" />
  </switchAssemblyConfig>
*/
	TiXmlHandle hRoot(NULL);
	hRoot = TiXmlHandle(pElem);

	std::string fontname;
	std::string fontWeight;
	int fontHeight;
	bool boldFont;
	int bgColor, fgColor;
	int width, height;
	int vOffset, hOffset;

	pElem = hRoot.FirstChild("switch").Element();
	if (!pElem)
		return false;
	
	fontname = "courier";
	fontWeight.clear();
	fontHeight = 10;
	width = height = 0;
	vOffset = hOffset = 0;
	fgColor = -1;

	pElem->QueryHexAttribute("foregroundColor", &fgColor);
	// QueryValueAttribute does not work with string when there are 
	// spaces (truncated at whitespace); use Attribute instead
	if (pElem->Attribute("font-name"))
		fontname = pElem->Attribute("font-name");
	pElem->QueryValueAttribute("font-weight", &fontWeight);
	pElem->QueryIntAttribute("font-height", &fontHeight);
	pElem->QueryIntAttribute("height", &height);
	pElem->QueryIntAttribute("width", &width);
	pElem->QueryIntAttribute("vOffset", &vOffset);
	pElem->QueryIntAttribute("hOffset", &hOffset);
	boldFont = (fontWeight == "bold");
	mUi->SetSwitchConfig(width, height, vOffset, hOffset, fontname, fontHeight, boldFont, (unsigned int) fgColor);

	pElem = hRoot.FirstChild("switchTextDisplay").Element();
	if (!pElem)
		return false;

	fontname = "courier";
	fontWeight.clear();
	fontHeight = 10;
	bgColor = 0;
	fgColor = 0xffffff;
	width = height = 0;
	vOffset = hOffset = 0;

	pElem->QueryHexAttribute("foregroundColor", &fgColor);
	pElem->QueryHexAttribute("backgroundColor", &bgColor);
	// QueryValueAttribute does not work with string when there are 
	// spaces (truncated at whitespace); use Attribute instead
	if (pElem->Attribute("font-name"))
		fontname = pElem->Attribute("font-name");
	pElem->QueryValueAttribute("font-weight", &fontWeight);
	pElem->QueryIntAttribute("font-height", &fontHeight);
	pElem->QueryIntAttribute("height", &height);
	pElem->QueryIntAttribute("width", &width);
	pElem->QueryIntAttribute("vOffset", &vOffset);
	pElem->QueryIntAttribute("hOffset", &hOffset);
	boldFont = (fontWeight == "bold");
	mUi->SetSwitchTextDisplayConfig(width, height, vOffset, hOffset, fontname, fontHeight, boldFont, (unsigned int) bgColor, (unsigned int) fgColor);

	pElem = hRoot.FirstChild("switchLed").Element();
	if (!pElem)
		return false;

	bgColor = 0;
	fgColor = 0xffffff;
	width = height = 0;
	vOffset = hOffset = 0;

	pElem->QueryHexAttribute("onColor", &fgColor);
	pElem->QueryHexAttribute("offColor", &bgColor);
	pElem->QueryIntAttribute("height", &height);
	pElem->QueryIntAttribute("width", &width);
	pElem->QueryIntAttribute("vOffset", &vOffset);
	pElem->QueryIntAttribute("hOffset", &hOffset);
	mUi->SetSwitchLedConfig(width, height, vOffset, hOffset, (unsigned int) fgColor, (unsigned int) bgColor);

	return true;
}

void
UiLoader::LoadSwitchMappings(TiXmlElement * pElem)
{
/*
	<switchMappings>
		<switchGridMap number="0" row="1" col="0" />
	</switchMappings>
*/
	TiXmlHandle hRoot(NULL);
	hRoot = TiXmlHandle(pElem);

	for (TiXmlElement * childElem = hRoot.FirstChild().Element(); 
		 childElem; 
		 childElem = childElem->NextSiblingElement())
	{
		if (childElem->ValueStr() != "switchMap")
			continue;

		int switchNumber = -1, row = -1, col = -1;
		childElem->QueryIntAttribute("number", &switchNumber);
		childElem->QueryIntAttribute("row", &row);
		childElem->QueryIntAttribute("col", &col);
		if (-1 == switchNumber ||
			-1 == row ||
			-1 == col)
			continue;

		mUi->AddSwitchMapping(switchNumber, row, col);
	}
}

void
UiLoader::LoadSwitchAssemblies(TiXmlElement * pElem)
{
/*
  <switchAssemblies>
    <switchAssembly>
    </switchAssembly>
  </switchAssemblies>
*/
	TiXmlHandle hRoot(NULL);
	hRoot = TiXmlHandle(pElem);

	for (TiXmlElement * childElem = hRoot.FirstChild().Element(); 
		 childElem; 
		 childElem = childElem->NextSiblingElement())
	{
		if (childElem->ValueStr() != "switchAssembly")
			continue;

		LoadSwitchAssembly(childElem);
	}
}

void
UiLoader::LoadSwitchAssembly(TiXmlElement * pElem)
{
/*
    <switchAssembly number="0" top="5" left="5" >
      <switchTextDisplay />
      <switchLed vOffset="30" hOffset="40" />
      <switch label="&amp;1" vOffset="45" hOffset="15" />
    </switchAssembly>
*/
	TiXmlHandle hRoot(NULL);
	hRoot = TiXmlHandle(pElem);

	int vOffset, hOffset;
	int tmp = -1;
	int idNumber = -1;
	int top = -1;
	int left = -1;
	int width = 0;
	int height = 0;

	vOffset = hOffset = 0;
	pElem->QueryIntAttribute("number", &idNumber);
	pElem->QueryIntAttribute("top", &top);
	pElem->QueryIntAttribute("left", &left);
	pElem->QueryIntAttribute("incrementalVpos", &vOffset);
	pElem->QueryIntAttribute("incrementalHpos", &hOffset);
	if (-1 == idNumber)
		return;
	if (-1 == top && (-1 == vOffset || 0 == mPreviousAssemblyVpos))
		return;
	if (-1 == left && (-1 == hOffset || 0 == mPreviousAssemblyHpos))
		return;

	if (-1 == top)
		top = mPreviousAssemblyVpos + vOffset;
	mPreviousAssemblyVpos = top;

	if (-1 == left)
		left = mPreviousAssemblyHpos + hOffset;
	mPreviousAssemblyHpos = left;

	pElem = hRoot.FirstChild("switchTextDisplay").Element();
	if (pElem)
	{
		vOffset = hOffset = width = height = 0;
		pElem->QueryIntAttribute("vOffset", &vOffset);
		pElem->QueryIntAttribute("hOffset", &hOffset);

		tmp = -1;
		pElem->QueryIntAttribute("top", &tmp);
		if (-1 != tmp)
			top = tmp;

		tmp = -1;
		pElem->QueryIntAttribute("left", &tmp);
		if (-1 != tmp)
			left = tmp;

		tmp = -1;
		pElem->QueryIntAttribute("width", &tmp);
		if (-1 != tmp)
			width = tmp;

		tmp = -1;
		pElem->QueryIntAttribute("height", &height);
		if (-1 != tmp)
			height = tmp;

		if (width && height)
			mUi->CreateSwitchTextDisplay(idNumber, top + vOffset, left + hOffset, width, height);
		else if (width)
			mUi->CreateSwitchTextDisplay(idNumber, top + vOffset, left + hOffset, width);
		else
			mUi->CreateSwitchTextDisplay(idNumber, top + vOffset, left + hOffset);
	}

	pElem = hRoot.FirstChild("switchLed").Element();
	if (pElem)
	{
		vOffset = hOffset = width = height = 0;
		tmp = -1;
		pElem->QueryIntAttribute("vOffset", &vOffset);
		pElem->QueryIntAttribute("hOffset", &hOffset);
		mUi->CreateSwitchLed(idNumber, top + vOffset, left + hOffset);
	}

	pElem = hRoot.FirstChild("switch").Element();
	if (pElem)
	{
		vOffset = hOffset = width = height = 0;
		tmp = -1;
		pElem->QueryIntAttribute("vOffset", &vOffset);
		pElem->QueryIntAttribute("hOffset", &hOffset);
		std::string label;
		if (pElem->Attribute("label"))
			label = pElem->Attribute("label");
		mUi->CreateSwitch(idNumber, label, top + vOffset, left + hOffset);
	}
}

void
UiLoader::LoadOtherStuffAndFinalize(TiXmlElement * pElem)
{
	TiXmlHandle hRoot(NULL);
	hRoot = TiXmlHandle(pElem);

	// <mainTextDisplay font-height="12" top="177" left="5" width="338" height="163" />
	pElem = hRoot.FirstChild("mainTextDisplay").Element();
	if (pElem)
	{
		std::string fontname("courier");
		// QueryValueAttribute does not work with string when there are 
		// spaces (truncated at whitespace); use Attribute instead
		if (pElem->Attribute("font-name"))
			fontname = pElem->Attribute("font-name");

		std::string fontWeight;
		pElem->QueryValueAttribute("font-weight", &fontWeight);
		const bool boldFont = (fontWeight == "bold");

		int fontHeight = 10;
		pElem->QueryIntAttribute("font-height", &fontHeight);

		int bgColor = 0;
		int fgColor = 0xffffff;
		pElem->QueryAttribute<int>("foregroundColor", &fgColor, "%x");
		pElem->QueryAttribute<int>("backgroundColor", &bgColor, "%x");

		int top = -1;
		int left = -1;
		int width = -1;
		int height = -1;

		pElem->QueryIntAttribute("top", &top);
		pElem->QueryIntAttribute("left", &left);
		pElem->QueryIntAttribute("width", &width);
		pElem->QueryIntAttribute("height", &height);

		if (!(-1 == top ||
			-1 == left ||
			-1 == width ||
			-1 == height))
		{
			mUi->CreateMainDisplay(top, left, width, height, fontname, fontHeight, boldFont, bgColor, fgColor);
		}
	}

	// <traceTextDisplay top="177" left="305" width="338" height="163" />
	pElem = hRoot.FirstChild("traceTextDisplay").Element();
	if (pElem)
	{
		std::string fontname("courier new");
		// QueryValueAttribute does not work with string when there are 
		// spaces (truncated at whitespace); use Attribute instead
		if (pElem->Attribute("font-name"))
			fontname = pElem->Attribute("font-name");

		std::string fontWeight;
		pElem->QueryValueAttribute("font-weight", &fontWeight);
		const bool boldFont = (fontWeight == "bold");

		int fontHeight = 10;
		pElem->QueryIntAttribute("font-height", &fontHeight);

		int top = -1;
		int left = -1;
		int width = -1;
		int height = -1;

		pElem->QueryIntAttribute("top", &top);
		pElem->QueryIntAttribute("left", &left);
		pElem->QueryIntAttribute("width", &width);
		pElem->QueryIntAttribute("height", &height);

		if (!(-1 == top ||
			-1 == left ||
			-1 == width ||
			-1 == height))
		{
			mUi->CreateTraceDisplay(top, left, width, height, fontname, fontHeight, boldFont);
		}
	}

	// <hardware ledBrightness="0" invertLeds="0" />
	pElem = hRoot.FirstChild("hardware").Element();
	if (pElem)
	{
		int ledBrightness = -1;
		pElem->QueryIntAttribute("ledBrightness", &ledBrightness);

		if (-1 != ledBrightness)
			mUi->SetHardwareLedIntensity(ledBrightness);

		int invertLeds = 0;
		pElem->QueryIntAttribute("invertLeds", &invertLeds);
		mUi->SetLedDisplayState(!!invertLeds);
	}
}

void
UiLoader::LoadFrameInfo(TiXmlElement * pElem)
{
	// <frame height="567" width="817" />
	TiXmlHandle hRoot(NULL);
	hRoot = TiXmlHandle(pElem);
	pElem = hRoot.FirstChild("frame").Element();
	if (pElem)
	{
		int height = -1;
		pElem->QueryIntAttribute("height", &height);

		int width = -1;
		pElem->QueryIntAttribute("width", &width);

		if (!(-1 == height || -1 == width))
			mUi->SetMainSize(width, height);

		int backgroundColor = -1;
		int frameHighlightColor = -1;
		pElem->QueryHexAttribute("backgroundColor", &backgroundColor);
		pElem->QueryHexAttribute("frameHighlightColor", &frameHighlightColor);
		if (backgroundColor != -1 && frameHighlightColor != -1)
			mUi->SetColors(backgroundColor, frameHighlightColor);
	}
}
