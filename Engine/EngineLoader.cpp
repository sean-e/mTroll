#include <strstream>
#include "EngineLoader.h"
#include "MidiControlEngine.h"
#include "PatchBank.h"
#include "Patch.h"
#include "../tinyxml/tinyxml.h"
#include "HexStringUtils.h"
#include "IMidiOutGenerator.h"
#include "ITraceDisplay.h"
#include "../Monome40h/IMonome40h.h"
#include "NormalPatch.h"
#include "TogglePatch.h"
#include "SequencePatch.h"
#include "MomentaryPatch.h"
#include "MetaPatch_ResetBankPatches.h"
#include "MetaPatch_LoadBank.h"
#include "MetaPatch_BankHistory.h"


static PatchBank::PatchState GetLoadState(const std::string & tmpLoad);


EngineLoader::EngineLoader(IMidiOutGenerator * midiOutGenerator,
						   IMainDisplay * mainDisplay,
						   ISwitchDisplay * switchDisplay,
						   ITraceDisplay * traceDisplay) :
	mEngine(NULL),
	mMidiOutGenerator(midiOutGenerator),
	mMainDisplay(mainDisplay),
	mSwitchDisplay(switchDisplay),
	mTraceDisplay(traceDisplay)
{
	for (int idx = 0; idx < 4; ++idx)
		mAdcEnables[idx] = adc_default;
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

	mMidiOutGenerator->OpenMidiOuts();
	mEngine->CompleteInit(mAdcCalibration);

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

	// <midiDevice port="1" outIdx="3" activityIndicatorId="100" />
	pChildElem = hRoot.FirstChild("midiDevices").FirstChild().Element();
	for ( ; pChildElem; pChildElem = pChildElem->NextSiblingElement())
	{
		int deviceIdx = 1;
		int port = 1;
		int activityIndicatorId = 0;

		pChildElem->QueryIntAttribute("outIdx", &deviceIdx);
		pChildElem->QueryIntAttribute("port", &port);
		pChildElem->QueryIntAttribute("activityIndicatorId", &activityIndicatorId);

		mMidiOutPortToDeviceIdxMap[port] = deviceIdx;
		mMidiOutGenerator->CreateMidiOut(mMidiOutPortToDeviceIdxMap[port], activityIndicatorId);
	}
	mEngine = new MidiControlEngine(mMainDisplay, mSwitchDisplay, mTraceDisplay, incrementSwitch, decrementSwitch, modeSwitch);
	mEngine->SetPowerup(powerupBank, powerupPatch, powerupTimeout);
	mEngine->FilterRedundantProgChg(filterPC ? true : false);

	// <expression port="">
	//   <globaExpr inputNumber="1" assignmentNumber="1" channel="" controller="" min="" max="" invert="0" enable="" />
	//   <adc inputNumber="" enable="" />
	// </expression>
	ExpressionPedals & globalPedals = mEngine->GetPedals();
	pChildElem = hRoot.FirstChild("expression").FirstChild().Element();
	for ( ; pChildElem; pChildElem = pChildElem->NextSiblingElement())
	{
		if (pChildElem->ValueStr() == "globalExpr")
		{
			LoadExpressionPedalSettings(pChildElem, globalPedals);
		}
		else if (pChildElem->ValueStr() == "adc")
		{
			int exprInputNumber = -1;
			int enable = 1;
			int minVal = 0;
			int maxVal = PedalCalibration::MaxAdcVal;

			pChildElem->QueryIntAttribute("inputNumber", &exprInputNumber);
			pChildElem->QueryIntAttribute("enable", &enable);
			pChildElem->QueryIntAttribute("minimumAdcVal", &minVal);
			pChildElem->QueryIntAttribute("maximumAdcVal", &maxVal);

			if (exprInputNumber > 0 &&
				exprInputNumber <= ExpressionPedals::PedalCount)
			{
				if (enable)
					mAdcEnables[exprInputNumber - 1] = adc_forceOn;
				else
					mAdcEnables[exprInputNumber - 1] = adc_forceOff;

				mAdcCalibration[exprInputNumber - 1].Init(minVal, maxVal);
			}
		}
	}

	int midiOutPortNumber = -1;
	pChildElem = hRoot.FirstChild("expression").Element();
	if (pChildElem)
	{
		pChildElem->QueryIntAttribute("port", &midiOutPortNumber);
		if (-1 == midiOutPortNumber && mMidiOutPortToDeviceIdxMap.begin() != mMidiOutPortToDeviceIdxMap.end())
			midiOutPortNumber = (*mMidiOutPortToDeviceIdxMap.begin()).second;
		IMidiOut * globalExprPedalMidiOut = mMidiOutGenerator->GetMidiOut(mMidiOutPortToDeviceIdxMap[midiOutPortNumber]);
		if (!globalExprPedalMidiOut)
			return false;

		globalPedals.InitMidiOut(globalExprPedalMidiOut);
	}

	return true;
}

