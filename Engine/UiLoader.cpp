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
    <switch font-name="Tahoma" font-weight="bold" font-height="10" width="75" height="23" />
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

	pElem = hRoot.FirstChild("switch").Element();
	if (!pElem)
		return false;
	
	fontname = "courier";
	fontWeight.clear();
	fontHeight = 10;
	width = height = 0;

	pElem->QueryValueAttribute("font-name", &fontname);
	pElem->QueryValueAttribute("font-weight", &fontWeight);
	pElem->QueryIntAttribute("font-height", &fontHeight);
	pElem->QueryIntAttribute("height", &height);
	pElem->QueryIntAttribute("width", &width);
	boldFont = (fontWeight == "bold");
	mUi->SetSwitchConfig(width, height, fontname, fontHeight, boldFont);

	pElem = hRoot.FirstChild("switchTextDisplay").Element();
	if (!pElem)
		return false;

	fontname = "courier";
	fontWeight.clear();
	fontHeight = 10;
	bgColor = 0;
	fgColor = 0xffffff;
	width = height = 0;

	pElem->QueryHexAttribute("foregroundColor", &fgColor);
	pElem->QueryHexAttribute("backgroundColor", &bgColor);
	pElem->QueryValueAttribute("font-name", &fontname);
	pElem->QueryValueAttribute("font-weight", &fontWeight);
	pElem->QueryIntAttribute("font-height", &fontHeight);
	pElem->QueryIntAttribute("height", &height);
	pElem->QueryIntAttribute("width", &width);
	boldFont = (fontWeight == "bold");
	mUi->SetSwitchTextDisplayConfig(width, height, fontname, fontHeight, boldFont, (unsigned int) bgColor, (unsigned int) fgColor);

	pElem = hRoot.FirstChild("switchLed").Element();
	if (!pElem)
		return false;

	bgColor = 0;
	fgColor = 0xffffff;
	width = height = 0;

	pElem->QueryHexAttribute("onColor", &fgColor);
	pElem->QueryHexAttribute("offColor", &bgColor);
	pElem->QueryIntAttribute("height", &height);
	pElem->QueryIntAttribute("width", &width);
	mUi->SetSwitchLedConfig(width, height, (unsigned int) fgColor, (unsigned int) bgColor);

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
	int idNumber = -1;
	int top = -1;
	int left = -1;

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
		vOffset = hOffset = 0;
		pElem->QueryIntAttribute("vOffset", &vOffset);
		pElem->QueryIntAttribute("hOffset", &hOffset);
		mUi->CreateSwitchTextDisplay(idNumber, top + vOffset, left + hOffset);
	}

	pElem = hRoot.FirstChild("switchLed").Element();
	if (pElem)
	{
		vOffset = hOffset = 0;
		pElem->QueryIntAttribute("vOffset", &vOffset);
		pElem->QueryIntAttribute("hOffset", &hOffset);
		mUi->CreateSwitchLed(idNumber, top + vOffset, left + hOffset);
	}

	pElem = hRoot.FirstChild("switch").Element();
	if (pElem)
	{
		vOffset = hOffset = 0;
		pElem->QueryIntAttribute("vOffset", &vOffset);
		pElem->QueryIntAttribute("hOffset", &hOffset);
		std::string label;
		pElem->QueryValueAttribute("label", &label);
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
		pElem->QueryValueAttribute("font-name", &fontname);

		std::string fontWeight;
		pElem->QueryValueAttribute("font-weight", &fontWeight);
		const bool boldFont = (fontWeight == "bold");

		int fontHeight = 10;
		pElem->QueryIntAttribute("font-height", &fontHeight);

		int bgColor = 0;
		int fgColor = 0xffffff;
		pElem->QueryHexAttribute("foregroundColor", &fgColor);
		pElem->QueryHexAttribute("backgroundColor", &bgColor);

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
		pElem->QueryValueAttribute("font-name", &fontname);

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

	// <frame height="567" width="817" />
	pElem = hRoot.FirstChild("frame").Element();
	if (pElem)
	{
		int height = -1;
		pElem->QueryIntAttribute("height", &height);

		int width = -1;
		pElem->QueryIntAttribute("width", &width);

		if (!(-1 == height || -1 == width))
			mUi->SetMainSize(width, height);
	}
}
