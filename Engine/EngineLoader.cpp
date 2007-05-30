#include "EngineLoader.h"
#include "MidiControlEngine.h"
#include "PatchBank.h"
#include "Patch.h"
#include "../tinyxml/tinyxml.h"
#include "HexStringUtils.h"


static PatchBank::PatchState GetLoadState(const std::string & tmpLoad);


EngineLoader::EngineLoader(IMidiOut * midiOut,
						   IMainDisplay * mainDisplay,
						   ISwitchDisplay * switchDisplay,
						   ITraceDisplay * traceDisplay) :
	mEngine(NULL),
	mMidiOut(midiOut),
	mMainDisplay(mainDisplay),
	mSwitchDisplay(switchDisplay),
	mTraceDisplay(traceDisplay)
{
}

MidiControlEngine *
EngineLoader::CreateEngine(const std::string & engineSettingsFile)
{
	TiXmlDocument doc(engineSettingsFile);
	if (!doc.LoadFile()) 
		return mEngine;

	TiXmlHandle hDoc(&doc);
	TiXmlElement* pElem = hDoc.FirstChildElement().Element();
	// should always have a valid root but handle gracefully if it does
	if (!pElem) 
		return mEngine;

	if (pElem->Value() != "MidiControlSettings")
		return mEngine;

	TiXmlHandle hRoot(TiXmlHandle(pElem));

	pElem = hRoot.FirstChild("SystemConfig").Element();
	if (!LoadSystemConfig(pElem))
		return mEngine;

	pElem = hRoot.FirstChild("patches").Element().FirstChild().Element();
	LoadPatches(pElem);

	pElem = hRoot.FirstChild("banks").Element().FirstChild().Element();
	LoadBanks(pElem);

	mEngine->CompleteInit();

	MidiControlEngine * createdEngine = mEngine;
	mEngine = NULL;
	return createdEngine;
}

bool
EngineLoader::LoadSystemConfig(TiXmlElement * pElem)
{
	if (!pElem)
		return false;

	int filterPC = 0;
	int powerupTimeout = 0;
	int powerupPatch = 0;
	int powerupBank = 0;
	int incrementSwitch = 13;
	int decrementSwitch = 14;
	int modeSwitch = 15;

	pElem->QueryIntAttribute("filterPC", &filterPC);

	TiXmlElement * pChildElem = pElem->FirstChild("powerup").Element();
	if (pChildElem)
	{
		pChildElem->QueryIntAttribute("timeout", &powerupTimeout);
		pChildElem->QueryIntAttribute("bank", &powerupBank);
		pChildElem->QueryIntAttribute("patch", &powerupPatch);
	}

	pChildElem = pElem->FirstChild("switches").Element().FirstChild().Element();
	for ( ; pChildElem; pChildElem = pChildElem->NextSiblingElement())
	{
		std::string name;
		int id = 0;
		pChildElem->QueryValueAttribute("name", &name);
		pChildElem->QueryIntAttribute("id", &id);

		if (name == "increment" && id > 0)
			incrementSwitch = id;
		else if (name == "decrement" && id > 0)
			decrementSwitch = id;
		else if (name == "mode" && id > 0)
			modeSwitch = id;
	}

	mEngine = new MidiControlEngine(mMidiOut, mMainDisplay, mSwitchDisplay, mTraceDisplay, incrementSwitch, decrementSwitch, modeSwitch);
	mEngine->SetPowerup(powerupBank, powerupPatch, powerupTimeout);
	mEngine->FilterRedundantProgChg(filterPC);
	return true;
}

void
EngineLoader::LoadPatches(TiXmlElement * pElem)
{
	for ( ; pElem; pElem = pElem->NextSiblingElement())
	{
		if (pElem->Value() != "patch")
			continue;

		std::string tmp;
		std::string byteStringA;
		std::string byteStringB;

		const std::string patchName = pElem->Attribute("name");
		const int patchNumber = pElem->Attribute("number");
		pElem->QueryValueAttribute("type", &tmp);
		Patch::PatchType patchType = Patch::ptNormal;
		if (tmp == "normal")
			patchType = Patch::ptNormal;
		else if (tmp == "toggle")
			patchType = Patch::ptToggle;
		else if (tmp == "momentary")
			patchType = Patch::ptMomentary;

		for (TiXmlElement * childElem = pElem->FirstChild().Element(); 
			 childElem; 
			 childElem = childElem->NextSiblingElement())
		{
			if (childElem->Value() != "bytestring")
				continue;
		
			tmp = childElem->Attribute("name");
			if (tmp == "A")
				byteStringA = childElem->GetText();
			else if (tmp == "B")
				byteStringB = childElem->GetText();
		}

		if (patchNumber != -1 && !patchName.empty())
		{
			Bytes bytesA;
			int retval = ::ValidateString(byteStringA, bytesA);
			if (-1 != retval)
			{
				Bytes bytesB;
				retval = ::ValidateString(byteStringB, bytesB);
				if (-1 != retval)
					mEngine->AddPatch(patchNumber, patchName, patchType, bytesA, bytesB);
			}
		}
	}
}

void
EngineLoader::LoadBanks(TiXmlElement * pElem)
{
	for ( ; pElem; pElem = pElem->NextSiblingElement())
	{
		if (pElem->Value() != "bank")
			continue;

		const std::string bankName = pElem->Attribute("name");
		const int bankNumber = pElem->Attribute("number");
		if (bankNumber == -1 || bankName.empty())
			continue;

		PatchBank & bank = mEngine->AddBank(bankNumber, bankName);

		for (TiXmlElement * childElem = pElem->FirstChild().Element(); 
			 childElem; 
			 childElem = childElem->NextSiblingElement())
		{
			if (childElem->Value() != "PatchMap")
				continue;

			const int switchNumber = childElem->Attribute("switch");
			const int patchNumber = childElem->Attribute("patch");

			std::string tmp;
			childElem->QueryValueAttribute("loadState", &tmp);
			const PatchBank::PatchState loadState = GetLoadState(tmp);
			childElem->QueryValueAttribute("unloadState", &tmp);
			const PatchBank::PatchState unloadState = GetLoadState(tmp);

			bank.AddPatchMapping(switchNumber, patchNumber, loadState, unloadState);
		}
	}
}

PatchBank::PatchState
GetLoadState(const std::string & tmpLoad)
{
	if (tmpLoad == "A")
		return PatchBank::stA;
	else if (tmpLoad == "B")
		return PatchBank::stB;
	else
		return PatchBank::stIgnore;
}
