#include "UiLoader.h"
#include "IMidiControlUi.h"
#include "../tinyxml/tinyxml.h"


UiLoader::UiLoader(IMidiControlUi * theUi,
				   const std::string & settingsFile) :
	mUi(theUi)
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

	pElem = hRoot.FirstChild("switches").Element();
	if (pElem)
		LoadSwitches(pElem);

	pElem = hRoot.FirstChild("switchLeds").Element();
	if (pElem)
		LoadSwitchLeds(pElem);

	pElem = hRoot.FirstChild("switchTextDisplays").Element();
	if (pElem)
		LoadSwitchDisplays(pElem);

	LoadOtherStuffAndFinalize(hRoot.ToElement());
}

void
UiLoader::LoadSwitchLeds(TiXmlElement * pElem)
{
	// global state from pElem

	TiXmlHandle hRoot(NULL);
	hRoot = TiXmlHandle(pElem);

	// <switchLed number="0" top="22" left="31" width="26" height="10" />
	for (TiXmlElement * childElem = hRoot.FirstChild().Element(); 
		 childElem; 
		 childElem = childElem->NextSiblingElement())
	{
		if (childElem->ValueStr() != "switchLed")
			continue;

		int number = -1;
		int top = -1;
		int left = -1;
		int width = -1;
		int height = -1;

		childElem->QueryIntAttribute("number", &number);
		childElem->QueryIntAttribute("top", &top);
		childElem->QueryIntAttribute("left", &left);
		childElem->QueryIntAttribute("width", &width);
		childElem->QueryIntAttribute("height", &height);

		if (-1 == number ||
			-1 == top ||
			-1 == left ||
			-1 == width ||
			-1 == height)
			continue;

		mUi->CreateSwitchLed(number, top, left, width, height);
	}
}

void
UiLoader::LoadSwitches(TiXmlElement * pElem)
{
	// <switches font-weight="bold" >
	std::string fontWeight;
	pElem->QueryValueAttribute("font-weight", &fontWeight);
	const bool boldFont = (fontWeight == "bold");

	int fontHeight = 10;
	pElem->QueryIntAttribute("font-height", &fontHeight);
	mUi->CreateSwitchFont(fontHeight, boldFont);

	TiXmlHandle hRoot(NULL);
	hRoot = TiXmlHandle(pElem);

	// <switch name="&1" number="0" top="31" left="15" width="75" height="23" />
	for (TiXmlElement * childElem = hRoot.FirstChild().Element(); 
		 childElem; 
		 childElem = childElem->NextSiblingElement())
	{
		if (childElem->ValueStr() != "switch")
			continue;

		int number = -1;
		int top = -1;
		int left = -1;
		int width = -1;
		int height = -1;

		std::string label;
		pElem->QueryValueAttribute("label", &label);
		childElem->QueryIntAttribute("number", &number);
		childElem->QueryIntAttribute("top", &top);
		childElem->QueryIntAttribute("left", &left);
		childElem->QueryIntAttribute("width", &width);
		childElem->QueryIntAttribute("height", &height);

		if (-1 == number ||
			-1 == top ||
			-1 == left ||
			-1 == width ||
			-1 == height)
			continue;

		mUi->CreateSwitch(number, label, top, left, width, height);
	}
}

void
UiLoader::LoadSwitchDisplays(TiXmlElement * pElem)
{
	// <switchTextDisplays font-height="12" >
	std::string fontWeight;
	pElem->QueryValueAttribute("font-weight", &fontWeight);
	const bool boldFont = (fontWeight == "bold");

	int fontHeight = 10;
	pElem->QueryIntAttribute("font-height", &fontHeight);
	mUi->CreateSwitchTextDisplayFont(fontHeight, boldFont);

	TiXmlHandle hRoot(NULL);
	hRoot = TiXmlHandle(pElem);

	// <switchTextDisplay number="0" top="5" left="5" width="105" height="23" />
	for (TiXmlElement * childElem = hRoot.FirstChild().Element(); 
		 childElem; 
		 childElem = childElem->NextSiblingElement())
	{
		if (childElem->ValueStr() != "switchTextDisplay")
			continue;

		int number = -1;
		int top = -1;
		int left = -1;
		int width = -1;
		int height = -1;

		childElem->QueryIntAttribute("number", &number);
		childElem->QueryIntAttribute("top", &top);
		childElem->QueryIntAttribute("left", &left);
		childElem->QueryIntAttribute("width", &width);
		childElem->QueryIntAttribute("height", &height);

		if (-1 == number ||
			-1 == top ||
			-1 == left ||
			-1 == width ||
			-1 == height)
			continue;

		mUi->CreateSwitchTextDisplay(number, top, left, width, height);
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
			mUi->CreateMainDisplay(top, left, width, height, fontHeight, boldFont);
		}
	}

	// <traceTextDisplay top="177" left="305" width="338" height="163" />
	pElem = hRoot.FirstChild("traceTextDisplay").Element();
	if (pElem)
	{
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
			mUi->CreateTraceDisplay(top, left, width, height, fontHeight, boldFont);
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
