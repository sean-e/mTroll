/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2015,2018,2020-2024 Sean Echevarria
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

#include <strstream>
#include "EngineLoader.h"
#include "MidiControlEngine.h"
#include "PatchBank.h"
#include "Patch.h"
#include "../tinyxml/tinyxml.h"
#include "HexStringUtils.h"
#include "ISwitchDisplay.h"
#include "IMidiOutGenerator.h"
#include "IMidiInGenerator.h"
#include "ITraceDisplay.h"
#include "../Monome40h/IMonome40h.h"
#include "NormalPatch.h"
#include "TogglePatch.h"
#include "SequencePatch.h"
#include "MomentaryPatch.h"
#include "MetaPatch_ResetBankPatches.h"
#include "MetaPatch_LoadBank.h"
#include "MetaPatch_BankHistory.h"
#include "MetaPatch_SyncAxeFx.h"
#include "MidiCommandString.h"
#include "RefirePedalCommand.h"
#include "SleepCommand.h"
#include "AxeFxManager.h"
#include "AxeFx3Manager.h"
#include "IMidiIn.h"
#include "ITrollApplication.h"
#include "AxeTogglePatch.h"
#include "AxeFxProgramChange.h"
#include "PatchListSequencePatch.h"
#include "MetaPatch_ResetExclusiveGroup.h"
#include "MetaPatch_BankNav.h"
#include "PersistentPedalOverridePatch.h"
#include "MetaPatch_ResetPatch.h"
#include "AxeMomentaryPatch.h"
#include "MetaPatch_AxeFxNav.h"
#include "CompositeTogglePatch.h"
#include "ControllerTogglePatch.h"
#include "ControllerInputMonitor.h"
#include "RepeatingMomentaryPatch.h"
#include "RepeatingTogglePatch.h"
#include "SleepRandomCommand.h"
#include "DynamicMidiCommand.h"


#ifdef _MSC_VER
#pragma warning(disable:4482)
#endif


static PatchBank::PatchState GetLoadState(const std::string & tmpLoad);
static PatchBank::PatchSyncState GetSyncState(const std::string & tmpLoad);
static PatchBank::SecondFunctionOperation GetSecondFuncOp(const std::string & tmp);
static void ReadTwoIntValues(std::string tmp, int & val1, int & val2);
static int StringToNoteValue(const char *noteData);

extern const std::map<const std::string, int> kMidiNoteValues;


EngineLoader::EngineLoader(ITrollApplication * app,
						   IMidiOutGenerator * midiOutGenerator,
						   IMidiInGenerator * midiInGenerator,
						   IMainDisplay * mainDisplay,
						   ISwitchDisplay * switchDisplay,
						   ITraceDisplay * traceDisplay) :
	mApp(app),
	mMidiOutGenerator(midiOutGenerator),
	mMidiInGenerator(midiInGenerator),
	mMainDisplay(mainDisplay),
	mSwitchDisplay(switchDisplay),
	mTraceDisplay(traceDisplay)
{
	for (auto & adcEnable : mAdcEnables)
		adcEnable = adc_default;
}

MidiControlEnginePtr
EngineLoader::CreateEngine(const std::string & engineSettingsFile)
{
	TiXmlDocument doc(engineSettingsFile);
	if (!doc.LoadFile()) 
	{
		std::string err = doc.ErrorDesc();
		if (mTraceDisplay)
		{
			std::strstream traceMsg;
			traceMsg << "Error loading config file: failed to load file " << err << '\n' << std::ends;
			mTraceDisplay->Trace(std::string(traceMsg.str()));
		}
		return mEngine;
	}

	TiXmlHandle hDoc(&doc);
	TiXmlElement* pElem = hDoc.FirstChildElement().Element();
	// should always have a valid root but handle gracefully if it doesn't
	if (!pElem) 
	{
		if (mTraceDisplay)
		{
			std::strstream traceMsg;
			traceMsg << "Error loading config file: invalid config file\n" << std::ends;
			mTraceDisplay->Trace(std::string(traceMsg.str()));
		}
		return mEngine;
	}

	if (pElem->ValueStr() != "MidiControlSettings")
	{
		if (mTraceDisplay)
		{
			std::strstream traceMsg;
			traceMsg << "Error loading config file: invalid config file\n" << std::ends;
			mTraceDisplay->Trace(std::string(traceMsg.str()));
		}
		return mEngine;
	}

	TiXmlHandle hRoot(nullptr);
	hRoot = TiXmlHandle(pElem);

	// load DeviceChannelMap before SystemConfig so that pedals can use Devices
	pElem = hRoot.FirstChild("DeviceChannelMap").FirstChildElement().Element();
	if (pElem)
		LoadDeviceChannelMap(pElem);

	// init colors before system config since midi indicator can be defaulted
	InitDefaultLedPresetColors();
	pElem = hRoot.FirstChild("LedPresetColors").FirstChildElement().Element();
	if (pElem)
		LoadLedPresetColors(pElem);

	pElem = hRoot.FirstChild("LedDefaultColors").FirstChildElement().Element();
	if (pElem)
		LoadLedDefaultColors(pElem);

	// generate patch numbers so that patches referenced by name in SystemConfig can 
	// be resolved, and so that patches can reference other patches that have not
	// been defined before the patch that is referencing the other
	pElem = hRoot.FirstChild("patches").FirstChildElement().Element();
	GeneratePatchNumbers(pElem);

	pElem = hRoot.FirstChild("SystemConfig").Element();
	if (!LoadSystemConfig(pElem))
	{
		if (mTraceDisplay)
		{
			std::strstream traceMsg;
			traceMsg << "Error loading config file: invalid SystemConfig\n" << std::ends;
			mTraceDisplay->Trace(std::string(traceMsg.str()));
		}
		return mEngine;
	}

	GenerateDefaultNotePatches();

	pElem = hRoot.FirstChild("patches").FirstChildElement().Element();
	LoadPatches(pElem);

	pElem = hRoot.FirstChild("banks").FirstChildElement().Element();
	LoadBanks(pElem);

	mSwitchDisplay->UpdatePresetColors(mLedPresetColors);

	mMidiOutGenerator->OpenMidiOuts();
	if (mMidiInGenerator)
		mMidiInGenerator->OpenMidiIns();

	if (mAxeFx3Manager)
	{
		IMidiOutPtr midiOut;
		std::vector<std::string> axeNames = {"AxeFx3", "Axe-Fx3", "Axe-Fx 3", "AxeFxIII", "Axe-Fx III" };
		for (const auto& axeName : axeNames)
		{
			std::map<std::string, int>::iterator it = mDevicePorts.find(axeName);
			if (it != mDevicePorts.end())
			{
				int port = mDevicePorts[axeName];
				midiOut = mMidiOutGenerator->GetMidiOut(mMidiOutPortToDeviceIdxMap[port]);
				break;
			}
		}

		if (!midiOut && mAxe3SyncPort != -1 && !mAxe3DeviceName.empty())
		{
			// user's device name was not in axeNames, so use the port they specified
			for (const auto & cur : mDevicePorts)
			{
				if (cur.second == mAxe3SyncPort && cur.first == mAxe3DeviceName)
				{
					midiOut = mMidiOutGenerator->GetMidiOut(mMidiOutPortToDeviceIdxMap[mAxe3SyncPort]);
					break;
				}
			}
		}

		mAxeFx3Manager->CompleteInit(midiOut);
	}

	if (mAxeFxManager)
	{
		IMidiOutPtr midiOut;
		std::vector<std::string> axeNames = { "AxeFx", "Axe-Fx" };
		for (const auto& axeName : axeNames)
		{
			std::map<std::string, int>::iterator it = mDevicePorts.find(axeName);
			if (it != mDevicePorts.end())
			{
				int port = mDevicePorts[axeName];
				midiOut = mMidiOutGenerator->GetMidiOut(mMidiOutPortToDeviceIdxMap[port]);
				break;
			}
		}

		mAxeFxManager->CompleteInit(midiOut);
	}

	if (!mPowerupBankName.empty())
		mPowerupBank = mEngine->GetBankNumber(mPowerupBankName);
	mEngine->SetPowerup(mPowerupBank);
	mEngine->CompleteInit(mAdcCalibration, LookUpColor("mtroll", "*", 1));

	MidiControlEnginePtr createdEngine = mEngine;
	mEngine = nullptr;
	return createdEngine;
}

