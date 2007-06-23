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
	mTraceDisplay(traceDisplay),
	mMidiOutDeviceIdx(0)
{
}

MidiControlEngine *
EngineLoader::CreateEngine(const std::string & engineSettingsFile)
{
	TiXmlDocument doc(engineSettingsFile);
	if (!doc.LoadFile()) 
	{
		std::string err = doc.ErrorDesc();
		return mEngine;
	}

	TiXmlHandle hDoc(&doc);
	TiXmlElement* pElem = hDoc.FirstChildElement().Element();
	// should always have a valid root but handle gracefully if it does
	if (!pElem) 
		return mEngine;

	if (pElem->ValueStr() != "MidiControlSettings")
		return mEngine;

	TiXmlHandle hRoot(NULL);
	hRoot = TiXmlHandle(pElem);

	pElem = hRoot.FirstChild("SystemConfig").Element();
	if (!LoadSystemConfig(pElem))
		return mEngine;

	pElem = hRoot.FirstChild("patches").FirstChild().Element();
	LoadPatches(pElem);

	pElem = hRoot.FirstChild("banks").FirstChild().Element();
	LoadBanks(pElem);

	mEngine->CompleteInit(mMidiOutDeviceIdx);

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
	int incrementSwitch = -1;
	int decrementSwitch = -1;
	int modeSwitch = -1;

	pElem->QueryIntAttribute("filterPC", &filterPC);

	TiXmlHandle hRoot(NULL);
	hRoot = TiXmlHandle(pElem);

	TiXmlElement * pChildElem = hRoot.FirstChild("powerup").Element();
	if (pChildElem)
	{
		pChildElem->QueryIntAttribute("timeout", &powerupTimeout);
		pChildElem->QueryIntAttribute("bank", &powerupBank);
		pChildElem->QueryIntAttribute("patch", &powerupPatch);
	}

	pChildElem = hRoot.FirstChild("switches").FirstChild().Element();
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

	// <midiDevice port="1" out="3" />
	pChildElem = hRoot.FirstChild("midiDevice").Element();
	if (pChildElem)
		pChildElem->QueryIntAttribute("outIdx", &mMidiOutDeviceIdx);

	mEngine = new MidiControlEngine(mMidiOut, mMainDisplay, mSwitchDisplay, mTraceDisplay, incrementSwitch, decrementSwitch, modeSwitch);
	mEngine->SetPowerup(powerupBank, powerupPatch, powerupTimeout);
	mEngine->FilterRedundantProgChg(filterPC ? true : false);
	return true;
}

void
EngineLoader::LoadPatches(TiXmlElement * pElem)
{
	for ( ; pElem; pElem = pElem->NextSiblingElement())
	{
		if (pElem->ValueStr() != "patch")
			continue;

		std::string tmp;
		std::string byteStringA;
		std::string byteStringB;
		int midiOutPortNumber = 1;

		const std::string patchName = pElem->Attribute("name");
		int patchNumber = -1;
		pElem->QueryIntAttribute("number", &patchNumber);
		if (-1 == patchNumber || patchName.empty())
			continue;

		pElem->QueryIntAttribute("port", &midiOutPortNumber);
		pElem->QueryValueAttribute("type", &tmp);
		Patch::PatchType patchType = Patch::ptNormal;
		if (tmp == "normal")
			patchType = Patch::ptNormal;
		else if (tmp == "toggle")
			patchType = Patch::ptToggle;
		else if (tmp == "momentary")
			patchType = Patch::ptMomentary;

		TiXmlHandle hRoot(NULL);
		hRoot = TiXmlHandle(pElem);

		for (TiXmlElement * childElem = hRoot.FirstChild().Element(); 
			 childElem; 
			 childElem = childElem->NextSiblingElement())
		{
			if (childElem->ValueStr() != "bytestring")
				continue;
		
			tmp = childElem->Attribute("name");
			if (tmp == "A")
				byteStringA = childElem->GetText();
			else if (tmp == "B")
				byteStringB = childElem->GetText();
		}

		Bytes bytesA;
		int retval = ::ValidateString(byteStringA, bytesA);
		if (-1 != retval)
		{
			Bytes bytesB;
			retval = ::ValidateString(byteStringB, bytesB);
			if (-1 != retval)
				mEngine->AddPatch(patchNumber, patchName, patchType, midiOutPortNumber, bytesA, bytesB);
		}
	}
}

void
EngineLoader::LoadBanks(TiXmlElement * pElem)
{
	for ( ; pElem; pElem = pElem->NextSiblingElement())
	{
		if (pElem->ValueStr() != "bank")
			continue;

		const std::string bankName = pElem->Attribute("name");
		int bankNumber = -1;
		pElem->QueryIntAttribute("number", &bankNumber);
		if (-1 == bankNumber || bankName.empty())
			continue;

		PatchBank & bank = mEngine->AddBank(bankNumber, bankName);

		TiXmlHandle hRoot(NULL);
		hRoot = TiXmlHandle(pElem);

		for (TiXmlElement * childElem = hRoot.FirstChild().Element(); 
			 childElem; 
			 childElem = childElem->NextSiblingElement())
		{
			if (childElem->ValueStr() != "PatchMap")
				continue;

			int switchNumber = -1;
			childElem->QueryIntAttribute("switch", &switchNumber);
			int patchNumber = -1;
			childElem->Attribute("patch", &patchNumber);
			if (switchNumber <= 0|| -1 == patchNumber)
				continue;

			std::string tmp;
			childElem->QueryValueAttribute("loadState", &tmp);
			const PatchBank::PatchState loadState = GetLoadState(tmp);
			childElem->QueryValueAttribute("unloadState", &tmp);
			const PatchBank::PatchState unloadState = GetLoadState(tmp);

			bank.AddPatchMapping(switchNumber - 1, patchNumber, loadState, unloadState);
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