void
EngineLoader::LoadPatches(TiXmlElement * pElem)
{
	for ( ; pElem; pElem = pElem->NextSiblingElement())
	{
		if (pElem->ValueStr() == "engineMetaPatch")
		{
			const std::string patchName = pElem->Attribute("name");
			int patchNumber = -1;
			pElem->QueryIntAttribute("number", &patchNumber);
			if (-1 == patchNumber || patchName.empty())
				continue;

			std::string tmp;
			pElem->QueryValueAttribute("action", &tmp);
			if (tmp.empty())
				continue;

			if (tmp == "ResetBankPatches")
				 mEngine->AddPatch(new MetaPatch_ResetBankPatches(mEngine, patchNumber, patchName));
			else if (tmp == "LoadBank")
			{
				int bankNumber = -1;
				pElem->QueryIntAttribute("bankNumber", &bankNumber);
				if (-1 != bankNumber)
					mEngine->AddPatch(new MetaPatch_LoadBank(mEngine, patchNumber, patchName, bankNumber));
			}
			else if (tmp == "BankHistoryBackward")
				 mEngine->AddPatch(new MetaPatch_BankHistoryBackward(mEngine, patchNumber, patchName));
			else if (tmp == "BankHistoryForward")
				 mEngine->AddPatch(new MetaPatch_BankHistoryForward(mEngine, patchNumber, patchName));
			else if (tmp == "BankHistoryRecall")
				 mEngine->AddPatch(new MetaPatch_BankHistoryRecall(mEngine, patchNumber, patchName));
			continue;
		}
		
		if (pElem->ValueStr() != "patch")
			continue;

		std::string tmp;
		std::string midiByteStringA;
		std::string midiByteStringB;
		int midiOutPortNumber = 1;

		const std::string patchName = pElem->Attribute("name");
		int patchNumber = -1;
		pElem->QueryIntAttribute("number", &patchNumber);
		if (-1 == patchNumber || patchName.empty())
			continue;

		pElem->QueryIntAttribute("port", &midiOutPortNumber);

		TiXmlHandle hRoot(NULL);
		hRoot = TiXmlHandle(pElem);

		TiXmlElement * childElem;
		for (childElem = hRoot.FirstChild().Element(); 
			 childElem; 
			 childElem = childElem->NextSiblingElement())
		{
			if (childElem->ValueStr() != "midiByteString")
				continue;
		
			childElem->QueryValueAttribute("name", &tmp);
			if (tmp == "A")
				midiByteStringA = childElem->GetText();
			else if (tmp == "B")
				midiByteStringB = childElem->GetText();
		}

		Bytes bytesA;
		int retval = ::ValidateString(midiByteStringA, bytesA);
		if (-1 == retval)
			continue;

		Bytes bytesB;
		retval = ::ValidateString(midiByteStringB, bytesB);
		if (-1 == retval)
			continue;

		Patch * newPatch = NULL;
		IMidiOut * midiOut = mMidiOutGenerator->GetMidiOut(mMidiOutPortToDeviceIdxMap[midiOutPortNumber]);
		pElem->QueryValueAttribute("type", &tmp);
		if (tmp == "normal")
			newPatch = new NormalPatch(patchNumber, patchName, midiOutPortNumber, midiOut, bytesA, bytesB);
		else if (tmp == "toggle")
			newPatch = new TogglePatch(patchNumber, patchName, midiOutPortNumber, midiOut, bytesA, bytesB);
		else if (tmp == "momentary")
			newPatch = new MomentaryPatch(patchNumber, patchName, midiOutPortNumber, midiOut, bytesA, bytesB);
		else if (tmp == "sequence")
		{
			SequencePatch * seqpatch = new SequencePatch(patchNumber, patchName, midiOutPortNumber, midiOut);
			for (childElem = hRoot.FirstChild().Element(); 
				 childElem && seqpatch; 
				 childElem = childElem->NextSiblingElement())
			{
				if (childElem->ValueStr() != "midiByteString")
					continue;
			
				midiByteStringA = childElem->GetText();
				Bytes bytesA;
				int retval = ::ValidateString(midiByteStringA, bytesA);
				if (-1 == retval)
					continue;
					
				seqpatch->AddString(bytesA);
			}
			
			newPatch = seqpatch;
		}

		if (!newPatch)
			continue;

		mEngine->AddPatch(newPatch);
		ExpressionPedals & pedals = newPatch->GetPedals();
		for (childElem = hRoot.FirstChild().Element(); 
			 childElem; 
			 childElem = childElem->NextSiblingElement())
		{
			if (childElem->ValueStr() == "localExpr")
			{
				// <localExpr inputNumber="1" assignmentNumber="1" channel="" controller="" min="" max="" invert="0" enable="" />
				LoadExpressionPedalSettings(childElem, pedals);
			}
			else if (childElem->ValueStr() == "globalExpr")
			{
				// <globalExpr inputNumber="" enable="" />
				int exprInputNumber = -1;
				int enable = 1;

				childElem->QueryIntAttribute("inputNumber", &exprInputNumber);
				childElem->QueryIntAttribute("enable", &enable);

				if (exprInputNumber > 0 &&
					exprInputNumber <= ExpressionPedals::PedalCount)
				{
					ExpressionPedals & pedals = newPatch->GetPedals();
					pedals.EnableGlobal(exprInputNumber - 1, !!enable);
				}
			}
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
			if (childElem->ValueStr() == "PatchMap")
			{
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
			else if (childElem->ValueStr() == "ExclusiveSwitchGroup")
			{
				std::string switchesStr(childElem->GetText());
				PatchBank::GroupSwitches * switches = new PatchBank::GroupSwitches;

				// split switchesStr - space token
				while (switchesStr.size())
				{
					const int curSwitch = ::atoi(switchesStr.c_str());
					if (curSwitch)
						switches->insert(curSwitch - 1);

					const int spacePos = switchesStr.find(' ');
					if (std::string::npos == spacePos)
						break;

					switchesStr = switchesStr.substr(spacePos + 1);
				}

				bank.CreateExclusiveGroup(switches);
			}
		}
	}
}

void
EngineLoader::LoadExpressionPedalSettings(TiXmlElement * childElem, 
										  ExpressionPedals &pedals)
{
	int exprInputNumber = -1;
	int assignmentIndex = -1;
	int channel = -1;
	int controller = -1;
	int maxVal = 127;
	int minVal = 0;
	int enable = 1;
	int invert = 0;

	childElem->QueryIntAttribute("inputNumber", &exprInputNumber);
	childElem->QueryIntAttribute("assignmentNumber", &assignmentIndex);
	childElem->QueryIntAttribute("channel", &channel);
	childElem->QueryIntAttribute("controller", &controller);
	childElem->QueryIntAttribute("max", &maxVal);
	childElem->QueryIntAttribute("min", &minVal);
	childElem->QueryIntAttribute("invert", &invert);
	childElem->QueryIntAttribute("enable", &enable);

	if (enable &&
		exprInputNumber > 0 &&
		exprInputNumber <= ExpressionPedals::PedalCount &&
		assignmentIndex > 0 &&
		assignmentIndex < 3 &&
		channel >= 0 &&
		channel < 16 &&
		controller >= 0 &&
		controller < 128)
	{
		pedals.Init(exprInputNumber - 1, assignmentIndex - 1, !!invert, channel - 1, controller, minVal, maxVal);
	}

	if (enable && mAdcEnables[exprInputNumber - 1] == adc_default)
		mAdcEnables[exprInputNumber - 1] = adc_used;
}

void
EngineLoader::InitMonome(IMonome40h * monome)
{
	for (int idx = 0; idx < 4; ++idx)
	{
		if (mAdcEnables[idx] == adc_used ||
			mAdcEnables[idx] == adc_forceOn)
		{
			monome->EnableAdc(idx, true);
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "  ADC port " << idx << " enabled" << std::endl << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
		}
		else
		{
			monome->EnableAdc(idx, false);
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "  ADC port " << idx << " disabled" << std::endl << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
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