bool
EngineLoader::LoadSystemConfig(TiXmlElement * pElem)
{
	if (!pElem)
		return false;

	int filterPC = 0;
	int incrementSwitch = -1;
	int decrementSwitch = -1;
	int modeSwitch = -1;

	pElem->QueryIntAttribute("filterPC", &filterPC);

	TiXmlHandle hRoot(nullptr);
	hRoot = TiXmlHandle(pElem);

	TiXmlElement * pChildElem = hRoot.FirstChild("powerup").Element();
	if (pChildElem)
	{
		pChildElem->QueryIntAttribute("bank", &mPowerupBank);
		if (0 == mPowerupBank)
		{
			std::string bankName;
			// QueryValueAttribute does not work with string when there are 
			// spaces (truncated at whitespace); use Attribute instead
			if (pChildElem->Attribute("bankName"))
				bankName = pChildElem->Attribute("bankName");
			if (!bankName.empty())
				mPowerupBankName = bankName;
		}
	}

	pChildElem = hRoot.FirstChild("switches").FirstChildElement().Element();
	for ( ; pChildElem; pChildElem = pChildElem->NextSiblingElement())
	{
		std::string name;
		int id = 0;
		pChildElem->QueryValueAttribute("command", &name);
		if (name.empty())
			pChildElem->QueryValueAttribute("name", &name); // original version
		pChildElem->QueryIntAttribute("id", &id);

		if (name == "increment" && id > 0)
			incrementSwitch = id;
		else if (name == "decrement" && id > 0)
			decrementSwitch = id;
		else if (name == "menu" && id > 0)
			modeSwitch = id;
		else if (name == "mode" && id > 0) // original version
			modeSwitch = id;
	}

	// <midiDevice port="1" outIdx="3" activityIndicatorId="100" />
	// <midiDevice port="2" out="Axe-Fx II" in="Axe-Fx II" activityIndicatorId="100" />
	pChildElem = hRoot.FirstChild("MidiDevices").FirstChildElement().Element();
	if (!pChildElem)
		pChildElem = hRoot.FirstChild("midiDevices").FirstChildElement().Element();
	for ( ; pChildElem; pChildElem = pChildElem->NextSiblingElement())
	{
		int deviceIdx = -1;
		int inDeviceIdx = -1;
		int activityIndicatorId = -1;
		int port = 1;
		std::string inDevice, outDevice;

		if (pChildElem->Attribute("in"))
			inDevice = pChildElem->Attribute("in");
		if (pChildElem->Attribute("out"))
			outDevice = pChildElem->Attribute("out");
		pChildElem->QueryIntAttribute("inIdx", &inDeviceIdx);
		pChildElem->QueryIntAttribute("port", &port);
		pChildElem->QueryIntAttribute("outIdx", &deviceIdx);
		pChildElem->QueryIntAttribute("activityIndicatorId", &activityIndicatorId);

		unsigned int ledActiveColor = -1;
		unsigned int ledInactiveColor = -1; // ignored
		LoadElementColorAttributes(pChildElem, ledActiveColor, ledInactiveColor);

		if (-1 == ledActiveColor)
			ledActiveColor = LookUpColor("midi", "*", 1);

		if (!outDevice.empty())
		{
			unsigned int idx = GetMidiOutDeviceIndex(outDevice);
			if (-1 != idx)
				deviceIdx = idx;
			else
			{
				if (mTraceDisplay) 
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file midiDevices section: MidiOut device name not found: " << outDevice << '\n' << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}

				if (-1 == deviceIdx)
				{
					// do not default to -1 if name was specified (if idx specified, then try it)
					continue;
				}
			}
		}

		if (!inDevice.empty() && mMidiInGenerator)
		{
			unsigned int idx = GetMidiInDeviceIndex(inDevice);
			if (-1 != idx)
				inDeviceIdx = idx;
			else
			{
				if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file midiDevices section: MidiIn device name not found: " << inDevice << '\n' << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}

				if (-1 == inDeviceIdx)
				{
					// do not default to -1 if name was specified (if idx specified, then try it)
					continue;
				}
			}
		}

		if (-1 == inDeviceIdx || -1 != deviceIdx)
		{
			if (-1 == deviceIdx)
			{
				// maintain back compat with original behavior
				deviceIdx = 1;
			}

			// first successful port binding is last one attempted for that port
			if (mMidiOutPortToDeviceIdxMap.find(port) == mMidiOutPortToDeviceIdxMap.end())
			{
				if (mMidiOutGenerator->CreateMidiOut(deviceIdx, activityIndicatorId, ledActiveColor))
					mMidiOutPortToDeviceIdxMap[port] = deviceIdx;
			}
		}

		if (-1 != inDeviceIdx && mMidiInGenerator)
		{
			// first successful port binding is last one attempted for that port
			if (mMidiInPortToDeviceIdxMap.find(port) == mMidiInPortToDeviceIdxMap.end())
			{
				IMidiInPtr midiIn = mMidiInGenerator->CreateMidiIn(inDeviceIdx);
				if (midiIn)
				{
					mMidiInPortToDeviceIdxMap[port] = inDeviceIdx;
					if (mAxeFx3Manager && port == mAxe3SyncPort)
						mAxeFx3Manager->SubscribeToMidiIn(midiIn);
					if (mAxeFxManager && port == mAxeSyncPort)
						mAxeFxManager->SubscribeToMidiIn(midiIn);
					if (mEdpManager && port == mEdpPort)
						mEdpManager->SubscribeToMidiIn(midiIn, inDeviceIdx);
				}
			}
		}
	}

	IMidiOutPtr engOut; // for direct program change, control change, and MIDI clock
	if (!mMidiOutPortToDeviceIdxMap.empty())
	{
		// first device listed in config MidiDevices section is used for interactive 
		// program change, control change, and MIDI clock by default 
		// override via implementation of issue #12
		engOut = mMidiOutGenerator->GetMidiOut((*mMidiOutPortToDeviceIdxMap.begin()).second);
	}

	mEngine = std::make_shared<MidiControlEngine>(mApp, mMainDisplay, mSwitchDisplay, mTraceDisplay,
		mMidiOutGenerator, engOut, mAxeFxManager, mAxeFx3Manager, mEdpManager, incrementSwitch, decrementSwitch, modeSwitch);
	mEngine->FilterRedundantProgChg(filterPC ? true : false);

	// <expression port="">
	//   <globaExpr inputNumber="1" assignmentNumber="1" channel="" controller="" min="" max="" invert="0" enable="" />
	//   <adc inputNumber="" enable="" />
	// </expression>
	ExpressionPedals & globalPedals = mEngine->GetPedals();
	int defaultExprPedalMidiOutPortNumber = -1;
	pChildElem = hRoot.FirstChild("expression").Element();
	if (pChildElem)
	{
		pChildElem->QueryIntAttribute("port", &defaultExprPedalMidiOutPortNumber);
		if (-1 == defaultExprPedalMidiOutPortNumber && mMidiOutPortToDeviceIdxMap.begin() != mMidiOutPortToDeviceIdxMap.end())
			defaultExprPedalMidiOutPortNumber = (*mMidiOutPortToDeviceIdxMap.begin()).second;
		if (-1 == defaultExprPedalMidiOutPortNumber)
			return false;
		IMidiOutPtr globalExprPedalMidiOut = mMidiOutGenerator->GetMidiOut(mMidiOutPortToDeviceIdxMap[defaultExprPedalMidiOutPortNumber]);
		if (!globalExprPedalMidiOut)
			return false;

		globalPedals.InitMidiOut(globalExprPedalMidiOut);
	}

	pChildElem = hRoot.FirstChild("expression").FirstChildElement().Element();
	for ( ; pChildElem; pChildElem = pChildElem->NextSiblingElement())
	{
		if (pChildElem->ValueStr() == "globalExpr")
		{
			LoadExpressionPedalSettings(pChildElem, globalPedals, -1);
		}
		else if (pChildElem->ValueStr() == "adc")
		{
			int exprInputNumber = -1;
			int enable = 1;
			int minVal = 0;
			int maxVal = PedalCalibration::MaxAdcVal;
			int	bottomToggleZoneSize = -1;
			int	bottomToggleDeadzoneSize = -1;
			int	topToggleZoneSize = -1;
			int	topToggleDeadzoneSize = -1;

			pChildElem->QueryIntAttribute("inputNumber", &exprInputNumber);
			pChildElem->QueryIntAttribute("enable", &enable);
			pChildElem->QueryIntAttribute("minimumAdcVal", &minVal);
			pChildElem->QueryIntAttribute("maximumAdcVal", &maxVal);
			pChildElem->QueryIntAttribute("bottomToggleZoneSize", &bottomToggleZoneSize);
			pChildElem->QueryIntAttribute("bottomToggleDeadzoneSize", &bottomToggleDeadzoneSize);
			pChildElem->QueryIntAttribute("topToggleZoneSize", &topToggleZoneSize);
			pChildElem->QueryIntAttribute("topToggleDeadzoneSize", &topToggleDeadzoneSize);

			if (exprInputNumber > 0 &&
				exprInputNumber <= ExpressionPedals::PedalCount)
			{
				if (enable)
					mAdcEnables[exprInputNumber - 1] = adc_forceOn;
				else
					mAdcEnables[exprInputNumber - 1] = adc_forceOff;

				PedalCalibration & pc = mAdcCalibration[exprInputNumber - 1];
				pc.mMinAdcVal = minVal;
				pc.mMaxAdcVal = maxVal;
				pc.mBottomToggleZoneSize = bottomToggleZoneSize;
				pc.mBottomToggleDeadzoneSize = bottomToggleDeadzoneSize;
				pc.mTopToggleZoneSize = topToggleZoneSize;
				pc.mTopToggleDeadzoneSize = topToggleDeadzoneSize;
			}
		}
	}

	pChildElem = hRoot.FirstChild("switches").FirstChildElement().Element();
	for ( ; pChildElem; pChildElem = pChildElem->NextSiblingElement())
	{
		std::string name;
		pChildElem->QueryValueAttribute("command", &name);
		if (name.empty())
		{
			// original version used name instead of command
			pChildElem->QueryValueAttribute("name", &name);
		}
		if (name.empty() || name == "mode" || name == "menu" || name == "increment" || name == "decrement")
			continue;

		int switchNumber = -1;
		pChildElem->QueryIntAttribute("id", &switchNumber);
		if (-1 == switchNumber)
			continue;

		if (name == "LoadBank")
		{
			int bnkNum = -1;
			pChildElem->QueryIntAttribute("bankNumber", &bnkNum);
			if (-1 == bnkNum)
			{
				std::string targetBankName;
				// QueryValueAttribute does not work with string when there are 
				// spaces (truncated at whitespace); use Attribute instead
				if (pChildElem->Attribute("bankName"))
				{
					targetBankName = pChildElem->Attribute("bankName");
					if (!targetBankName.empty())
						mEngine->AssignCustomBankLoad(switchNumber, targetBankName);
				}
				continue;
			}

			mEngine->AssignCustomBankLoad(switchNumber, bnkNum);
			continue;
		}

		if (name == "bankLoad")
		{
			// original version of LoadBank
			int bnkNum = -1;
			pChildElem->QueryIntAttribute("bank", &bnkNum);
			if (-1 == bnkNum)
				continue;

			mEngine->AssignCustomBankLoad(switchNumber, bnkNum);
			continue;
		}

		MidiControlEngine::EngineModeSwitch m = MidiControlEngine::kUnassignedSwitchNumber;
		if (name == "recall") m = MidiControlEngine::kModeRecall;
		else if (name == "back") m = MidiControlEngine::kModeBack;
		else if (name == "forward") m = MidiControlEngine::kModeForward;
		else if (name == "time") m = MidiControlEngine::kModeTime;
		else if (name == "bankDescription") m = MidiControlEngine::kModeBankDesc;
		else if (name == "bankDirect") m = MidiControlEngine::kModeBankDirect;
		else if (name == "programChangeDirect") m = MidiControlEngine::kModeProgramChangeDirect;
		else if (name == "controlChangeDirect") m = MidiControlEngine::kModeControlChangeDirect;
		else if (name == "adcDisplay") m = MidiControlEngine::kModeExprPedalDisplay;
		else if (name == "adcOverride") m = MidiControlEngine::kModeAdcOverride;
		else if (name == "testLeds") m = MidiControlEngine::kModeTestLeds;
		else if (name == "toggleTraceWindow") m = MidiControlEngine::kModeToggleTraceWindow;
		else if (name == "midiClockSetup") m = MidiControlEngine::kModeClockSetup;
		else if (name == "midiOutSelect") m = MidiControlEngine::kModeMidiOutSelect;

		if (MidiControlEngine::kUnassignedSwitchNumber != m)
			mEngine->AssignModeSwitchNumber(m, switchNumber);
	}

	return true;
}

int
EngineLoader::GetPatchNumber(const std::string& name) const
{
	if (name.empty())
		return -1;

	Patches::const_iterator it = mPatchMap.find(name);
	if (it == mPatchMap.end())
		return -1;

	return it->second;
}

bool
EngineLoader::IsPatchNumberUsed(int num) const
{
	return mPatchNumbersUsed.find(num) != mPatchNumbersUsed.end();
}

void
SplitDevNames(std::string deviceNamesStr, std::vector<std::string> &devNames)
{
	if (-1 == deviceNamesStr.find(';'))
		return;

	if (';' != deviceNamesStr.at(deviceNamesStr.length() - 1))
		deviceNamesStr += ";";

	std::string::size_type startPos = 0;
	while (startPos < deviceNamesStr.length())
	{
		const std::string::size_type endPos = deviceNamesStr.find(';', startPos);
		if (endPos == -1)
		{
			// not possible since we force an end delimiter before the loop...
			break;
		}

		if (endPos == startPos)
		{
			// multiple consecutive delimiters
			++startPos;
			continue;
		}

		const std::string curDev = deviceNamesStr.substr(startPos, endPos - startPos);
		if (!curDev.empty())
			devNames.emplace_back(curDev);

		startPos = endPos + 1;
	}
}

unsigned int
EngineLoader::GetMidiOutDeviceIndex(std::string outDevice)
{
	_ASSERTE(!outDevice.empty() && mMidiOutGenerator);
	unsigned int deviceIdx = mMidiOutGenerator->GetMidiOutDeviceIndex(outDevice);
	if (deviceIdx != -1)
		return deviceIdx;

	if (-1 == outDevice.find(';'))
		return deviceIdx;

	std::vector<std::string> devNames;
	::SplitDevNames(outDevice, devNames);

	for (const auto &it : devNames)
	{
		deviceIdx = mMidiOutGenerator->GetMidiOutDeviceIndex(it);
		if (deviceIdx != -1)
			return deviceIdx;
	}

	return -1;
}

unsigned int
EngineLoader::GetMidiInDeviceIndex(std::string inDevice)
{
	_ASSERTE(!inDevice.empty() && mMidiInGenerator);
	unsigned int deviceIdx = mMidiInGenerator->GetMidiInDeviceIndex(inDevice);
	if (deviceIdx != -1)
		return deviceIdx;

	if (-1 == inDevice.find(';'))
		return deviceIdx;

	std::vector<std::string> devNames;
	::SplitDevNames(inDevice, devNames);

	for (const auto &it : devNames)
	{
		deviceIdx = mMidiInGenerator->GetMidiInDeviceIndex(it);
		if (deviceIdx != -1)
			return deviceIdx;
	}

	return -1;
}

