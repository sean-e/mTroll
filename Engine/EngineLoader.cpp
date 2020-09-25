/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2015,2018,2020 Sean Echevarria
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


#ifdef _MSC_VER
#pragma warning(disable:4482)
#endif


static PatchBank::PatchState GetLoadState(const std::string & tmpLoad);
static PatchBank::PatchSyncState GetSyncState(const std::string & tmpLoad);
static PatchBank::SecondFunctionOperation GetSecondFuncOp(const std::string & tmp);

EngineLoader::EngineLoader(ITrollApplication * app,
						   IMidiOutGenerator * midiOutGenerator,
						   IMidiInGenerator * midiInGenerator,
						   IMainDisplay * mainDisplay,
						   ISwitchDisplay * switchDisplay,
						   ITraceDisplay * traceDisplay) :
	mEngine(nullptr),
	mApp(app),
	mMidiOutGenerator(midiOutGenerator),
	mMidiInGenerator(midiInGenerator),
	mMainDisplay(mainDisplay),
	mSwitchDisplay(switchDisplay),
	mTraceDisplay(traceDisplay),
	mAxeFxManager(nullptr),
	mAxeFx3Manager(nullptr),
	mAxeSyncPort(-1),
	mAxe3SyncPort(-1)
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
			traceMsg << "Error loading config file: failed to load file " << err << std::endl << std::ends;
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
			traceMsg << "Error loading config file: invalid config file" << std::endl << std::ends;
			mTraceDisplay->Trace(std::string(traceMsg.str()));
		}
		return mEngine;
	}

	if (pElem->ValueStr() != "MidiControlSettings")
	{
		if (mTraceDisplay)
		{
			std::strstream traceMsg;
			traceMsg << "Error loading config file: invalid config file" << std::endl << std::ends;
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

	pElem = hRoot.FirstChild("SystemConfig").Element();
	if (!LoadSystemConfig(pElem))
	{
		if (mTraceDisplay)
		{
			std::strstream traceMsg;
			traceMsg << "Error loading config file: invalid SystemConfig" << std::endl << std::ends;
			mTraceDisplay->Trace(std::string(traceMsg.str()));
		}
		return mEngine;
	}

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
	int powerupTimeout = 0;
	int powerupPatch = 0;
	int powerupBank = 0;
	int incrementSwitch = -1;
	int decrementSwitch = -1;
	int modeSwitch = -1;

	pElem->QueryIntAttribute("filterPC", &filterPC);

	TiXmlHandle hRoot(nullptr);
	hRoot = TiXmlHandle(pElem);

	TiXmlElement * pChildElem = hRoot.FirstChild("powerup").Element();
	if (pChildElem)
	{
		pChildElem->QueryIntAttribute("timeout", &powerupTimeout);
		pChildElem->QueryIntAttribute("bank", &powerupBank);
		pChildElem->QueryIntAttribute("patch", &powerupPatch);
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
			unsigned int idx = mMidiOutGenerator->GetMidiOutDeviceIndex(outDevice);
			if (-1 != idx)
				deviceIdx = idx;
			else if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file midiDevices section: MidiOut device name not found: " << outDevice << std::endl << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));

				if (-1 == deviceIdx)
				{
					// do not default to -1 if name was specified (if idx specified, then try it)
					continue;
				}
			}
		}

		if (!inDevice.empty() && mMidiInGenerator)
		{
			unsigned int idx = mMidiInGenerator->GetMidiInDeviceIndex(inDevice);
			if (-1 != idx)
				inDeviceIdx = idx;
			else if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file midiDevices section: MidiIn device name not found: " << inDevice << std::endl << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));

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
					if (mAxeFx3Manager && port == mAxe3SyncPort && midiIn)
						mAxeFx3Manager->SubscribeToMidiIn(midiIn);
					if (mAxeFxManager && port == mAxeSyncPort && midiIn)
						mAxeFxManager->SubscribeToMidiIn(midiIn);
				}
			}
		}
	}

	IMidiOutPtr engOut; // for direct program change use
	if (!mMidiOutPortToDeviceIdxMap.empty())
		engOut = mMidiOutGenerator->GetMidiOut((*mMidiOutPortToDeviceIdxMap.begin()).second);

	mEngine = std::make_shared<MidiControlEngine>(mApp, mMainDisplay, mSwitchDisplay, mTraceDisplay,
		engOut, mAxeFxManager, mAxeFx3Manager, incrementSwitch, decrementSwitch, modeSwitch);
	mEngine->SetPowerup(powerupBank, powerupPatch, powerupTimeout);
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
				continue;

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

		if (MidiControlEngine::kUnassignedSwitchNumber != m)
			mEngine->AssignModeSwitchNumber(m, switchNumber);
	}

	return true;
}