void
EngineLoader::GeneratePatchNumbers(TiXmlElement* pElem)
{
	constexpr int kFirstAutoGenPatchNumber = 100;
	int autoGeneratedPatchNumber = kFirstAutoGenPatchNumber;

	for (const auto &it : kMidiNoteValues)
	{
		const int patchNumber = autoGeneratedPatchNumber++;
		const std::string patchName(it.first);

		// check for name collision
		if (-1 != GetPatchNumber(patchName))
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error generating default note patches: multiple patches with same name (" << patchName << ")\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		// check for number collision
		if (IsPatchNumberUsed(patchNumber))
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error generating default note patches: multiple patches with same number (" << patchNumber << ")\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		mPatchNumbersUsed.insert(patchNumber);
		mPatchMap[patchName] = patchNumber;
	}

	const int kAutoGeneratedPatchCount = mPatchMap.size();

	for (; pElem; pElem = pElem->NextSiblingElement())
	{
		std::string patchName;

		if (pElem->ValueStr() != "patch")
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: unrecognized Patches item\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		if (pElem->Attribute("name"))
			patchName = pElem->Attribute("name");
		if (patchName.empty())
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: patch definition missing patch name\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		int patchNumber = -1;
		pElem->QueryIntAttribute("number", &patchNumber);
		if (-1 == patchNumber)
		{
			if (!mPatchMap.empty() && autoGeneratedPatchNumber == kFirstAutoGenPatchNumber)
			{
				if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: patch definition missing patch number\n" << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}

			patchNumber = autoGeneratedPatchNumber++;
		}
		else if (autoGeneratedPatchNumber != (kFirstAutoGenPatchNumber + kAutoGeneratedPatchCount))
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: mixed use of patch numbers -- all patch definitions should have number or no patch definitions should have number\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		// check for name collision
		if (-1 != GetPatchNumber(patchName))
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: multiple patches with same name (" << patchName << ")\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		// check for number collision
		if (IsPatchNumberUsed(patchNumber))
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: multiple patches with same number (" << patchNumber << ")\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		mPatchNumbersUsed.insert(patchNumber);
		mPatchMap[patchName] = patchNumber;
	}

	_ASSERTE(mPatchMap.size() == mPatchNumbersUsed.size());
}

void
EngineLoader::GenerateDefaultNotePatches()
{
	for (const auto &it : kMidiNoteValues)
	{
		const std::string patchName(it.first);
		PatchCommands cmds, cmds2;

		{
			Bytes bytes;
			bytes.push_back(0x90); // NoteOn
			bytes.push_back(it.second);
			bytes.push_back(0);
			cmds.push_back(std::make_shared<DynamicMidiCommand>(nullptr, bytes, true, true));
		}

		{
			Bytes bytes;
			bytes.push_back(0x80); // NoteOff
			bytes.push_back(it.second);
			bytes.push_back(0);
			cmds2.push_back(std::make_shared<DynamicMidiCommand>(nullptr, bytes, true, false));
		}

		auto newPatch = std::make_shared<MomentaryPatch>(mPatchMap[patchName], patchName, nullptr, cmds, cmds2);
		mEngine->AddPatch(newPatch);
	}
}

void
EngineLoader::LoadPatches(TiXmlElement * pElem)
{
	bool dynamicPortInit = false;
	for ( ; pElem; pElem = pElem->NextSiblingElement())
	{
		std::string patchName;

		if (pElem->ValueStr() != "patch")
			continue;
		if (pElem->Attribute("name"))
			patchName = pElem->Attribute("name");

		const int patchNumber = GetPatchNumber(patchName);
		if (-1 == patchNumber)
			continue;

		std::string patchType;
		pElem->QueryValueAttribute("type", &patchType);

		bool isDynamicDevice = false;
		std::string device;
		if (pElem->Attribute("device"))
		{
			device = pElem->Attribute("device");
			if (device == "*")
			{
				isDynamicDevice = true;
				device.clear();
			}
		}

		// optional default channel in <patch> so that it doesn't need to be specified in each patch command
		int patchDefaultCh = -1;
		bool isDynamicCh = false;
		{
			std::string chStr;
			pElem->QueryValueAttribute("channel", &chStr);
			if (chStr.empty() && !device.empty())
				chStr = mDeviceChannels[device];

			if (!chStr.empty())
			{
				if (chStr == "*")
				{
					// potentially isDynamicCh
					isDynamicDevice = true;
				}
				else
				{
					patchDefaultCh = ::atoi(chStr.c_str()) - 1;
					if (patchDefaultCh < 0 || patchDefaultCh > 15)
					{
						if (mTraceDisplay)
						{
							std::strstream traceMsg;
							traceMsg << "Error loading config file: invalid command channel in patch " << patchName << '\n' << std::ends;
							mTraceDisplay->Trace(std::string(traceMsg.str()));
						}
						patchDefaultCh = -1;
					}
				}
			}
		}

		bool isDynamicPort = false;
		int midiOutPortNumber = -1;
		if (TIXML_WRONG_TYPE == pElem->QueryIntAttribute("port", &midiOutPortNumber))
		{
			const std::string portStr(pElem->Attribute("port"));
			if (portStr == "*")
				isDynamicPort = true;
		}

		if (-1 == midiOutPortNumber && !isDynamicPort && device == "*")
			isDynamicPort = true;

		IMidiOutPtr midiOut;
		if (isDynamicPort)
		{
			if (!dynamicPortInit)
			{
				DynamicMidiCommand::SetDynamicPortData(mMidiOutGenerator, mMidiOutPortToDeviceIdxMap);
				dynamicPortInit = true;
			}
		}
		else
		{
			if (-1 == midiOutPortNumber && !device.empty())
				midiOutPortNumber = mDevicePorts[device]; // see if a port mapping was set up for the device

			if (-1 == midiOutPortNumber)
				midiOutPortNumber = 1;

			if (mMidiOutPortToDeviceIdxMap.find(midiOutPortNumber) != mMidiOutPortToDeviceIdxMap.end())
			{
				midiOut = mMidiOutGenerator->GetMidiOut(mMidiOutPortToDeviceIdxMap[midiOutPortNumber]);
			}
			else if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading patch: device port not mapped to midi device for patchname: " << patchName << '\n' << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
		}

		IAxeFxPtr mgr = GetAxeMgr(pElem);
		PatchCommands cmds, cmds2;
		std::vector<int> intList;
		std::vector<std::pair<int, int>> pairIntListA, pairIntListB;
		TiXmlHandle hRoot(nullptr);
		hRoot = TiXmlHandle(pElem);

		TiXmlElement * childElem;
		for (childElem = hRoot.FirstChildElement().Element(); 
			 childElem; 
			 childElem = childElem->NextSiblingElement())
		{
			bool isDynamicVel = false;
			Bytes bytes;
			const std::string& patchElement = childElem->ValueStr();
			std::string group;
			childElem->QueryValueAttribute("group", &group);
			if (group.empty())
			{
				// 'group' was formerly 'name' (when only a single A and B were supported)
				childElem->QueryValueAttribute("name", &group);
			}

			if (patchElement == "midiByteString")
			{
				std::string patchCommandString;
				if (childElem->GetText())
					patchCommandString = childElem->GetText();
				if (-1 == ::ValidateString(patchCommandString, bytes))
				{
					if (mTraceDisplay)
					{
						std::strstream traceMsg;
						traceMsg << "Error loading config file: invalid midiByteString in patch " << patchName << '\n' << std::ends;
						mTraceDisplay->Trace(std::string(traceMsg.str()));
					}
					continue;
				}
			}
			else if (patchElement == "RefirePedal")
			{
				int pedalNumber = -1;
				// pedal values are 1-4
				childElem->QueryIntAttribute("pedal", &pedalNumber);
				pedalNumber--;  // but used internally 0-based
				if (-1 < pedalNumber)
				{
					if (group == "B")
						cmds2.push_back(std::make_shared<RefirePedalCommand>(mEngine.get(), pedalNumber));
					else
						cmds.push_back(std::make_shared<RefirePedalCommand>(mEngine.get(), pedalNumber));
				}
				else if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: invalid pedal specified in RefirePedal in patch " << patchName << '\n' << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}
			else if (patchElement == "Sleep")
			{
				std::string sleepAmtStr;
				if (childElem->GetText())
					sleepAmtStr = childElem->GetText();
				const int sleepAmt = ::atoi(sleepAmtStr.c_str());
				if (0 < sleepAmt)
				{
					if (group == "B")
						cmds2.push_back(std::make_shared<SleepCommand>(sleepAmt));
					else
						cmds.push_back(std::make_shared<SleepCommand>(sleepAmt));
				}
				else if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: no (or invalid) time specified in Sleep in patch " << patchName << '\n' << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}
			else if (patchElement == "SleepRandom")
			{
				std::string sleepAmtStr;
				if (childElem->GetText())
					sleepAmtStr = childElem->GetText();
				int sleepMinAmt = 0;
				int sleepMaxAmt = 0;
				::ReadTwoIntValues(sleepAmtStr, sleepMinAmt, sleepMaxAmt);
				if (0 < sleepMinAmt && 0 < sleepMaxAmt)
				{
					if (group == "B")
						cmds2.push_back(std::make_shared<SleepRandomCommand>(sleepMinAmt, sleepMaxAmt));
					else
						cmds.push_back(std::make_shared<SleepRandomCommand>(sleepMinAmt, sleepMaxAmt));
				}
				else if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: no (or invalid) times specified in SleepRandom in patch " << patchName << '\n' << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}
			else if (patchElement == "SetDynamicPort")
			{
				std::string portStr;
				if (childElem->GetText())
					portStr = childElem->GetText();
				const int portVal = ::atoi(portStr.c_str());
				if (0 <= portVal && 16 > portVal)
				{
					if (group == "B")
						cmds2.push_back(std::make_shared<SetDynamicPortCommand>(portVal));
					else
						cmds.push_back(std::make_shared<SetDynamicPortCommand>(portVal));
				}
				else if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: no (or invalid) velocity specified in SetDynamicPort in patch " << patchName << '\n' << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}
			else if (patchElement == "SetDynamicChannel")
			{
				std::string channelStr;
				if (childElem->GetText())
					channelStr = childElem->GetText();
				const int channelVal = ::atoi(channelStr.c_str());
				if (0 <= channelVal && 16 > channelVal)
				{
					if (group == "B")
						cmds2.push_back(std::make_shared<SetDynamicChannelCommand>(channelVal));
					else
						cmds.push_back(std::make_shared<SetDynamicChannelCommand>(channelVal));
				}
				else if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: no (or invalid) velocity specified in SetDynamicChannel in patch " << patchName << '\n' << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}
			else if (patchElement == "SetDynamicChannelVelocity")
			{
				std::string velocityStr;
				if (childElem->GetText())
					velocityStr = childElem->GetText();
				const int velocityVal = ::atoi(velocityStr.c_str());
				if (0 <= velocityVal && 128 > velocityVal)
				{
					if (group == "B")
						cmds2.push_back(std::make_shared<SetDynamicChannelVelocityCommand>(velocityVal));
					else
						cmds.push_back(std::make_shared<SetDynamicChannelVelocityCommand>(velocityVal));
				}
				else if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: no (or invalid) velocity specified in SetDynamicChannelVelocity in patch " << patchName << '\n' << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}
			else if (patchElement == "SetDynamicChannelRandomVelocity")
			{
				std::string randomAmtStr;
				if (childElem->GetText())
					randomAmtStr = childElem->GetText();
				int randomMinVal = 0;
				int randomMaxVal = 0;
				::ReadTwoIntValues(randomAmtStr, randomMinVal, randomMaxVal);
				if (0 <= randomMinVal && 0 <= randomMaxVal && randomMinVal < randomMaxVal && 128 > randomMaxVal)
				{
					if (group == "B")
						cmds2.push_back(std::make_shared<SetDynamicChannelRandomVelocityCommand>(randomMinVal, randomMaxVal));
					else
						cmds.push_back(std::make_shared<SetDynamicChannelRandomVelocityCommand>(randomMinVal, randomMaxVal));
				}
				else if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: no (or invalid) times specified in SetDynamicChannelRandomVelocity in patch " << patchName << '\n' << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}
			else if (patchElement == "localExpr")
				continue;
			else if (patchElement == "globalExpr")
				continue;
			else if (patchElement == "patchListItem" || patchElement == "patchListItemName")
			{
				std::string patchStr;
				if (childElem->GetText())
					patchStr = childElem->GetText();

				int num;
				if (patchElement == "patchListItemName")
					num = GetPatchNumber(patchStr);
				else
					num = ::atoi(patchStr.c_str());

				if (-1 != num)
				{
					intList.push_back(num);
				}
				else if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: no or invalid patch number or name specified in patchListItem/patchListItemName in patch " << patchName << '\n' << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}
			else if (patchElement == "refPatch" || patchElement == "refPatchName")
			{
				std::string refStr;
				if (childElem->GetText())
					refStr = childElem->GetText();

				int num;
				if (patchElement == "refPatchName")
					num = GetPatchNumber(refStr);
				else
					num = ::atoi(refStr.c_str());

				if (-1 != num)
				{
					std::string refGroup;
					childElem->QueryValueAttribute("refGroup", &refGroup);
					if (refGroup.empty())
					{
						if (group == "A")
							refGroup = "A";
						else if (group == "B")
							refGroup = "B";
					}

					if (group == "A")
						pairIntListA.emplace_back(num, refGroup == "B");
					else if (group == "B")
						pairIntListB.emplace_back(num, refGroup == "B");
					else if (mTraceDisplay)
					{
						std::strstream traceMsg;
						traceMsg << "Error loading config file: no (or invalid) group specified for refPatch/refPatchName in patch " << patchName << '\n' << std::ends;
						mTraceDisplay->Trace(std::string(traceMsg.str()));
					}
				}
				else if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: no or invalid patch number or name specified in refPatch/refPatchName in patch " << patchName << '\n' << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}
			else
			{
				std::string chStr;
				childElem->QueryValueAttribute("channel", &chStr);
				if (chStr.empty() && !isDynamicDevice)
				{
					std::string device2;
					if (childElem->Attribute("device"))
						device2 = childElem->Attribute("device");
					if (!device2.empty())
						chStr = mDeviceChannels[device2];
					if (patchDefaultCh < 0 || patchDefaultCh > 15)
					{
						if (chStr.empty())
						{
							if (mTraceDisplay)
							{
								std::strstream traceMsg;
								traceMsg << "Error loading config file: command missing channel in patch " << patchName << '\n' << std::ends;
								mTraceDisplay->Trace(std::string(traceMsg.str()));
							}
							continue;
						}
					}
				}

				int ch = -1;
				if (chStr == "*" || (isDynamicDevice && chStr.empty()))
				{
					// explicitly requested dynamic channel, 
					// but if the command type doesn't support dynamic channel, then default to 0.
					// at present, only ProgramChange and NoteOn support dynamic channel.
					ch = 0;
				}
				else
				{
					if (!chStr.empty())
						ch = ::atoi(chStr.c_str()) - 1;

					if (ch < 0 || ch > 15)
					{
						if (patchDefaultCh < 0 || patchDefaultCh > 15)
						{
							if (mTraceDisplay)
							{
								std::strstream traceMsg;
								traceMsg << "Error loading config file: invalid command channel in patch " << patchName << '\n' << std::ends;
								mTraceDisplay->Trace(std::string(traceMsg.str()));
							}
							continue;
						}
						else
							ch = patchDefaultCh;
					}
				}

				byte cmdByte = 0;
				int data1 = 0, data2 = 0;
				bool useDataByte2 = true;

				if (patchElement == "ProgramChange")
				{
					// <ProgramChange group="A" channel="0" program="0" />
					childElem->QueryIntAttribute("program", &data1);
					if (chStr == "*" || (chStr.empty() && isDynamicDevice))
						isDynamicCh = true;
					cmdByte = 0xc0;
					useDataByte2 = false;
				}
				else if (patchElement == "AxeProgramChange")
				{
					// <AxeProgramChange group="A" device="Axe-Fx" program="0" />
					// use data2 as bank change
					// data1 val range 0 - 1023
					childElem->QueryIntAttribute("program", &data1);
					if (0 > data1 || data1 > 1023)
					{
						if (mTraceDisplay)
						{
							std::strstream traceMsg;
							traceMsg << "Error loading config file: too large a preset specified for AxeProgramChange in patch " << patchName << '\n' << std::ends;
							mTraceDisplay->Trace(std::string(traceMsg.str()));
						}
						continue;
					}

					while (data1 > 127)
					{
						data1 -= 128;
						++data2;
					}

					// bank select
					bytes.push_back(0xb0 | ch);
					bytes.push_back(0);
					bytes.push_back(data2);

					bytes.push_back(0xc0 | ch);
					bytes.push_back(data1);

					if (group == "B")
						cmds2.push_back(std::make_shared<AxeFxProgramChange>(midiOut, bytes, mgr));
					else
						cmds.push_back(std::make_shared<AxeFxProgramChange>(midiOut, bytes, mgr));
				}
				else if (patchElement == "EdpProgramChange")
				{
					// <EdpProgramChange group="A" device="EDP" program="0" />
					// data1 val range 0 - 15
					childElem->QueryIntAttribute("program", &data1);
					if (0 > data1 || data1 > 15)
					{
						if (mTraceDisplay)
						{
							std::strstream traceMsg;
							traceMsg << "Error loading config file: too large a preset specified for EdpProgramChange in patch " << patchName << '\n' << std::ends;
							mTraceDisplay->Trace(std::string(traceMsg.str()));
						}
						continue;
					}

					bytes.push_back(0xc0 | ch);
					bytes.push_back(data1);

					PatchCommands & theCmds = group == "B" ? cmds2 : cmds;
					theCmds.push_back(std::make_shared<MidiCommandString>(midiOut, bytes));
					bytes.clear();

					if (mEdpManager)
					{
						theCmds.push_back(std::make_shared<SleepCommand>(400));
						theCmds.push_back(std::make_shared<MidiCommandString>(midiOut, mEdpManager->GetLocalStateRequest()));
					}
					else
					{
						if (mTraceDisplay)
						{
							std::strstream traceMsg;
							traceMsg << "Error loading config file: EdpProgramChange in patch " << patchName << " requires EDP device.\n" << std::ends;
							mTraceDisplay->Trace(std::string(traceMsg.str()));
						}
					}
				}
				else if (patchElement == "ControlChange")
				{
					// <ControlChange group="A" channel="0" controller="0" value="0" />
					childElem->QueryIntAttribute("controller", &data1);
					childElem->QueryIntAttribute("value", &data2);
					cmdByte = 0xb0;
				}
				else if (patchElement == "NoteOn")
				{
					// <NoteOn group="B" channel="0" note="60" velocity="120" />
					// <NoteOn group="B" channel="0" note="C4" velocity="120" />
					if (chStr == "*" || (chStr.empty() && isDynamicDevice))
						isDynamicCh = true;
					const std::string noteData(childElem->Attribute("note"));
					data1 = StringToNoteValue(noteData.c_str());
					if (TIXML_WRONG_TYPE == childElem->QueryIntAttribute("velocity", &data2))
					{
						const std::string velStr(childElem->Attribute("velocity"));
						if (velStr == "*")
							isDynamicVel = true;
					}
					cmdByte = 0x90;
				}
				else if (patchElement == "NoteOff")
				{
					// <NoteOff group="B" channel="0" note="60" velocity="0" />
					// <NoteOff group="B" channel="0" note="C4" velocity="0" />
					if (chStr == "*" || (chStr.empty() && isDynamicDevice))
						isDynamicCh = true;
					const std::string noteData(childElem->Attribute("note"));
					data1 = ::StringToNoteValue(noteData.c_str());
					childElem->QueryIntAttribute("velocity", &data2);
					cmdByte = 0x80;
				}
				else if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: unrecognized command in patch " << patchName << '\n' << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}

				if (cmdByte)
				{
					if ((data1 > 0xff) || (useDataByte2 && data2 > 0xff))
					{
						if (mTraceDisplay)
						{
							std::strstream traceMsg;
							traceMsg << "Error loading config file: too large a value specified for command in patch " << patchName << '\n' << std::ends;
							mTraceDisplay->Trace(std::string(traceMsg.str()));
						}
						continue;
					}

					if (isDynamicCh)
						bytes.push_back(cmdByte);
					else
						bytes.push_back(cmdByte | ch);
					bytes.push_back(data1);
					if (useDataByte2)
						bytes.push_back(data2);
				}
			}

			if (!bytes.empty())
			{
				// If isDynamicPort, midiOut will be nullptr
				_ASSERTE(!isDynamicPort || !midiOut);

				// commented out check for isDynamicPort because if that is the only dynamic 
				// element, then too many additional commands would need to be supported.
				// Dynamic port then is really only supported for program change, note on, and note off.
				if (group == "B")
				{
					if (/*isDynamicPort ||*/ isDynamicCh || isDynamicVel)
						cmds2.push_back(std::make_shared<DynamicMidiCommand>(midiOut, bytes, isDynamicCh, isDynamicVel));
					else
						cmds2.push_back(std::make_shared<MidiCommandString>(midiOut, bytes));
				}
				else
				{
					if (/*isDynamicPort ||*/ isDynamicCh || isDynamicVel)
						cmds.push_back(std::make_shared<DynamicMidiCommand>(midiOut, bytes, isDynamicCh, isDynamicVel));
					else
						cmds.push_back(std::make_shared<MidiCommandString>(midiOut, bytes));
				}
			}
		}

		int axeFxScene = 0;
		int axeFxBlockId = 0;
		int axeFxBlockChannel = 0;
		bool isAxeFx3EffectBlockBypassCommand = false;
		int overrideCc = -1;
		if (mgr && 
			(patchType.empty() || patchType == "AxeToggle" || patchType == "AxeMomentary") && 
			cmds.empty() && cmds2.empty() &&
			patchDefaultCh != -1)
		{
			if (patchType.empty())
				patchType = "AxeToggle";

			// check for scene attribute
			pElem->QueryIntAttribute("scene", &axeFxScene);

			int axeCc = 0;
			pElem->QueryIntAttribute("cc", &axeCc);
			if (axeCc)
				overrideCc = axeCc; // no default (like Feedback Return) or overridden from default
			else if (mgr->GetModel() == Axe3)
			{
				_ASSERTE(mAxeFx3Manager);
				if (mAxeFx3Manager)
				{
					std::string effectBlockStr;
					std::string effectBlockChannelStr;

					if (pElem->Attribute("axeBlockId"))
						effectBlockStr = pElem->Attribute("axeBlockId");
					if (pElem->Attribute("axeBlockChannel"))
						effectBlockChannelStr = pElem->Attribute("axeBlockChannel");

					if (axeFxScene)
					{
						Bytes b1 = mAxeFx3Manager->GetSceneSelectCommandString(axeFxScene);
						if (!b1.empty())
							cmds.push_back(std::make_shared<MidiCommandString>(midiOut, b1));
					}
					else if (effectBlockStr.empty() && effectBlockChannelStr.empty())
					{
						Bytes b1 = mAxeFx3Manager->GetCommandString(patchName, true);
						if (!b1.empty())
						{
							if (b1.size() > 5 && b1[5] == (byte)AxeFx3MessageIds::EffectBypass)
								isAxeFx3EffectBlockBypassCommand = true;

							cmds.push_back(std::make_shared<MidiCommandString>(midiOut, b1));
						}

						Bytes b2 = mAxeFx3Manager->GetCommandString(patchName, false);
						if (!b2.empty())
							cmds2.push_back(std::make_shared<MidiCommandString>(midiOut, b2));
					}
					else
					{
						Bytes b1 = mAxeFx3Manager->GetBlockChannelSelectCommandString(effectBlockStr, effectBlockChannelStr, axeFxBlockId, axeFxBlockChannel);
						if (!b1.empty())
							cmds.push_back(std::make_shared<MidiCommandString>(midiOut, b1));
					}
				}
			}
			else
			{
				_ASSERTE(mAxeFxManager);
				if (mAxeFxManager)
					axeCc = mAxeFxManager->GetDefaultAxeCc(patchName); // fallback to default
			}

			if (axeCc)
			{
				Bytes bytes;

				// default to A (127)
				bytes.push_back(0xb0 | patchDefaultCh);
				bytes.push_back(axeCc);
				bytes.push_back(axeFxScene ? (axeFxScene - 1) : 127);
				cmds.push_back(std::make_shared<MidiCommandString>(midiOut, bytes));
				bytes.clear();

				if (!axeFxScene)
				{
					// default to B (0)
					bytes.push_back(0xb0 | patchDefaultCh);
					bytes.push_back(axeCc);
					bytes.push_back(0);
					cmds2.push_back(std::make_shared<MidiCommandString>(midiOut, bytes));
					bytes.clear();
				}
			}

			if (!cmds.empty() || !cmds2.empty())
			{
				int invert = 0;
				pElem->QueryIntAttribute("invert", &invert);
				if (1 == invert)
					cmds.swap(cmds2);

				int singleState = 0;
				pElem->QueryIntAttribute("singleState", &singleState);
				if (1 == singleState)
					cmds2.clear();
			}
		}

		bool patchTypeErr = false;
		PatchPtr newPatch = nullptr;
		if (patchType == "normal")
			newPatch = std::make_shared<NormalPatch>(patchNumber, patchName, midiOut, cmds, cmds2);
		else if (patchType == "toggle")
			newPatch = std::make_shared<TogglePatch>(patchNumber, patchName, midiOut, cmds, cmds2);
		else if (patchType == "repeatingToggle")
			newPatch = std::make_shared<RepeatingTogglePatch>(patchNumber, patchName, midiOut, cmds, cmds2);
		else if (patchType == "toggleControlChange")
		{
			if (-1 == patchDefaultCh)
			{
				if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: patch " << patchName << " is of toggleControlChange type but does not specify a device\n" << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}

			int data1 = -1;
			pElem->QueryIntAttribute("controller", &data1);
			if (-1 == data1)
			{
				if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: no controller specified for toggleControlChange patch " << patchName << '\n' << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}

			if (data1 > 0xff)
			{
				if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: too large a controller value specified for toggleControlChange patch " << patchName << '\n' << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}

			ControllerTogglePatchPtr ctp{std::make_shared<ControllerTogglePatch>(patchNumber, patchName, midiOut, patchDefaultCh, data1)};
			newPatch = ctp;

			// [issue: #18] if attribute "inputDevice" exists, set up input monitor
			// 
			// QueryValueAttribute does not work with string when there are 
			// spaces (truncated at whitespace); use Attribute instead
			if (mMidiInGenerator && pElem->Attribute("inputDevice"))
			{
				const std::string inputDeviceName{pElem->Attribute("inputDevice")};
				if (!inputDeviceName.empty())
				{
					int inputDeviceChannel = -1;
					int inputDevicePort = -1;

					const std::string tmp{mDeviceChannels[inputDeviceName]};
					if (!tmp.empty())
					{
						inputDeviceChannel = ::atoi(tmp.c_str()) - 1; // channels in device map are 1-based
						inputDevicePort = mDevicePorts[inputDeviceName];
						if (-1 == inputDeviceChannel || -1 == inputDevicePort)
						{
							if (mTraceDisplay)
							{
								std::strstream traceMsg;
								traceMsg << "Error loading toggleControlChange patch: midiInputDevice channel or port not found: " << inputDeviceName << '\n' << std::ends;
								mTraceDisplay->Trace(std::string(traceMsg.str()));
							}
						}
					}
					else
					{
						if (mTraceDisplay)
						{
							std::strstream traceMsg;
							traceMsg << "Error loading toggleControlChange patch: midiInputDevice name not found (1): " << inputDeviceName << '\n' << std::ends;
							mTraceDisplay->Trace(std::string(traceMsg.str()));
						}
					}

					if (-1 != inputDeviceChannel && -1 != inputDevicePort)
					{
						// get the monitor (one per port) from the engine
						ControllerInputMonitorPtr mon{ mEngine->GetControllerInputMonitor(inputDevicePort) };
						if (!mon)
						{
							// create a new monitor
							mon = std::make_shared<ControllerInputMonitor>(mSwitchDisplay, mTraceDisplay);
							if (mMidiInPortToDeviceIdxMap.find(inputDevicePort) != mMidiInPortToDeviceIdxMap.end())
							{
								IMidiInPtr midiIn{ mMidiInGenerator->CreateMidiIn(mMidiInPortToDeviceIdxMap[inputDevicePort]) };
								if (midiIn)
									mon->SubscribeToMidiIn(midiIn);
							}
							else if (mTraceDisplay)
							{
								std::strstream traceMsg;
								traceMsg << "Error loading toggleControlChange patch: midiInputDevice port not defined: " << inputDeviceName << '\n' << std::ends;
								mTraceDisplay->Trace(std::string(traceMsg.str()));
							}

							// store monitor in MidiControlEngine
							mEngine->AddControllerInputMonitor(inputDevicePort, mon);
						}

						mon->AddPatch(ctp, inputDeviceChannel, data1);
					}
				}
			}
		}
		else if (patchType == "persistentPedalOverride")
			newPatch = std::make_shared<PersistentPedalOverridePatch>(patchNumber, patchName, midiOut, cmds, cmds2);
		else if (patchType == "AxeToggle")
		{
			std::shared_ptr<AxeTogglePatch> axePatch;
			if (axeFxScene && mgr && mgr->GetModel() == Axe3)
				axePatch = std::make_shared<Axe3ScenePatch>(patchNumber, patchName, midiOut, cmds, cmds2, mgr, axeFxScene);
			else if (isAxeFx3EffectBlockBypassCommand)
				axePatch = std::make_shared<Axe3EffectBlockPatch>(patchNumber, patchName, midiOut, cmds, cmds2, mgr);
			else if (axeFxBlockId)
				axePatch = std::make_shared<Axe3EffectChannelPatch>(patchNumber, patchName, midiOut, cmds, cmds2, mgr);
			else
				axePatch = std::make_shared<AxeTogglePatch>(patchNumber, patchName, midiOut, cmds, cmds2, mgr);

			if (mgr)
			{
				if (!axeFxScene)
				{
					bool res;
					if (mgr->GetModel() == Axe3)
						res = mgr->SetSyncPatch(axePatch, axeFxBlockId, axeFxBlockChannel);
					else
						res = mgr->SetSyncPatch(axePatch, overrideCc);

					if (!res)
						axePatch->ClearAxeMgr();
				}
			}
			else if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: Axe-Fx device specified but no Axe-Fx input was created\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}

			newPatch = axePatch;
		}
		else if (patchType == "momentary")
			newPatch = std::make_shared<MomentaryPatch>(patchNumber, patchName, midiOut, cmds, cmds2);
		else if (patchType == "repeatingMomentary")
			newPatch = std::make_shared<RepeatingMomentaryPatch>(patchNumber, patchName, midiOut, cmds, cmds2);
		else if (patchType == "momentaryControlChange")
		{
			if (-1 == patchDefaultCh)
			{
				if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: patch " << patchName << " is of momentaryControlChange type but does not specify a device\n" << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}

			int data1 = -1;
			pElem->QueryIntAttribute("controller", &data1);
			if (-1 == data1)
			{
				if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: no controller specified for momentaryControlChange patch " << patchName << '\n' << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}

			if (data1 > 0xff)
			{
				if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: too large a controller value specified for momentaryControlChange patch " << patchName << '\n' << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}

			Bytes bytesA, bytesB;
			bytesA.push_back(0xb0 | patchDefaultCh);
			bytesA.push_back(data1);
			bytesA.push_back(127);

			bytesB.push_back(0xb0 | patchDefaultCh);
			bytesB.push_back(data1);
			bytesB.push_back(0);

			cmds.push_back(std::make_shared<MidiCommandString>(midiOut, bytesA));
			cmds2.push_back(std::make_shared<MidiCommandString>(midiOut, bytesB));
			newPatch = std::make_shared<MomentaryPatch>(patchNumber, patchName, midiOut, cmds, cmds2);
		}
		else if (patchType == "AxeMomentary")
		{
			auto axePatch = std::make_shared<AxeMomentaryPatch>(patchNumber, patchName, midiOut, cmds, cmds2, mgr, axeFxScene);
			if (mgr)
			{
				if (!axeFxScene)
				{
					bool res;
					if (mgr->GetModel() == Axe3)
						res = mgr->SetSyncPatch(axePatch, axeFxBlockId, axeFxBlockChannel);
					else
						res = mgr->SetSyncPatch(axePatch, overrideCc);

					if (!res)
						axePatch->ClearAxeMgr();
				}
			}
			else if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: Axe-Fx device specified but no Axe-Fx input was created\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}

			newPatch = axePatch;
		}
		else if (patchType == "sequence")
			newPatch = std::make_shared<SequencePatch>(patchNumber, patchName, midiOut, cmds);
		else if (patchType == "compositeToggle")
			newPatch = std::make_shared<CompositeTogglePatch>(patchNumber, patchName, midiOut, pairIntListA, pairIntListB);
		else if (patchType == "patchListSequence")
		{
			int immediateWrap = 0;
			pElem->QueryIntAttribute("gaplessRestart", &immediateWrap);
			int initialStep = 0;
			pElem->QueryIntAttribute("initialStep", &initialStep);
			newPatch = std::make_shared<PatchListSequencePatch>(patchNumber, patchName, intList, immediateWrap, initialStep);
		}
		else if (patchType == "AxeFxTapTempo")
		{
			if (cmds.empty() && cmds2.empty())
			{
				if (mgr && mgr->GetModel() == Axe3)
				{
					if (mAxeFx3Manager)
						cmds.push_back(std::make_shared<MidiCommandString>(midiOut, mAxeFx3Manager->GetCommandString("taptempo", true)));
				}
				else if (-1 != patchDefaultCh && mAxeFxManager)
				{
					const int cc = mAxeFxManager->GetDefaultAxeCc("Tap Tempo");
					if (cc)
					{
						Bytes bytes;
						bytes.push_back(0xb0 | patchDefaultCh);
						bytes.push_back(cc);
						bytes.push_back(127);
						cmds.push_back(std::make_shared<MidiCommandString>(midiOut, bytes));
					}
				}
			}

			newPatch = std::make_shared<MomentaryPatch>(patchNumber, patchName, midiOut, cmds, cmds2);
			if (mgr)
				mgr->SetTempoPatch(newPatch);
			else if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: AxeFxTapTempo specified but no Axe-Fx input was created\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
		}
		else
			patchTypeErr = true;

		if (patchTypeErr && mTraceDisplay)
		{
			std::strstream traceMsg;
			traceMsg << "Error loading config file: invalid patch type specified for patch " << patchName << '\n' << std::ends;
			mTraceDisplay->Trace(std::string(traceMsg.str()));
		}

		if (!newPatch)
			continue;

		bool isAxeLooperPatch = false;
		if (mgr)
		{
			if (axeFxScene)
				mgr->SetScenePatch(axeFxScene, newPatch);
			else
				isAxeLooperPatch = mgr->SetLooperPatch(newPatch);
		}

		// setup patch led color
		{
			unsigned int ledActiveColor = -1;
			unsigned int ledInactiveColor = -1;
			LoadElementColorAttributes(pElem, ledActiveColor, ledInactiveColor);

			if (axeFxScene)
				patchType = "axescene";
			else if (isAxeLooperPatch)
				patchType = "axelooper";
			else if (axeFxBlockId)
			{
				patchType = "axeblock";
				const char * blockChannelStrs[6]
				{ 
					"axeblocka",
					"axeblockb",
					"axeblockc",
					"axeblockd",
					"axeblocke",
					"axeblockf"
				};
				
				if (-1 == ledActiveColor && axeFxBlockChannel < 6)
					ledActiveColor = LookUpColor("*", blockChannelStrs[axeFxBlockChannel], 1, -1);

				if (-1 == ledInactiveColor && axeFxBlockChannel < 6)
					ledInactiveColor = LookUpColor("*", blockChannelStrs[axeFxBlockChannel], 0, -1);
			}
			else if (-1 != patchName.find("x/y") || -1 != patchName.find("X/Y"))
			{
				if (-1 == ledActiveColor)
					ledActiveColor = LookUpColor("*", "axexy", 1, -1);

				if (-1 == ledInactiveColor)
					ledInactiveColor = LookUpColor("*", "axexy", 0, -1);
			}
			else if (-1 != patchName.find("a/b") || -1 != patchName.find("A/B"))
			{
				if (-1 == ledActiveColor)
					ledActiveColor = LookUpColor("*", "axeCh", 1, -1);

				if (-1 == ledInactiveColor)
					ledInactiveColor = LookUpColor("*", "axeCh", 0, -1);
			}

			if (-1 == ledActiveColor)
				ledActiveColor = LookUpColor(device, patchType, 1, kFirstColorPreset);

			if (-1 == ledInactiveColor)
			{
				ledInactiveColor = LookUpColor(device, patchType, 0, -1);
				if (-1 == ledInactiveColor)
				{
					ledInactiveColor = kFirstColorPreset;
					if (axeFxScene || axeFxBlockId || isAxeLooperPatch || 
						patchType == "AxeToggle" || patchType == "AxeMomentary")
					{
						if (ledActiveColor == kFirstColorPreset)
							ledInactiveColor = 0; // just set dim to off
						else if (ledActiveColor & kPresetColorMarkerBit)
							; // first preset slot
						else
						{
							// calculate inactive color based on active color
							constexpr float onOpacity = 0.25f;
							BYTE R = (BYTE)((float)GetRValue(ledInactiveColor) * onOpacity);
							BYTE G = (BYTE)((float)GetGValue(ledInactiveColor) * onOpacity);
							BYTE B = (BYTE)((float)GetBValue(ledInactiveColor) * onOpacity);
							ledInactiveColor = RGB(R, G, B);
						}
					}
				}
			}

			newPatch->SetLedColors(ledActiveColor, ledInactiveColor);
		}

		mEngine->AddPatch(newPatch);

		ExpressionPedals & pedals = newPatch->GetPedals();
		for (childElem = hRoot.FirstChildElement().Element(); 
			 childElem; 
			 childElem = childElem->NextSiblingElement())
		{
			const std::string patchElement = childElem->ValueStr();
			if (patchElement == "localExpr")
			{
				// <localExpr inputNumber="1" assignmentNumber="1" channel="" controller="" min="" max="" invert="0" enable="" port="2" />
				LoadExpressionPedalSettings(childElem, pedals, patchDefaultCh + 1);
			}
			else if (patchElement == "globalExpr")
			{
				// <globalExpr inputNumber="" enable="" />
				int exprInputNumber = -1;
				int enable = 1;

				childElem->QueryIntAttribute("inputNumber", &exprInputNumber);
				childElem->QueryIntAttribute("enable", &enable);

				if (exprInputNumber > 0 &&
					exprInputNumber <= ExpressionPedals::PedalCount)
				{
					ExpressionPedals & pedals2 = newPatch->GetPedals();
					pedals2.EnableGlobal(exprInputNumber - 1, !!enable);
				}
			}
		}
	}
}

void
EngineLoader::LoadElementColorAttributes(TiXmlElement * pElem, unsigned int &ledActiveColor, unsigned int &ledInactiveColor)
{
	int activeColorPreset = -1;
	int inactiveColorPreset = -1;
	std::string activeColorStr, inactiveColorStr;

	if (pElem->Attribute("ledColor"))
		activeColorStr = pElem->Attribute("ledColor");
	if (pElem->Attribute("ledInactiveColor"))
		inactiveColorStr = pElem->Attribute("ledInactiveColor");

	pElem->QueryIntAttribute("ledColorPreset", &activeColorPreset);
	pElem->QueryIntAttribute("ledInactiveColorPreset", &inactiveColorPreset);

	if ((!activeColorStr.empty() && -1 != activeColorPreset) ||
		(!inactiveColorStr.empty() && -1 != inactiveColorPreset))
	{
		if (mTraceDisplay)
		{
			std::strstream traceMsg;
			traceMsg << "Error loading config file: ledColor attribute incompatible with ledColorPreset attribute\n" << std::ends;
			mTraceDisplay->Trace(std::string(traceMsg.str()));
		}
		return;
	}

	if (!activeColorStr.empty())
	{
		if (activeColorStr.length() != 6)
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: color attribute should be specified as 6 hex chars\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			return;
		}
		ledActiveColor = ::StrToRgb(activeColorStr);
	}

	if (!inactiveColorStr.empty())
	{
		if (inactiveColorStr.length() != 6)
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: color attribute should be specified as 6 hex chars\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			return;
		}
		ledInactiveColor = ::StrToRgb(inactiveColorStr);
	}

	if (-1 != activeColorPreset)
	{
		if (activeColorPreset < 1 || activeColorPreset > 32)
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: invalid ledColorPreset on patch -- valid range of values 1-32\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			return;
		}

		--activeColorPreset;
		ledActiveColor = kPresetColorMarkerBit | activeColorPreset;
	}

	if (-1 != inactiveColorPreset)
	{
		if (inactiveColorPreset < 1 || inactiveColorPreset > 32)
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: invalid ledInactiveColorPreset on patch -- valid range of values 1-32\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			return;
		}

		--inactiveColorPreset;
		ledInactiveColor = kPresetColorMarkerBit | inactiveColorPreset;
	}
}

IAxeFxPtr
EngineLoader::GetAxeMgr(TiXmlElement * pElem)
{
	if (mAxeFx3Manager || mAxeFxManager)
	{
		if (pElem && pElem->Attribute("device"))
		{
			const std::string devStr(pElem->Attribute("device"));
			if (!devStr.empty())
			{
				if (devStr == mAxe3DeviceName)
					return mAxeFx3Manager;
				if (devStr == mAxeDeviceName)
					return mAxeFxManager;
			}
		}
	}
	return nullptr;
}

void
EngineLoader::LoadBanks(TiXmlElement * pElem)
{
	constexpr int kFirstAutoGenBankNumber = 1;
	int autoGeneratedBankNumber = kFirstAutoGenBankNumber;
	int	nonDefaultBankCount = 0;
	bool defaultBankFound = false;
	int autoGenPatchNumber = (int)ReservedPatchNumbers::kAutoGenPatchNumberStart;

	for ( ; pElem; pElem = pElem->NextSiblingElement())
	{
		if (pElem->ValueStr() != "bank")
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: unrecognized Banks item\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		std::string bankName;
		if (pElem->Attribute("name"))
			bankName = pElem->Attribute("name");
		if (bankName.empty())
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: bank definition missing bank name\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		int bankNumber = -1;
		pElem->QueryIntAttribute("number", &bankNumber);
		if (-1 == bankNumber)
		{
			if (nonDefaultBankCount && autoGeneratedBankNumber == kFirstAutoGenBankNumber)
			{
				if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: bank definition missing bank number\n" << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}

			if (!defaultBankFound && 
				(bankName == "default" || bankName == "defaults" || bankName == "Default" || bankName == "Defaults"))
			{
				defaultBankFound = true;
				bankNumber = 0;
			}
			else
				bankNumber = autoGeneratedBankNumber++;
		}
		else if (autoGeneratedBankNumber != kFirstAutoGenBankNumber)
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: mixed use of bank numbers -- all bank definitions should have number or no bank definitions should have number\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		// check for name collision
		if (autoGeneratedBankNumber != kFirstAutoGenBankNumber  && -1 != mEngine->GetBankNumber(bankName))
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: bank name already used -- " << bankName << '\n' << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		PatchBankPtr bank = mEngine->AddBank(bankNumber, bankName); 
		if (bank && bankNumber)
			++nonDefaultBankCount;

		TiXmlHandle hRoot(nullptr);
		hRoot = TiXmlHandle(pElem);

		for (TiXmlElement * childElem = hRoot.FirstChildElement().Element(); 
			 childElem; 
			 childElem = childElem->NextSiblingElement())
		{
			if (childElem->ValueStr() == "switch" ||
				childElem->ValueStr() == "PatchMap") // PatchMap was the old name for switch
			{
				std::string tmp;
				int switchNumber = -1;
				childElem->QueryIntAttribute("number", &switchNumber);
				if (switchNumber <= 0)
					childElem->QueryIntAttribute("switch", &switchNumber); // switch was the old name for number (when used with PatchMap)
				if (switchNumber <= 0)
				{
					if (mTraceDisplay)
					{
						std::strstream traceMsg;
						traceMsg << "Error loading config file: invalid switch number for bank " << bankName << '\n' << std::ends;
						mTraceDisplay->Trace(std::string(traceMsg.str()));
					}
					continue;
				}

				std::string nameOverride;
				if (childElem->Attribute("label"))
					nameOverride = childElem->Attribute("label");
				else if (childElem->Attribute("name"))
					nameOverride = childElem->Attribute("name");

				int patchNumber = -1;
				childElem->QueryIntAttribute("patch", &patchNumber);
				// patch may be empty; not an error; retain value of -1

				if (-1 == patchNumber)
				{
					// support for switch assignment by patch name instead of patchNumber
					std::string patchName;
					// QueryValueAttribute does not work with string when there are 
					// spaces (truncated at whitespace); use Attribute instead
					if (childElem->Attribute("patchName"))
						patchName = childElem->Attribute("patchName");
					if (!patchName.empty())
					{
						patchNumber = GetPatchNumber(patchName);
						if (-1 == patchNumber)
						{
							if (mTraceDisplay)
							{
								std::strstream traceMsg;
								traceMsg << "Error loading config file: switch " << switchNumber << " in bank " << bankNumber << " failed to locate patch name " << patchName << "\n" << std::ends;
								mTraceDisplay->Trace(std::string(traceMsg.str()));
							}
							continue;
						}
					}
				}

				if (childElem->Attribute("command"))
				{
					// handle engineMetaPatch functionality without an explicit engineMetaPatch ("command" instead of "action")
					std::string cmdName;
					childElem->QueryValueAttribute("command", &cmdName);
					if (cmdName.empty())
					{
						if (mTraceDisplay)
						{
							std::strstream traceMsg;
							traceMsg << "Error loading config file: switch " << switchNumber << " in bank " << bankNumber << " missing action\n" << std::ends;
							mTraceDisplay->Trace(std::string(traceMsg.str()));
						}
						continue;
					}

					std::string gendPatchName(nameOverride);
					bool isAutoGendPatchNumber = false;
					if (-1 == patchNumber)
					{
						patchNumber = autoGenPatchNumber--;
						isAutoGendPatchNumber = true;
					}

					PatchPtr cmdPatch = nullptr;

					if (cmdName == "ResetBankPatches")
					{
						if (!isAutoGendPatchNumber)
						{
							gendPatchName = "Reset bank patches";
							cmdPatch = std::make_shared<MetaPatch_ResetBankPatches>(mEngine.get(), patchNumber, gendPatchName);
						}
						else
							patchNumber = ReservedPatchNumbers::kResetBankPatches;
					}
					else if (cmdName == "SyncAxeFx")
					{
						if (!isAutoGendPatchNumber)
						{
							gendPatchName = "Sync Axe-FX";
							auto metPatch = std::make_shared<MetaPatch_SyncAxeFx>(patchNumber, gendPatchName);
							std::vector<IAxeFxPtr> mgrs;
							if (mAxeFx3Manager)
								mgrs.push_back(mAxeFx3Manager);
							if (mAxeFxManager)
								mgrs.push_back(mAxeFxManager);
							metPatch->AddAxeManagers(mgrs);
							cmdPatch = metPatch;
						}
						else
							patchNumber = ReservedPatchNumbers::kSyncAxeFx;
					}
					else if (cmdName == "LoadNextBank")
					{
						if (!isAutoGendPatchNumber)
						{
							gendPatchName = "[next]";
							cmdPatch = std::make_shared<MetaPatch_LoadNextBank>(mEngine.get(), patchNumber, gendPatchName);
						}
						else
							patchNumber = ReservedPatchNumbers::kLoadNextBank;
					}
					else if (cmdName == "LoadPreviousBank")
					{
						if (!isAutoGendPatchNumber)
						{
							gendPatchName = "[previous]";
							cmdPatch = std::make_shared<MetaPatch_LoadPreviousBank>(mEngine.get(), patchNumber, gendPatchName);
						}
						else
							patchNumber = ReservedPatchNumbers::kLoadPrevBank;
					}
					else if (cmdName == "NavNextBank")
					{
						if (!isAutoGendPatchNumber)
						{
							gendPatchName = "Nav next";
							cmdPatch = std::make_shared<MetaPatch_BankNavNext>(mEngine.get(), patchNumber, gendPatchName);
						}
						else
							patchNumber = ReservedPatchNumbers::kBankNavNext;
					}
					else if (cmdName == "NavPreviousBank")
					{
						if (!isAutoGendPatchNumber)
						{
							gendPatchName = "Nav previous";
							cmdPatch = std::make_shared<MetaPatch_BankNavPrevious>(mEngine.get(), patchNumber, gendPatchName);
						}
						else
							patchNumber = ReservedPatchNumbers::kBankNavPrev;
					}
					else if (cmdName == "BankHistoryBackward")
					{
						if (!isAutoGendPatchNumber)
						{
							gendPatchName = "[back]";
							cmdPatch = std::make_shared<MetaPatch_BankHistoryBackward>(mEngine.get(), patchNumber, gendPatchName);
						}
						else
							patchNumber = ReservedPatchNumbers::kBankHistoryBackward;
					}
					else if (cmdName == "BankHistoryForward")
					{
						if (!isAutoGendPatchNumber)
						{
							gendPatchName = "[forward]";
							cmdPatch = std::make_shared<MetaPatch_BankHistoryForward>(mEngine.get(), patchNumber, gendPatchName);
						}
						else
							patchNumber = ReservedPatchNumbers::kBankHistoryForward;
					}
					else if (cmdName == "BankHistoryRecall")
					{
						if (!isAutoGendPatchNumber)
						{
							gendPatchName = "[recall]";
							cmdPatch = std::make_shared<MetaPatch_BankHistoryRecall>(mEngine.get(), patchNumber, gendPatchName);
						}
						else
							patchNumber = ReservedPatchNumbers::kBankHistoryRecall;
					}
					else if (cmdName == "LoadBank")
					{
						int bankNum = -1;
						childElem->QueryIntAttribute("bankNumber", &bankNum);
						if (-1 != bankNum)
						{
							if (nameOverride.empty())
								nameOverride = "meta load bank";
							cmdPatch = std::make_shared<MetaPatch_LoadBank>(mEngine.get(), patchNumber, nameOverride, bankNum);
							nameOverride.clear();
						}
						else
						{
							// support for selecting target by bank name instead of bank number
							std::string targetBankName;
							// QueryValueAttribute does not work with string when there are 
							// spaces (truncated at whitespace); use Attribute instead
							if (childElem->Attribute("bankName"))
								targetBankName = childElem->Attribute("bankName");
							if (!targetBankName.empty())
							{
								if (nameOverride.empty())
									nameOverride = "meta load bank";
								cmdPatch = std::make_shared<MetaPatch_LoadBank>(mEngine.get(), patchNumber, nameOverride, targetBankName);
								nameOverride.clear();
							}
							else
							{
								if (mTraceDisplay)
								{
									std::strstream traceMsg;
									traceMsg << "Error loading config file: switch " << nameOverride << " missing LoadBank target\n" << std::ends;
									mTraceDisplay->Trace(std::string(traceMsg.str()));
								}
								continue;
							}
						}
					}
					else if (cmdName == "ResetPatch")
					{
						int patchNumToReset = -1;
						childElem->QueryIntAttribute("patchToReset", &patchNumToReset);
						if (-1 == patchNumToReset)
						{
							std::string patchName;
							// QueryValueAttribute does not work with string when there are 
							// spaces (truncated at whitespace); use Attribute instead
							if (childElem->Attribute("patchNameToReset"))
								patchName = childElem->Attribute("patchNameToReset");
							if (!patchName.empty())
								patchNumToReset = GetPatchNumber(patchName);
						}

						if (-1 != patchNumToReset)
						{
							if (nameOverride.empty())
								nameOverride = "Reset Patch";
							cmdPatch = std::make_shared<MetaPatch_ResetPatch>(mEngine.get(), patchNumber, nameOverride, patchNumToReset);
							nameOverride.clear();
						}
						else 
						{
							if (mTraceDisplay)
							{
								std::strstream traceMsg;
								traceMsg << "Error loading config file: switch " << nameOverride << " missing ResetPatch switch target\n" << std::ends;
								mTraceDisplay->Trace(std::string(traceMsg.str()));
							}
							continue;
						}
					}
					else if (cmdName == "ResetExclusiveGroup")
					{
						int activeSwitch = -1;
						childElem->QueryIntAttribute("activeSwitch", &activeSwitch);
						if (-1 != activeSwitch)
						{
							if (nameOverride.empty())
								nameOverride = "Reset exclusive group";
							cmdPatch = std::make_shared<MetaPatch_ResetExclusiveGroup>(mEngine.get(), patchNumber, nameOverride, activeSwitch);
							nameOverride.clear();
						}
						else 
						{
							if (mTraceDisplay)
							{
								std::strstream traceMsg;
								traceMsg << "Error loading config file: switch " << nameOverride << " missing ResetExclusiveGroup switch target\n" << std::ends;
								mTraceDisplay->Trace(std::string(traceMsg.str()));
							}
							continue;
						}
					}
					else if (cmdName == "AxeFx3IncrementPreset")
					{
						if (!isAutoGendPatchNumber)
						{
							gendPatchName = "AxeFx: Next Preset";
							auto metPatch = std::make_shared<MetaPatch_AxeFxNextPreset>(patchNumber, gendPatchName);
							std::vector<IAxeFxPtr> mgrs;
							if (mAxeFx3Manager)
								mgrs.push_back(mAxeFx3Manager);
							metPatch->AddAxeManagers(mgrs);
							cmdPatch = metPatch;
						}
						else
							patchNumber = ReservedPatchNumbers::kAxeFx3NextPreset;
					}
					else if (cmdName == "AxeFx3DecrementPreset")
					{
						if (!isAutoGendPatchNumber)
						{
							gendPatchName = "AxeFx: Prev Preset";
							auto metPatch = std::make_shared<MetaPatch_AxeFxPrevPreset>(patchNumber, gendPatchName);
							std::vector<IAxeFxPtr> mgrs;
							if (mAxeFx3Manager)
								mgrs.push_back(mAxeFx3Manager);
							metPatch->AddAxeManagers(mgrs);
							cmdPatch = metPatch;
						}
						else
							patchNumber = ReservedPatchNumbers::kAxeFx3PrevPreset;
					}
					else if (cmdName == "AxeFx3IncrementScene")
					{
						if (!isAutoGendPatchNumber)
						{
							gendPatchName = "AxeFx: Next Scene";
							auto metPatch = std::make_shared<MetaPatch_AxeFxNextScene>(patchNumber, gendPatchName);
							std::vector<IAxeFxPtr> mgrs;
							if (mAxeFx3Manager)
								mgrs.push_back(mAxeFx3Manager);
							metPatch->AddAxeManagers(mgrs);
							cmdPatch = metPatch;
						}
						else
							patchNumber = ReservedPatchNumbers::kAxeFx3NextScene;
					}
					else if (cmdName == "AxeFx3DecrementScene")
					{
						if (!isAutoGendPatchNumber)
						{
							gendPatchName = "AxeFx: Prev Scene";
							auto metPatch = std::make_shared<MetaPatch_AxeFxPrevScene>(patchNumber, gendPatchName);
							std::vector<IAxeFxPtr> mgrs;
							if (mAxeFx3Manager)
								mgrs.push_back(mAxeFx3Manager);
							metPatch->AddAxeManagers(mgrs);
							cmdPatch = metPatch;
						}
						else
							patchNumber = ReservedPatchNumbers::kAxeFx3PrevScene;
					}
					else if (cmdName == "EdpShowLocalState")
					{
						if (-1 == mEdpPort)
							continue;

						patchNumber = autoGenPatchNumber--;
						gendPatchName = "EDP Local State";
						auto midiOut = mMidiOutGenerator->GetMidiOut(mMidiOutPortToDeviceIdxMap[mEdpPort]);
						auto metPatch = std::make_shared<NormalPatch>(patchNumber, gendPatchName, midiOut, PatchCommands{ std::make_shared<MidiCommandString>(midiOut, mEdpManager->GetLocalStateRequest()) }, PatchCommands{});
						cmdPatch = metPatch;
					}
					else if (cmdName == "EdpShowGlobalState")
					{
						if (-1 == mEdpPort)
							continue;

						patchNumber = autoGenPatchNumber--;
						gendPatchName = "EDP Global State";
						auto midiOut = mMidiOutGenerator->GetMidiOut(mMidiOutPortToDeviceIdxMap[mEdpPort]);
						auto metPatch = std::make_shared<NormalPatch>(patchNumber, gendPatchName, midiOut, PatchCommands{ std::make_shared<MidiCommandString>(midiOut, mEdpManager->GetGlobalStateRequest()) }, PatchCommands{});
						cmdPatch = metPatch;
					}
					else
					{
						if (mTraceDisplay)
						{
							std::strstream traceMsg;
							traceMsg << "Error loading config file: switch " << nameOverride << " unknown command " << cmdName << '\n' << std::ends;
							mTraceDisplay->Trace(std::string(traceMsg.str()));
						}
						continue;
					}

					if (cmdPatch)
					{
						unsigned int ledActiveColor = -1;
						unsigned int ledInactiveColor = -1;
						LoadElementColorAttributes(childElem, ledActiveColor, ledInactiveColor);
						ledInactiveColor = 0;
						if (-1 == ledActiveColor)
							ledActiveColor = LookUpColor("mtroll", cmdName, 1);

						cmdPatch->SetLedColors(ledActiveColor, ledInactiveColor);
						mEngine->AddPatch(cmdPatch);
					}
				}

				tmp.clear();
				childElem->QueryValueAttribute("secondFunction", &tmp);
				const PatchBank::SwitchFunctionAssignment swFunc = (tmp.length()) ? PatchBank::ssSecondary : PatchBank::ssPrimary;
				const PatchBank::SecondFunctionOperation sfoOp = ::GetSecondFuncOp(tmp);

				tmp.clear();
				childElem->QueryValueAttribute("loadState", &tmp);
				const PatchBank::PatchState loadState = ::GetLoadState(tmp);
				tmp.clear();
				childElem->QueryValueAttribute("unloadState", &tmp);
				const PatchBank::PatchState unloadState = ::GetLoadState(tmp);
				tmp.clear();
				childElem->QueryValueAttribute("override", &tmp);
				const PatchBank::PatchState stateOverride = ::GetLoadState(tmp);
				tmp.clear();
				childElem->QueryValueAttribute("sync", &tmp);
				const PatchBank::PatchSyncState syncState = ::GetSyncState(tmp);
				tmp.clear();
// 				if (syncState != PatchBank::syncIgnore && 
// 					stateOverride != PatchBank::stIgnore && 
// 					mTraceDisplay)
// 				{
// 					std::strstream traceMsg;
// 					traceMsg << "Warning load of config file: bank " << bankName << " has a switch with both override and sync attributes (might not work)\n" << std::ends;
// 					mTraceDisplay->Trace(std::string(traceMsg.str()));
// 				}

				bank->AddSwitchAssignment(switchNumber - 1, patchNumber, nameOverride, swFunc, sfoOp, loadState, unloadState, stateOverride, syncState);
			}
			else if (childElem->ValueStr() == "ExclusiveSwitchGroup")
			{
				std::string switchesStr;
				if (childElem->GetText())
					switchesStr = childElem->GetText();
				PatchBank::GroupSwitchesPtr switches = std::make_shared<PatchBank::GroupSwitches>();

				// split switchesStr - space token
				while (!switchesStr.empty())
				{
					const int curSwitch = ::atoi(switchesStr.c_str());
					if (curSwitch)
						switches->insert(curSwitch - 1);

					const int spacePos = switchesStr.find(' ');
					if (std::string::npos == spacePos)
						break;

					switchesStr = switchesStr.substr(spacePos + 1);
				}

				bank->CreateExclusiveGroup(switches);
			} 
			else if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: unrecognized element in bank " << bankName << '\n' << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
		}
	}
}

unsigned int
EngineLoader::LookUpColor(std::string device, std::string patchType, int activeState, unsigned int defaultColor /*= kFirstColorPreset*/)
{
	_ASSERTE(!device.empty() || !patchType.empty());
	_ASSERTE(device != "*" || patchType != "*");

	std::transform(device.begin(), device.end(), device.begin(),
		[](unsigned char c) { return std::tolower(c); }
	);

	std::transform(patchType.begin(), patchType.end(), patchType.begin(),
		[](unsigned char c) { return std::tolower(c); }
	);

	// lookup device + type
	if (device != "*" && patchType != "*")
	{
		auto it = mLedDefaultColors.find({ device, patchType, activeState });
		if (it != mLedDefaultColors.end())
			return it->second;
	}

	// if type starts with Axe, lookup device=* + type
	if (0 == patchType.find("axe"))
	{
		auto it = mLedDefaultColors.find({ "*", patchType, activeState });
		if (it != mLedDefaultColors.end())
			return it->second;
	}

	// lookup device + type=*
	if (device != "*")
	{
		auto it = mLedDefaultColors.find({ device, "*", activeState });
		if (it != mLedDefaultColors.end())
			return it->second;
	}

	// lookup device=* + type
	if (patchType != "*")
	{
		auto it = mLedDefaultColors.find({ "*", patchType, activeState });
		if (it != mLedDefaultColors.end())
			return it->second;
	}

	// default to first preset slot
	return defaultColor;
}

void
EngineLoader::LoadExpressionPedalSettings(TiXmlElement * childElem, 
										  ExpressionPedals &pedals,
										  int defaultChannel)
{
	int exprInputNumber = -1;
	int assignmentIndex = -1;
	int channel = -1;
	int controller = -1;
	int maxVal = 127;
	int minVal = 0;
	int enable = 1;
	int invert = 0;
	int isDoubleByte = 0;
	int bottomTogglePatchNumber = -1;
	int topTogglePatchNumber = -1;
	int midiOutPortNumber = -1;
	ExpressionControl::SweepCurve curve = ExpressionControl::scLinear;

	childElem->QueryIntAttribute("inputNumber", &exprInputNumber);
	childElem->QueryIntAttribute("assignmentNumber", &assignmentIndex);
	childElem->QueryIntAttribute("channel", &channel);
	{
		std::string tmp;
		if (childElem->Attribute("device"))
			tmp = childElem->Attribute("device");
		if (!tmp.empty())
		{
			midiOutPortNumber = mDevicePorts[tmp]; // see if a port mapping was set up for the device

			if (-1 == channel)
			{
				tmp = mDeviceChannels[tmp];
				if (!tmp.empty())
					channel = ::atoi(tmp.c_str());
			}
		}
	}

	if (-1 == channel)
		channel = defaultChannel;

	childElem->QueryIntAttribute("controller", &controller);
	childElem->QueryIntAttribute("max", &maxVal);
	childElem->QueryIntAttribute("min", &minVal);
	childElem->QueryIntAttribute("invert", &invert);
	childElem->QueryIntAttribute("enable", &enable);
	childElem->QueryIntAttribute("doubleByte", &isDoubleByte);

	childElem->QueryIntAttribute("bottomTogglePatchNumber", &bottomTogglePatchNumber);
	if (-1 == bottomTogglePatchNumber)
	{
		std::string patchName;
		// QueryValueAttribute does not work with string when there are 
		// spaces (truncated at whitespace); use Attribute instead
		if (childElem->Attribute("bottomTogglePatchName"))
			patchName = childElem->Attribute("bottomTogglePatchName");
		if (!patchName.empty())
			bottomTogglePatchNumber = GetPatchNumber(patchName);
	}

	childElem->QueryIntAttribute("topTogglePatchNumber", &topTogglePatchNumber);
	if (-1 == topTogglePatchNumber)
	{
		std::string patchName;
		// QueryValueAttribute does not work with string when there are 
		// spaces (truncated at whitespace); use Attribute instead
		if (childElem->Attribute("topTogglePatchName"))
			patchName = childElem->Attribute("topTogglePatchName");
		if (!patchName.empty())
			topTogglePatchNumber = GetPatchNumber(patchName);
	}

	bool curveOk = true;
	std::string tmp;
	childElem->QueryValueAttribute("sweepCurve", &tmp);
	if (tmp.length())
	{
		if (!::_stricmp(tmp.c_str(), "linear"))
			curve = ExpressionControl::scLinear;
		else if (!::_stricmp(tmp.c_str(), "AudioLog"))
			curve = ExpressionControl::scAudioLog;
		else if (!::_stricmp(tmp.c_str(), "ShallowLog"))
			curve = ExpressionControl::scShallowLog;
		else if (!::_stricmp(tmp.c_str(), "ReverseShallowLog"))
			curve = ExpressionControl::scReverseShallowLog;
		else if (!::_stricmp(tmp.c_str(), "ReverseAudioLog"))
			curve = ExpressionControl::scReverseAudioLog;
		else if (!::_stricmp(tmp.c_str(), "ReversePseudoAudioLog"))
			curve = ExpressionControl::scReversePseudoAudioLog;
		else if (!::_stricmp(tmp.c_str(), "PseudoAudioLog"))
			curve = ExpressionControl::scPseudoAudioLog;
		else
			curveOk = false;
	}

	if (enable &&
		exprInputNumber > 0 &&
		exprInputNumber <= ExpressionPedals::PedalCount &&
		assignmentIndex > 0 &&
		assignmentIndex < 3 &&
		channel > 0 &&
		channel < 17 &&
		controller >= 0 &&
		controller < 128 &&
		curveOk)
	{
		ExpressionControl::InitParams initParams;
		initParams.mInvert = !!invert;
		initParams.mChannel = channel - 1;
		initParams.mControlNumber = controller;
		initParams.mMinVal = minVal;
		initParams.mMaxVal = maxVal;
		initParams.mDoubleByte = !!isDoubleByte;
		initParams.mBottomTogglePatchNumber = bottomTogglePatchNumber;
		initParams.mTopTogglePatchNumber = topTogglePatchNumber;
		initParams.mCurve = curve;
		pedals.Init(exprInputNumber - 1, assignmentIndex - 1, initParams);

		childElem->QueryIntAttribute("port", &midiOutPortNumber);
		if (-1 != midiOutPortNumber)
		{
			IMidiOutPtr midiOut = mMidiOutGenerator->GetMidiOut(mMidiOutPortToDeviceIdxMap[midiOutPortNumber]);
			if (midiOut)
				pedals.InitMidiOut(midiOut, exprInputNumber - 1, assignmentIndex - 1);
		}
	}
	else if (enable)
	{
		if (mTraceDisplay)
		{
			std::strstream traceMsg;
			traceMsg << "Error loading config file: expression pedal settings error\n" << std::ends;
			mTraceDisplay->Trace(std::string(traceMsg.str()));
		}
	}

	if (enable)
	{
		_ASSERTE(exprInputNumber > 0);
		if (exprInputNumber > 0 && mAdcEnables[exprInputNumber - 1] == adc_default)
			mAdcEnables[exprInputNumber - 1] = adc_used;
	}
}

void
EngineLoader::InitMonome(IMonome40h * monome, 
						 const bool adcOverrides[ExpressionPedals::PedalCount],
						 bool userAdcSettings[ExpressionPedals::PedalCount])
{
	for (int idx = 0; idx < ExpressionPedals::PedalCount; ++idx)
	{
		userAdcSettings[idx] = (mAdcEnables[idx] == adc_used || mAdcEnables[idx] == adc_forceOn);
		const bool enable = !adcOverrides[idx] && userAdcSettings[idx];
		monome->EnableAdc(idx, enable);
		if (mTraceDisplay)
		{
			std::strstream traceMsg;
			traceMsg << "  ADC port " << idx << (enable ? " enabled" : " disabled") << '\n' << std::ends;
			mTraceDisplay->Trace(std::string(traceMsg.str()));
		}
	}
}

void
EngineLoader::LoadDeviceChannelMap(TiXmlElement * pElem)
{
/*
 * optional device / channel and device / port map
	<DeviceChannelMap>
		<device channel="1" port="1">EDP</>
		<device channel="2" port="2">H8000</>
	</DeviceChannelMap>
 */
	std::string dev;
	for ( ; pElem; pElem = pElem->NextSiblingElement())
	{
		if (pElem->ValueStr() != "device" && pElem->ValueStr() != "Device")
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: unrecognized element in DeviceChannelMap\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		if (!pElem->GetText())
			continue;
		dev = pElem->GetText();

		std::string ch;
		pElem->QueryValueAttribute("channel", &ch);
		if (ch.empty())
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: missing channel name in DeviceChannelMap\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		mDeviceChannels[dev] = ch;

		// optional midi port device mapping
		int port = -1;
		pElem->QueryIntAttribute("port", &port);
		mDevicePorts[dev] = port;

		if (dev == "EDP" || 
			dev == "EDP+" ||
			dev == "Echoplex Digital Pro" ||
			dev == "Echoplex Digital Pro Plus" ||
			dev == "Echoplex Digital Pro+")
		{
			if (!mEdpManager)
			{
				mEdpManager = std::make_shared<EdpManager>(mMainDisplay, mSwitchDisplay, mTraceDisplay);
				mEdpPort = -1 == port ? 1 : port;
			}
			else if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: multiple Echoplex devices\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
		}
		else if (dev == "AxeFx3" ||
			dev == "Axe-Fx3" ||
			dev == "Axe-Fx 3" ||
			dev == "AxeFx III" ||
			dev == "Axe-Fx III"
			)
		{
			if (!mAxeFx3Manager)
			{
				AxeFxModel axeModel(Axe3);
				const int axeCh = ::atoi(ch.c_str()) - 1;
				mAxeFx3Manager = std::make_shared<AxeFx3Manager>(mMainDisplay, mSwitchDisplay, mTraceDisplay, mApp->ApplicationDirectory(), axeCh, axeModel);
				mAxe3SyncPort = -1 == port ? 1 : port;
				mAxe3DeviceName = dev;
			}
			else if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: multiple axe-fx 3 devices\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
		}
		else if (dev == "AxeFx" ||
			dev == "Axe-Fx" || 
			dev == "AxeFx Ultra" ||
			dev == "Axe-Fx Ultra" ||
			dev == "AxeFx Std" ||
			dev == "AxeFx Standard" ||
			dev == "Axe-Fx Standard" ||
			dev == "AxeFx II" ||
			dev == "Axe-Fx II" ||
			dev == "AxeFx 2" ||
			dev == "Axe-Fx 2" ||
			dev == "AxeFx II XL" ||
			dev == "Axe-Fx II XL" ||
			dev == "AxeFx 2 XL" ||
			dev == "Axe-Fx 2 XL"
			)
		{
			AxeFxModel axeModel(AxeStd);
			int tmp = -1;
			pElem->QueryIntAttribute("model", &tmp);
			if (2 == tmp)
				axeModel = Axe2;
			else if (3 == tmp)
				axeModel = Axe3;
			else if (6 == tmp)
				axeModel = Axe2XL;
			else if (7 == tmp)
				axeModel = Axe2XLPlus;
			else
			{
				int pos = dev.find("XL");
				if (std::string::npos != pos)
				{
					axeModel = Axe2XL;
					pos = dev.find("+");
					if (std::string::npos != pos)
						axeModel = Axe2XLPlus;
					else
					{
						pos = dev.find("Plus");
						if (std::string::npos != pos)
							axeModel = Axe2XLPlus;
					}
				}
				else
				{
					pos = dev.find("2");
					if (std::string::npos != pos)
						axeModel = Axe2;
					else
					{
						pos = dev.find("II");
						if (std::string::npos != pos)
							axeModel = Axe2;
					}
				}
			}

			const int axeCh = ::atoi(ch.c_str()) - 1;
			if (axeModel == Axe3 && !mAxeFx3Manager)
			{
				mAxeFx3Manager = std::make_shared<AxeFx3Manager>(mMainDisplay, mSwitchDisplay, mTraceDisplay, mApp->ApplicationDirectory(), axeCh, axeModel);
				mAxe3SyncPort = -1 == port ? 1 : port;
				mAxe3DeviceName = dev;
			}
			else if (axeModel != Axe3 && !mAxeFxManager)
			{
				mAxeFxManager = std::make_shared<AxeFxManager>(mMainDisplay, mSwitchDisplay, mTraceDisplay, mApp->ApplicationDirectory(), axeCh, axeModel);
				mAxeSyncPort = -1 == port ? 1 : port;
				mAxeDeviceName = dev;
			}
			else if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: multiple axe-fx devices\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
		}
	}
}

void
EngineLoader::InitDefaultLedPresetColors()
{
	std::array<unsigned int, 32> defaults
	{
		// normal
		0x00000b, //  1 blue
		0x000b00, //  2 green
		0x0b0000, //  3 red
		0x000b0b, //  4 cyan
		0x0b0b00, //  5 yellow
		0x0b000b, //  6 violet
		0x0b0b0b, //  7 white

		0x000000, //  8 blank

		// most dim
		0x000001, //  9 dimmest blue
		0x000100, // 10 dimmest green
		0x010000, // 11 dimmest red
		0x000101, // 12 dimmest cyan
		0x010100, // 13 dimmest yellow
		0x010001, // 14 dimmest violet
		0x010101, // 15 dimmest white

		// bright
		0x00001f, // 16 blue
		0x001f00, // 17 green
		0x1f0000, // 18 red
		0x001f1f, // 19 cyan
		0x1f1f00, // 20 yellow
		0x1f001f, // 21 violet
		0x1f1f1f, // 22 white

		0x000000, // 23 blank

		// dim
		0x000004, // 24 dim blue
		0x000400, // 25 dim green
		0x040000, // 26 dim red
		0x000404, // 27 dim cyan
		0x040400, // 28 dim yellow
		0x040004, // 29 dim violet
		0x040404, // 30 dim white

		0x000000, // 31 blank
		0x000000  // 32 blank
	};

	mLedPresetColors.swap(defaults);

	// set defaults that automatically differentiate from all else that default to kFirstPreset
	mLedDefaultColors[{"mtroll", "*", 1}] = kFirstColorPreset + 1;
}

void
EngineLoader::LoadLedPresetColors(TiXmlElement * pElem)
{
	/*
	 * optional, preset numbers valid over range 1-32, none required
		<LedPresetColors>
			<color preset="1">00007f</color> <!-- preset attribute required -->
			<color preset="3">007f00</color>
			<color preset="5">7f0000</color>
		</LedPresetColors>
	 */
	std::string colorStr;
	for (; pElem; pElem = pElem->NextSiblingElement())
	{
		if (pElem->ValueStr() != "color" && pElem->ValueStr() != "Color")
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: unrecognized element in LedPresetColors\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		if (!pElem->GetText())
			continue;
		colorStr = pElem->GetText();

		std::string presetSlotStr;
		pElem->QueryValueAttribute("preset", &presetSlotStr);
		if (presetSlotStr.empty())
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: missing preset number in LedPresetColors\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		int presetSlot = ::atoi(presetSlotStr.c_str()) - 1;
		if (presetSlot < 0 || presetSlot > 31)
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: invalid preset number in LedPresetColors -- valid presets are 1-32\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		mLedPresetColors[presetSlot] = ::StrToRgb(colorStr);
	}
}

void
EngineLoader::LoadLedDefaultColors(TiXmlElement * pElem)
{
	/*
	 * optional, preset numbers valid over range 1-32, none required
		<LedDefaultColors>
			<color type="normal" >00003f</color>
			<color type="toggle" device="*" >00003f</color>
			<color type="AxeScene" state="inactive" >00003f</color>
			<color type="AxeBlockE" colorPreset="1" />
		</LedDefaultColors>

		if device or type is not explicit, record as "*"
		state defaults to active, does not need to be explicit (active/inactive)
		preset notated as 1-based, decrement internally
		preset recorded as color with high-bit set
	 */
	int presetSlot, st;
	unsigned int clr;
	for (; pElem; pElem = pElem->NextSiblingElement())
	{
		if (pElem->ValueStr() != "color")
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: unrecognized element in LedDefaultColors -- expecting only color elements\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		// optional
		std::string device;
		if (pElem->Attribute("device"))
			device = pElem->Attribute("device");
		if (device.empty())
			device = "*";
		else if (device != "*" && device != "mtroll" && device != "midi" && mDeviceChannels.find(device) == mDeviceChannels.end())
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: device specified in LedDefaultColors is not defined in DeviceChannelMap\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
		}

		// optional
		std::string patchType;
		pElem->QueryValueAttribute("type", &patchType);
		if (patchType.empty())
			patchType = "*";

		if (device == "*" && patchType == "*")
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: invalid condition in LedDefaultColors -- neither device nor type are specified\n" << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		presetSlot = 0;
		pElem->QueryIntAttribute("preset", &presetSlot);
		if (presetSlot)
		{
			if (presetSlot < 1 || presetSlot > 32)
			{
				if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: invalid preset number in LedDefaultColors -- valid preset slots are 1-32\n" << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}

			// convert to 0-based
			--presetSlot;

			// preset slots are stored as color with high bit
			clr = kPresetColorMarkerBit | presetSlot;
		}
		else
		{
			// no preset, so check for RGB text
			if (!pElem->GetText())
			{
				if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: missing color in LedDefaultColors -- no color or colorPreset specified\n" << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}

			std::string colorStr{ pElem->GetText() };
			if (colorStr.length() != 6)
			{
				if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: invalid color in LedDefaultColors -- specify RGB color as 6 digit hex\n" << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}

			clr = ::StrToRgb(colorStr);
			if (clr & 0xFF000000)
			{
				if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: invalid color in LedDefaultColors -- specify RGB color as 6 digit hex (hi-bits present)\n" << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}
		}

		std::string stateStr;
		pElem->QueryValueAttribute("state", &stateStr);
		if (stateStr.empty())
			st = 1;
		else
		{
			if (stateStr == "active")
				st = 1;
			else if (stateStr == "inactive")
				st = 0;
			else
			{
				if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: invalid state in LedDefaultColors -- should be either active or inactive\n" << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}
		}

		std::transform(device.begin(), device.end(), device.begin(),
			[](unsigned char c) { return std::tolower(c); }
		);

		std::transform(patchType.begin(), patchType.end(), patchType.begin(),
			[](unsigned char c) { return std::tolower(c); }
		);

		mLedDefaultColors[{device, patchType, st}] = clr;
	}
}

PatchBank::PatchState
GetLoadState(const std::string & loadState)
{
	if (loadState == "A")
		return PatchBank::stA;
	else if (loadState == "B")
		return PatchBank::stB;
	else
		return PatchBank::stIgnore;
}

PatchBank::PatchSyncState
GetSyncState(const std::string & syncState)
{
	if (syncState == "syncInPhase" || syncState == "sync" || syncState == "inPhase")
		return PatchBank::syncInPhase;
	else if (syncState == "syncOutOfPhase" || syncState == "outOfPhase")
		return PatchBank::syncOutOfPhase;
	else
		return PatchBank::syncIgnore;
}

PatchBank::SecondFunctionOperation
GetSecondFuncOp(const std::string & secFuncOp)
{
	// secondFunction="manual|auto|autoOn|autoOff"
	if (secFuncOp == "manual")
		return PatchBank::sfoManual;
	else if (secFuncOp == "auto")
		return PatchBank::sfoAuto;
	else if (secFuncOp == "autoOn")
		return PatchBank::sfoAutoEnable;
	else if (secFuncOp == "autoOff")
		return PatchBank::sfoAutoDisable;
	else if (secFuncOp == "immediateToggle")
		return PatchBank::sfoStatelessToggle;
	else
		return PatchBank::sfoManual;
}

void
ReadTwoIntValues(std::string tmp, int & val1, int & val2)
{
	if (tmp.empty())
		return;

	int tokenPos = tmp.find_first_of(" ,:;");
	if (std::string::npos == tokenPos)
		return;

	val1 = ::atoi(tmp.c_str());

	tmp = tmp.substr(tokenPos + 1);
	while ((tokenPos = tmp.find_first_of(" ,:;")) != std::string::npos)
		tmp = tmp.substr(tokenPos + 1);

	if (!tmp.empty())
		val2 = ::atoi(tmp.c_str());
}

const std::map<const std::string, int> kMidiNoteValues
{
	{ "C-1",	0 },
	{ "C#-1",	1 },
	{ "D-1",	2 },
	{ "D#-1",	3 },
	{ "E-1",	4 },
	{ "F-1",	5 },
	{ "F#-1",	6 },
	{ "G-1",	7 },
	{ "G#-1",	8 },
	{ "A-1",	9 },
	{ "A#-1",	10 },
	{ "B-1",	11 },

	{ "C0",		12 },
	{ "C#0",	13 },
	{ "D0",		14 },
	{ "D#0",	15 },
	{ "E0",		16 },
	{ "F0",		17 },
	{ "F#0",	18 },
	{ "G0",		19 },
	{ "G#0",	20 },
	{ "A0",		21 },
	{ "A#0",	22 },
	{ "B0",		23 },

	{ "C1",		24 },
	{ "C#1",	25 },
	{ "D1",		26 },
	{ "D#1",	27 },
	{ "E1",		28 },
	{ "F1",		29 },
	{ "F#1",	30 },
	{ "G1",		31 },
	{ "G#1",	32 },
	{ "A1",		33 },
	{ "A#1",	34 },
	{ "B1",		35 },

	{ "C2",		36 },
	{ "C#2",	37 },
	{ "D2",		38 },
	{ "D#2",	39 },
	{ "E2",		40 },
	{ "F2",		41 },
	{ "F#2",	42 },
	{ "G2",		43 },
	{ "G#2",	44 },
	{ "A2",		45 },
	{ "A#2",	46 },
	{ "B2",		47 },

	{ "C3",		48 },
	{ "C#3",	49 },
	{ "D3",		50 },
	{ "D#3",	51 },
	{ "E3",		52 },
	{ "F3",		53 },
	{ "F#3",	54 },
	{ "G3",		55 },
	{ "G#3",	56 },
	{ "A3",		57 },
	{ "A#3",	58 },
	{ "B3",		59 },

	{ "C4",		60 },
	{ "C#4",	61 },
	{ "D4",		62 },
	{ "D#4",	63 },
	{ "E4",		64 },
	{ "F4",		65 },
	{ "F#4",	66 },
	{ "G4",		67 },
	{ "G#4",	68 },
	{ "A4",		69 },
	{ "A#4",	70 },
	{ "B4",		71 },

	{ "C5",		72 },
	{ "C#5",	73 },
	{ "D5",		74 },
	{ "D#5",	75 },
	{ "E5",		76 },
	{ "F5",		77 },
	{ "F#5",	78 },
	{ "G5",		79 },
	{ "G#5",	80 },
	{ "A5",		81 },
	{ "A#5",	82 },
	{ "B5",		83 },

	{ "C6",		84 },
	{ "C#6",	85 },
	{ "D6",		86 },
	{ "D#6",	87 },
	{ "E6",		88 },
	{ "F6",		89 },
	{ "F#6",	90 },
	{ "G6",		91 },
	{ "G#6",	92 },
	{ "A6",		93 },
	{ "A#6",	94 },
	{ "B6",		95 },

	{ "C7",		96 },
	{ "C#7",	97 },
	{ "D7",		98 },
	{ "D#7",	99 },
	{ "E7",		100 },
	{ "F7",		101 },
	{ "F#7",	102 },
	{ "G7",		103 },
	{ "G#7",	104 },
	{ "A7",		105 },
	{ "A#7",	106 },
	{ "B7",		107 },

	{ "C8",		108 },
	{ "C#8",	109 },
	{ "D8",		110 },
	{ "D#8",	111 },
	{ "E8",		112 },
	{ "F8",		113 },
	{ "F#8",	114 },
	{ "G8",		115 },
	{ "G#8",	116 },
	{ "A8",		117 },
	{ "A#8",	118 },
	{ "B8",		119 },

	{ "C9",		120 },
	{ "C#9",	121 },
	{ "D9",		122 },
	{ "D#9",	123 },
	{ "E9",		124 },
	{ "F9",		125 },
	{ "F#9",	126 },
	{ "G9",		127 }
};

int
StringToNoteValue(const char *noteData)
{
	if (!noteData)
		return 0;

	auto it = kMidiNoteValues.find(noteData);
	if (it != kMidiNoteValues.end())
	{
		// noteData was a note name
		return it->second;
	}

	// noteData must have been a midi note number

#ifdef _DEBUG
	{
		char ch;
		for (int idx = 0; (ch = noteData[idx]) != 0; ++idx)
		{
			_ASSERTE(ch >= '0' && ch <= '9');
		}
	}
#endif

	return ::atoi(noteData);
}