void
EngineLoader::LoadPatches(TiXmlElement * pElem)
{
	for ( ; pElem; pElem = pElem->NextSiblingElement())
	{
		std::string patchName;

		if (pElem->ValueStr() != "patch")
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: unrecognized Patches item" << std::endl << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		if (pElem->Attribute("name"))
			patchName = pElem->Attribute("name");
		int patchNumber = -1;
		pElem->QueryIntAttribute("number", &patchNumber);
		if (-1 == patchNumber || patchName.empty())
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: patch missing number or name" << std::endl << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		std::string patchType;
		pElem->QueryValueAttribute("type", &patchType);

		std::string device;
		if (pElem->Attribute("device"))
			device = pElem->Attribute("device");

		// optional default channel in <patch> so that it doesn't need to be specified in each patch command
		int patchDefaultCh = -1;
		{
			std::string chStr;
			pElem->QueryValueAttribute("channel", &chStr);
			if (chStr.empty() && !device.empty())
				chStr = mDeviceChannels[device];

			if (!chStr.empty())
			{
				patchDefaultCh = ::atoi(chStr.c_str()) - 1;
				if (patchDefaultCh < 0 || patchDefaultCh > 15)
				{
					if (mTraceDisplay)
					{
						std::strstream traceMsg;
						traceMsg << "Error loading config file: invalid command channel in patch " << patchName << std::endl << std::ends;
						mTraceDisplay->Trace(std::string(traceMsg.str()));
					}
					patchDefaultCh = -1;
				}
			}
		}

		int midiOutPortNumber = -1;
		pElem->QueryIntAttribute("port", &midiOutPortNumber);
		if (-1 == midiOutPortNumber && !device.empty())
			midiOutPortNumber = mDevicePorts[device]; // see if a port mapping was set up for the device

		if (-1 == midiOutPortNumber)
			midiOutPortNumber = 1;

		IMidiOutPtr midiOut = mMidiOutGenerator->GetMidiOut(mMidiOutPortToDeviceIdxMap[midiOutPortNumber]);

		IAxeFxPtr mgr = GetAxeMgr(pElem);
		PatchCommands cmds, cmds2;
		std::vector<int> intList;
		TiXmlHandle hRoot(nullptr);
		hRoot = TiXmlHandle(pElem);

		TiXmlElement * childElem;
		for (childElem = hRoot.FirstChildElement().Element(); 
			 childElem; 
			 childElem = childElem->NextSiblingElement())
		{
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
						traceMsg << "Error loading config file: invalid midiByteString in patch " << patchName << std::endl << std::ends;
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
					traceMsg << "Error loading config file: invalid pedal specified in RefirePedal in patch " << patchName << std::endl << std::ends;
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
					traceMsg << "Error loading config file: no (or invalid) time specified in Sleep in patch " << patchName << std::endl << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}
			else if (patchElement == "localExpr")
				continue;
			else if (patchElement == "globalExpr")
				continue;
			else if (patchElement == "patchListItem")
			{
				std::string patchNumStr;
				if (childElem->GetText())
					patchNumStr = childElem->GetText();
				const int num = ::atoi(patchNumStr.c_str());
				if (-1 != num)
				{
					intList.push_back(num);
				}
				else if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: no (or invalid) patch number specified in patchListItem in patch " << patchName << std::endl << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}
			else
			{
				std::string chStr;
				childElem->QueryValueAttribute("channel", &chStr);
				if (chStr.empty())
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
								traceMsg << "Error loading config file: command missing channel in patch " << patchName << std::endl << std::ends;
								mTraceDisplay->Trace(std::string(traceMsg.str()));
							}
							continue;
						}
					}
				}

				int ch = -1;
				if (!chStr.empty())
					ch = ::atoi(chStr.c_str()) - 1;

				if (ch < 0 || ch > 15)
				{
					if (patchDefaultCh < 0 || patchDefaultCh > 15)
					{
						if (mTraceDisplay)
						{
							std::strstream traceMsg;
							traceMsg << "Error loading config file: invalid command channel in patch " << patchName << std::endl << std::ends;
							mTraceDisplay->Trace(std::string(traceMsg.str()));
						}
						continue;
					}
					else
						ch = patchDefaultCh;
				}

				byte cmdByte = 0;
				int data1 = 0, data2 = 0;
				bool useDataByte2 = true;

				if (patchElement == "ProgramChange")
				{
					// <ProgramChange group="A" channel="0" program="0" />
					childElem->QueryIntAttribute("program", &data1);
					cmdByte = 0xc0;
					useDataByte2 = false;
				}
				else if (patchElement == "AxeProgramChange")
				{
					// <AxeProgramChange group="A" device="Axe-Fx" program="0" />
					// use data2 as bank change
					// data1 val range 0 - 383
					childElem->QueryIntAttribute("program", &data1);
					if (0 > data1 || data1 > 383)
					{
						if (mTraceDisplay)
						{
							std::strstream traceMsg;
							traceMsg << "Error loading config file: too large a preset specified for AxeProgramChange in patch " << patchName << std::endl << std::ends;
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
				else if (patchElement == "ControlChange")
				{
					// <ControlChange group="A" channel="0" controller="0" value="0" />
					childElem->QueryIntAttribute("controller", &data1);
					childElem->QueryIntAttribute("value", &data2);
					cmdByte = 0xb0;
				}
				else if (patchElement == "NoteOn")
				{
					// <NoteOn group="B" channel="0" note="0" velocity="0" />
					childElem->QueryIntAttribute("note", &data1);
					childElem->QueryIntAttribute("velocity", &data2);
					cmdByte = 0x90;
				}
				else if (patchElement == "NoteOff")
				{
					// <NoteOff group="B" channel="0" note="0" velocity="0" />
					childElem->QueryIntAttribute("note", &data1);
					childElem->QueryIntAttribute("velocity", &data2);
					cmdByte = 0x80;
				}
				else if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: unrecognized command in patch " << patchName << std::endl << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}

				if (cmdByte)
				{
					if ((data1 > 0xff) || (useDataByte2 && data2 > 0xff))
					{
						if (mTraceDisplay)
						{
							std::strstream traceMsg;
							traceMsg << "Error loading config file: too large a value specified for command in patch " << patchName << std::endl << std::ends;
							mTraceDisplay->Trace(std::string(traceMsg.str()));
						}
						continue;
					}

					bytes.push_back(cmdByte | ch);
					bytes.push_back(data1);
					if (useDataByte2)
						bytes.push_back(data2);
				}
			}

			if (!bytes.empty())
			{
				if (group == "B")
					cmds2.push_back(std::make_shared<MidiCommandString>(midiOut, bytes));
				else
					cmds.push_back(std::make_shared<MidiCommandString>(midiOut, bytes));
			}
		}

		int axeFxScene = 0;
		int axeFxBlockId = 0;
		int axeFxBlockChannel = 0;
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
							cmds.push_back(std::make_shared<MidiCommandString>(midiOut, b1));

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
		else if (patchType == "persistentPedalOverride")
			newPatch = std::make_shared<PersistentPedalOverridePatch>(patchNumber, patchName, midiOut, cmds, cmds2);
		else if (patchType == "AxeToggle")
		{
			auto axePatch = std::make_shared<AxeTogglePatch>(patchNumber, patchName, midiOut, cmds, cmds2, mgr, axeFxScene);
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
				traceMsg << "Error loading config file: Axe-Fx device specified but no Axe-Fx input was created" << std::endl << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}

			newPatch = axePatch;
		}
		else if (patchType == "momentary")
			newPatch = std::make_shared<MomentaryPatch>(patchNumber, patchName, midiOut, cmds, cmds2);
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
				traceMsg << "Error loading config file: Axe-Fx device specified but no Axe-Fx input was created" << std::endl << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}

			newPatch = axePatch;
		}
		else if (patchType == "sequence")
			newPatch = std::make_shared<SequencePatch>(patchNumber, patchName, midiOut, cmds);
		else if (patchType == "patchListSequence")
		{
			int immediateWrap = 0;
			pElem->QueryIntAttribute("gaplessRestart", &immediateWrap);
			newPatch = std::make_shared<PatchListSequencePatch>(patchNumber, patchName, intList, immediateWrap);
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
				traceMsg << "Error loading config file: AxeFxTapTempo specified but no Axe-Fx input was created" << std::endl << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
		}
		else
			patchTypeErr = true;

		if (patchTypeErr && mTraceDisplay)
		{
			std::strstream traceMsg;
			traceMsg << "Error loading config file: invalid patch type specified for patch " << patchName << std::endl << std::ends;
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
				patchType = "axeblock";

			if (-1 == ledActiveColor)
				ledActiveColor = LookUpColor(device, patchType, 1);

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
			traceMsg << "Error loading config file: ledColor attribute incompatible with ledColorPreset attribute" << std::endl << std::ends;
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
				traceMsg << "Error loading config file: color attribute should be specified as 6 hex chars" << std::endl << std::ends;
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
				traceMsg << "Error loading config file: color attribute should be specified as 6 hex chars" << std::endl << std::ends;
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
				traceMsg << "Error loading config file: invalid ledColorPreset on patch -- valid range of values 1-32" << std::endl << std::ends;
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
				traceMsg << "Error loading config file: invalid ledInactiveColorPreset on patch -- valid range of values 1-32" << std::endl << std::ends;
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
	int autoGenPatchNumber = (int)ReservedPatchNumbers::kAutoGenPatchNumberStart;
	for ( ; pElem; pElem = pElem->NextSiblingElement())
	{
		if (pElem->ValueStr() != "bank")
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: unrecognized Banks item" << std::endl << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		std::string bankName;
		if (pElem->Attribute("name"))
			bankName = pElem->Attribute("name");
		int bankNumber = -1;
		pElem->QueryIntAttribute("number", &bankNumber);
		if (-1 == bankNumber || bankName.empty())
		{
			if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: bank missing number or name" << std::endl << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		PatchBankPtr bank = mEngine->AddBank(bankNumber, bankName);

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
						traceMsg << "Error loading config file: invalid switch number for bank " << bankName << std::endl << std::ends;
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
							traceMsg << "Error loading config file: switch " << switchNumber << " in bank " << bankNumber << " missing action" << std::endl << std::ends;
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
							if (mTraceDisplay)
							{
								std::strstream traceMsg;
								traceMsg << "Error loading config file: switch " << nameOverride << " missing LoadBank target" << std::endl << std::ends;
								mTraceDisplay->Trace(std::string(traceMsg.str()));
							}
							continue;
						}
					}
					else if (cmdName == "ResetPatch")
					{
						int patchNumToReset = -1;
						childElem->QueryIntAttribute("patchToReset", &patchNumToReset);
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
								traceMsg << "Error loading config file: switch " << nameOverride << " missing DeactivatePatch switch target" << std::endl << std::ends;
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
								traceMsg << "Error loading config file: switch " << nameOverride << " missing ResetExclusiveGroup switch target" << std::endl << std::ends;
								mTraceDisplay->Trace(std::string(traceMsg.str()));
							}
							continue;
						}
					}
					else
					{
						if (mTraceDisplay)
						{
							std::strstream traceMsg;
							traceMsg << "Error loading config file: switch " << nameOverride << " unknown command " << cmdName << std::endl << std::ends;
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
// 					traceMsg << "Warning load of config file: bank " << bankName << " has a switch with both override and sync attributes (might not work)" << std::endl << std::ends;
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
				traceMsg << "Error loading config file: unrecognized element in bank " << bankName << std::endl << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
		}
	}
}

unsigned int
EngineLoader::LookUpColor(std::string device, std::string patchType, int activeState, unsigned int defaultColor /*= kFirstPreset*/)
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
	childElem->QueryIntAttribute("topTogglePatchNumber", &topTogglePatchNumber);

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
			traceMsg << "Error loading config file: expression pedal settings error" << std::endl << std::ends;
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
			traceMsg << "  ADC port " << idx << (enable ? " enabled" : " disabled") << std::endl << std::ends;
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
				traceMsg << "Error loading config file: unrecognized element in DeviceChannelMap" << std::endl << std::ends;
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
				traceMsg << "Error loading config file: missing channel name in DeviceChannelMap" << std::endl << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		mDeviceChannels[dev] = ch;

		// optional midi port device mapping
		int port = -1;
		pElem->QueryIntAttribute("port", &port);
		mDevicePorts[dev] = port;

		if (dev == "AxeFx3" ||
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
				traceMsg << "Error loading config file: multiple axe-fx 3 devices" << std::endl << std::ends;
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
				traceMsg << "Error loading config file: multiple axe-fx devices" << std::endl << std::ends;
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
		0x00000b, // blue
		0x000b00, // green
		0x0b0000, // red
		0x000b0b, // cyan
		0x0b0b00, // yellow
		0x0b000b, // violet
		0x0b0b0b, // white

		0x000000, // blank

		// most dim
		0x000001, // dimmest blue
		0x000100, // dimmest green
		0x010000, // dimmest red
		0x000101, // dimmest cyan
		0x010100, // dimmest yellow
		0x010001, // dimmest violet
		0x010101, // dimmest white

		// bright
		0x00001f, // blue
		0x001f00, // green
		0x1f0000, // red
		0x001f1f, // cyan
		0x1f1f00, // yellow
		0x1f001f, // violet
		0x1f1f1f, // white

		0x000000, // blank

		// dim
		0x000004, // dim blue
		0x000400, // dim green
		0x040000, // dim red
		0x000404, // dim cyan
		0x040400, // dim yellow
		0x040004, // dim violet
		0x040404, // dim white

		0x000000, // blank
		0x000000  // blank
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
				traceMsg << "Error loading config file: unrecognized element in LedPresetColors" << std::endl << std::ends;
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
				traceMsg << "Error loading config file: missing preset number in LedPresetColors" << std::endl << std::ends;
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
				traceMsg << "Error loading config file: invalid preset number in LedPresetColors -- valid presets are 1-32" << std::endl << std::ends;
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
				traceMsg << "Error loading config file: unrecognized element in LedDefaultColors -- expecting only color elements" << std::endl << std::ends;
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
				traceMsg << "Error loading config file: device specified in LedDefaultColors is not defined in DeviceChannelMap" << std::endl << std::ends;
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
				traceMsg << "Error loading config file: invalid condition in LedDefaultColors -- neither device nor type are specified" << std::endl << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		presetSlot = 0;
		pElem->QueryIntAttribute("colorPreset", &presetSlot);
		if (presetSlot)
		{
			if (presetSlot < 1 || presetSlot > 32)
			{
				if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: invalid preset number in LedDefaultColors -- valid preset slots are 1-32" << std::endl << std::ends;
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
					traceMsg << "Error loading config file: missing color in LedDefaultColors -- no color or colorPreset specified" << std::endl << std::ends;
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
					traceMsg << "Error loading config file: invalid color in LedDefaultColors -- specify RGB color as 6 digit hex" << std::endl << std::ends;
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
					traceMsg << "Error loading config file: invalid color in LedDefaultColors -- specify RGB color as 6 digit hex (hi-bits present)" << std::endl << std::ends;
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
					traceMsg << "Error loading config file: invalid state in LedDefaultColors -- should be either active or inactive" << std::endl << std::ends;
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
