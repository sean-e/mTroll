/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2014 Sean Echevarria
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
#include "IMidiIn.h"
#include "ITrollApplication.h"
#include "AxeTogglePatch.h"
#include "AxeFxProgramChange.h"
#include "PatchListSequencePatch.h"
#include "MetaPatch_ResetExclusiveGroup.h"


static PatchBank::PatchState GetLoadState(const std::string & tmpLoad);
static PatchBank::PatchSyncState GetSyncState(const std::string & tmpLoad);
static PatchBank::SecondFunctionOperation GetSecondFuncOp(const std::string & tmp);

EngineLoader::EngineLoader(ITrollApplication * app,
						   IMidiOutGenerator * midiOutGenerator,
						   IMidiInGenerator * midiInGenerator,
						   IMainDisplay * mainDisplay,
						   ISwitchDisplay * switchDisplay,
						   ITraceDisplay * traceDisplay) :
	mEngine(NULL),
	mApp(app),
	mMidiOutGenerator(midiOutGenerator),
	mMidiInGenerator(midiInGenerator),
	mMainDisplay(mainDisplay),
	mSwitchDisplay(switchDisplay),
	mTraceDisplay(traceDisplay),
	mAxeFxManager(NULL),
	mAxeSyncPort(-1)
{
	for (int idx = 0; idx < 4; ++idx)
		mAdcEnables[idx] = adc_default;
}

EngineLoader::~EngineLoader()
{
	if (mAxeFxManager)
		mAxeFxManager->Release();
}

MidiControlEngine *
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

	TiXmlHandle hRoot(NULL);
	hRoot = TiXmlHandle(pElem);

	// load DeviceChannelMap before SystemConfig so that pedals can use Devices
	pElem = hRoot.FirstChild("DeviceChannelMap").FirstChildElement().Element();
	if (pElem)
		LoadDeviceChannelMap(pElem);

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

	mMidiOutGenerator->OpenMidiOuts();
	if (mMidiInGenerator)
		mMidiInGenerator->OpenMidiIns();
	if (mAxeFxManager)
	{
		IMidiOut * midiOut = NULL;
		std::string axeName("AxeFx");
		std::map<std::string, int>::iterator it = mDevicePorts.find(axeName);
		if (it == mDevicePorts.end())
		{
			axeName = "Axe-Fx";
			it = mDevicePorts.find(axeName);
		}

		if (it != mDevicePorts.end())
		{
			int port = mDevicePorts[axeName];
			midiOut = mMidiOutGenerator->GetMidiOut(mMidiOutPortToDeviceIdxMap[port]);
		}
		mAxeFxManager->CompleteInit(midiOut);
	}
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

	pChildElem = hRoot.FirstChild("switches").FirstChildElement().Element();
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
	// <midiDevice port="2" out="Axe-Fx II" in="Axe-Fx II" activityIndicatorId="100" />
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
			}
		}

		if (-1 == inDeviceIdx || -1 != deviceIdx)
		{
			if (-1 == deviceIdx)
				deviceIdx = 1; // maintain back compat with original behavior
			mMidiOutPortToDeviceIdxMap[port] = deviceIdx;
			mMidiOutGenerator->CreateMidiOut(mMidiOutPortToDeviceIdxMap[port], activityIndicatorId);
		}

		if (-1 != inDeviceIdx)
		{
			mMidiInPortToDeviceIdxMap[port] = inDeviceIdx;
			if (mMidiInGenerator)
			{
				IMidiIn * midiIn = mMidiInGenerator->CreateMidiIn(mMidiInPortToDeviceIdxMap[port]);
				if (mAxeFxManager && port == mAxeSyncPort && midiIn)
				{
					if (midiIn->Subscribe(mAxeFxManager))
						mAxeFxManager->AddRef();
				}
			}
		}
	}

	IMidiOut * engOut = NULL; // for direct program change use
	if (mMidiOutPortToDeviceIdxMap.size())
		engOut = mMidiOutGenerator->GetMidiOut((*mMidiOutPortToDeviceIdxMap.begin()).second);

	mEngine = new MidiControlEngine(mApp, mMainDisplay, mSwitchDisplay, mTraceDisplay, 
		engOut, mAxeFxManager, incrementSwitch, decrementSwitch, modeSwitch);
	mEngine->SetPowerup(powerupBank, powerupPatch, powerupTimeout);
	mEngine->FilterRedundantProgChg(filterPC ? true : false);

	// <expression port="">
	//   <globaExpr inputNumber="1" assignmentNumber="1" channel="" controller="" min="" max="" invert="0" enable="" />
	//   <adc inputNumber="" enable="" />
	// </expression>
	ExpressionPedals & globalPedals = mEngine->GetPedals();
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

	int midiOutPortNumber = -1;
	pChildElem = hRoot.FirstChild("expression").Element();
	if (pChildElem)
	{
		pChildElem->QueryIntAttribute("port", &midiOutPortNumber);
		if (-1 == midiOutPortNumber && mMidiOutPortToDeviceIdxMap.begin() != mMidiOutPortToDeviceIdxMap.end())
			midiOutPortNumber = (*mMidiOutPortToDeviceIdxMap.begin()).second;
		if (-1 == midiOutPortNumber)
			return false;
		IMidiOut * globalExprPedalMidiOut = mMidiOutGenerator->GetMidiOut(mMidiOutPortToDeviceIdxMap[midiOutPortNumber]);
		if (!globalExprPedalMidiOut)
			return false;

		globalPedals.InitMidiOut(globalExprPedalMidiOut);
	}

	pChildElem = hRoot.FirstChild("switches").FirstChildElement().Element();
	for ( ; pChildElem; pChildElem = pChildElem->NextSiblingElement())
	{
		std::string name;
		pChildElem->QueryValueAttribute("name", &name);
		if (name.empty() || name == "mode" || name == "increment" || name == "decrement")
			continue;

		int switchNumber = -1;
		pChildElem->QueryIntAttribute("id", &switchNumber);
		if (-1 == switchNumber)
			continue;

		if (name == "bankLoad")
		{
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
		else if (name == "toggleLedInversion") m = MidiControlEngine::kModeToggleLedInversion;

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
		if (pElem->ValueStr() == "engineMetaPatch")
		{
			if (pElem->Attribute("name"))
				patchName = pElem->Attribute("name");
			int patchNumber = -1;
			pElem->QueryIntAttribute("number", &patchNumber);
			if (-1 == patchNumber || patchName.empty())
			{
				if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: engineMetaPatch missing number or name" << std::endl << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}

			std::string tmp;
			pElem->QueryValueAttribute("action", &tmp);
			if (tmp.empty())
			{
				if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: engineMetaPatch " << patchName << " missing action" << std::endl << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
				continue;
			}

			if (tmp == "ResetBankPatches")
				 mEngine->AddPatch(new MetaPatch_ResetBankPatches(mEngine, patchNumber, patchName));
			else if (tmp == "SyncAxeFx")
				 mEngine->AddPatch(new MetaPatch_SyncAxeFx(mAxeFxManager, patchNumber, patchName));
			else if (tmp == "LoadNextBank")
				mEngine->AddPatch(new MetaPatch_LoadNextBank(mEngine, patchNumber, patchName));
			else if (tmp == "LoadPreviousBank")
				mEngine->AddPatch(new MetaPatch_LoadPreviousBank(mEngine, patchNumber, patchName));
			else if (tmp == "LoadBank")
			{
				int bankNumber = -1;
				pElem->QueryIntAttribute("bankNumber", &bankNumber);
				if (-1 != bankNumber)
				{
					mEngine->AddPatch(new MetaPatch_LoadBank(mEngine, patchNumber, patchName, bankNumber));
				}
				else if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: engineMetaPatch " << patchName << " missing LoadBank target" << std::endl << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
			}
			else if (tmp == "ResetExclusiveGroup")
			{
				int activeSwitch = -1;
				pElem->QueryIntAttribute("activeSwitch", &activeSwitch);
				if (-1 != activeSwitch)
				{
					mEngine->AddPatch(new MetaPatch_ResetExclusiveGroup(mEngine, patchNumber, patchName, activeSwitch));
				}
				else if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: engineMetaPatch " << patchName << " missing ResetExclusiveGroup switch target" << std::endl << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
			}
			else if (tmp == "BankHistoryBackward")
				 mEngine->AddPatch(new MetaPatch_BankHistoryBackward(mEngine, patchNumber, patchName));
			else if (tmp == "BankHistoryForward")
				 mEngine->AddPatch(new MetaPatch_BankHistoryForward(mEngine, patchNumber, patchName));
			else if (tmp == "BankHistoryRecall")
				 mEngine->AddPatch(new MetaPatch_BankHistoryRecall(mEngine, patchNumber, patchName));
			else
			{
				if (mTraceDisplay)
				{
					std::strstream traceMsg;
					traceMsg << "Error loading config file: engineMetaPatch " << patchName << " unknown action " << tmp << std::endl << std::ends;
					mTraceDisplay->Trace(std::string(traceMsg.str()));
				}
			}
			continue;
		}

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

		IMidiOut * midiOut = mMidiOutGenerator->GetMidiOut(mMidiOutPortToDeviceIdxMap[midiOutPortNumber]);

		PatchCommands cmds, cmds2;
		std::vector<int> intList;
		TiXmlHandle hRoot(NULL);
		hRoot = TiXmlHandle(pElem);

		TiXmlElement * childElem;
		for (childElem = hRoot.FirstChildElement().Element(); 
			 childElem; 
			 childElem = childElem->NextSiblingElement())
		{
			Bytes bytes;
			const std::string patchElement = childElem->ValueStr();
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
						cmds2.push_back(new RefirePedalCommand(mEngine, pedalNumber));
					else
						cmds.push_back(new RefirePedalCommand(mEngine, pedalNumber));
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
						cmds2.push_back(new SleepCommand(sleepAmt));
					else
						cmds.push_back(new SleepCommand(sleepAmt));
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
				if (0 <num)
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
					std::string device;
					if (childElem->Attribute("device"))
						device = childElem->Attribute("device");
					if (!device.empty())
						chStr = mDeviceChannels[device];
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
						cmds2.push_back(new AxeFxProgramChange(midiOut, bytes, mAxeFxManager));
					else
						cmds.push_back(new AxeFxProgramChange(midiOut, bytes, mAxeFxManager));
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

			if (bytes.size())
			{
				if (group == "B")
					cmds2.push_back(new MidiCommandString(midiOut, bytes));
				else
					cmds.push_back(new MidiCommandString(midiOut, bytes));
			}
		}

		int axeFxScene = 0;
		int overrideCc = -1;
		if (device == "Axe-Fx" && 
			(patchType.empty() || patchType == "AxeToggle" || patchType == "AxeMomentary") && 
			cmds.empty() && cmds2.empty() &&
			patchDefaultCh != -1)
		{
			int axeCc = 0;
			pElem->QueryIntAttribute("cc", &axeCc);
			if (axeCc)
				overrideCc = axeCc; // no default (like Feedback Return) or overridden from default
			else
				axeCc = ::GetDefaultAxeCc(patchName, mTraceDisplay); // fallback to default

			if (axeCc)
			{
				Bytes bytes;
				if (patchType.empty())
					patchType = "AxeToggle";

				size_t pos = patchName.find("Scene");
				if (std::string::npos == pos)
					pos = patchName.find("scene");
				if (std::string::npos != pos)
				{
					const std::string sceneStr(&patchName.c_str()[pos + 5]);
					if (!sceneStr.empty())
					{
						if (sceneStr.length() > 1 && sceneStr[0] == ' ')
							axeFxScene = ::atoi(&sceneStr.c_str()[1]);
						else
							axeFxScene = ::atoi(sceneStr.c_str());

						if (axeFxScene)
						{
							// there is no axe-fx sync for scenes, so change to unsync'd patch type
							if (patchType == "AxeToggle")
								patchType = "toggle";
							else if (patchType == "AxeMomentary")
								patchType = "momentary";
						}
					}
				}

				// default to A (127)
				bytes.push_back(0xb0 | patchDefaultCh);
				bytes.push_back(axeCc);
				bytes.push_back(axeFxScene ? (axeFxScene - 1) : 127);
				cmds.push_back(new MidiCommandString(midiOut, bytes));
				bytes.clear();

				if (!axeFxScene)
				{
					// default to B (0)
					bytes.push_back(0xb0 | patchDefaultCh);
					bytes.push_back(axeCc);
					bytes.push_back(0);
					cmds2.push_back(new MidiCommandString(midiOut, bytes));
					bytes.clear();
				}

				int invert = 0;
				pElem->QueryIntAttribute("invert", &invert);
				if (1 == invert)
					cmds.swap(cmds2);

				int singleState = 0;
				pElem->QueryIntAttribute("singleState", &singleState);
				if (1 == singleState)
				{
					PatchCommands::iterator it = cmds2.begin();
					if (it != cmds2.end())
						delete *it;
					cmds2.clear();
				}
			}
		}

		bool patchTypeErr = false;
		Patch * newPatch = NULL;
		if (patchType == "normal")
			newPatch = new NormalPatch(patchNumber, patchName, midiOut, cmds, cmds2);
		else if (patchType == "toggle")
			newPatch = new TogglePatch(patchNumber, patchName, midiOut, cmds, cmds2);
		else if (patchType == "AxeToggle")
		{
			AxeTogglePatch * axePatch = new AxeTogglePatch(patchNumber, patchName, midiOut, cmds, cmds2, mAxeFxManager);
			if (mAxeFxManager)
			{
				if (!mAxeFxManager->SetSyncPatch(axePatch, overrideCc))
					axePatch->ClearAxeMgr();
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
			newPatch = new MomentaryPatch(patchNumber, patchName, midiOut, cmds, cmds2);
		else if (patchType == "AxeMomentary")
		{
			newPatch = new MomentaryPatch(patchNumber, patchName, midiOut, cmds, cmds2);
			if (mAxeFxManager)
				mAxeFxManager->SetSyncPatch(newPatch, overrideCc);
			else if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: Axe-Fx device specified but no Axe-Fx input was created" << std::endl << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
		}
		else if (patchType == "sequence")
			newPatch = new SequencePatch(patchNumber, patchName, midiOut, cmds);
		else if (patchType == "patchListSequence")
			newPatch = new PatchListSequencePatch(patchNumber, patchName, intList);
		else if (patchType == "AxeFxTapTempo")
		{
			if (!cmds.size() && !cmds2.size() && -1 != patchDefaultCh)
			{
				const int cc = ::GetDefaultAxeCc("Tap Tempo", mTraceDisplay);
				if (cc)
				{
					Bytes bytes;
					bytes.push_back(0xb0 | patchDefaultCh);
					bytes.push_back(cc);
					bytes.push_back(127);
					cmds.push_back(new MidiCommandString(midiOut, bytes));
				}
			}

			newPatch = new MomentaryPatch(patchNumber, patchName, midiOut, cmds, cmds2);
			if (mAxeFxManager)
				mAxeFxManager->SetTempoPatch(newPatch);
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

		mEngine->AddPatch(newPatch);
		if (mAxeFxManager)
		{
			if (axeFxScene)
				mAxeFxManager->SetScenePatch(axeFxScene, newPatch);
			else
				mAxeFxManager->SetLooperPatch(newPatch);
		}

		ExpressionPedals & pedals = newPatch->GetPedals();
		for (childElem = hRoot.FirstChildElement().Element(); 
			 childElem; 
			 childElem = childElem->NextSiblingElement())
		{
			const std::string patchElement = childElem->ValueStr();
			if (patchElement == "localExpr")
			{
				// <localExpr inputNumber="1" assignmentNumber="1" channel="" controller="" min="" max="" invert="0" enable="" />
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

		PatchBank & bank = mEngine->AddBank(bankNumber, bankName);

		TiXmlHandle hRoot(NULL);
		hRoot = TiXmlHandle(pElem);

		for (TiXmlElement * childElem = hRoot.FirstChildElement().Element(); 
			 childElem; 
			 childElem = childElem->NextSiblingElement())
		{
			if (childElem->ValueStr() == "PatchMap")
			{
				int switchNumber = -1;
				childElem->QueryIntAttribute("switch", &switchNumber);
				int patchNumber = -1;
				childElem->QueryIntAttribute("patch", &patchNumber);
				if (switchNumber <= 0)
				{
					if (mTraceDisplay)
					{
						std::strstream traceMsg;
						traceMsg << "Error loading config file: invalid switch in PatchMap for bank " << bankName << std::endl << std::ends;
						mTraceDisplay->Trace(std::string(traceMsg.str()));
					}
					continue;
				}

				std::string tmp;
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
// 					traceMsg << "Warning load of config file: bank " << bankName << " has a PatchMap with both override and sync attributes (might not work)" << std::endl << std::ends;
// 					mTraceDisplay->Trace(std::string(traceMsg.str()));
// 				}
				bank.AddPatchMapping(switchNumber - 1, patchNumber, swFunc, sfoOp, loadState, unloadState, stateOverride, syncState);
			}
			else if (childElem->ValueStr() == "ExclusiveSwitchGroup")
			{
				std::string switchesStr;
				if (childElem->GetText())
					switchesStr = childElem->GetText();
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
			else if (mTraceDisplay)
			{
				std::strstream traceMsg;
				traceMsg << "Error loading config file: unrecognized element in bank " << bankName << std::endl << std::ends;
				mTraceDisplay->Trace(std::string(traceMsg.str()));
			}
		}
	}
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
	ExpressionControl::SweepCurve curve = ExpressionControl::scLinear;

	childElem->QueryIntAttribute("inputNumber", &exprInputNumber);
	childElem->QueryIntAttribute("assignmentNumber", &assignmentIndex);
	childElem->QueryIntAttribute("channel", &channel);
	if (-1 == channel)
	{
		std::string tmp;
		if (childElem->Attribute("device"))
			tmp = childElem->Attribute("device");
		if (!tmp.empty())
		{
			tmp = mDeviceChannels[tmp];
			if (!tmp.empty())
				channel = ::atoi(tmp.c_str());
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

	if (enable && mAdcEnables[exprInputNumber - 1] == adc_default)
		mAdcEnables[exprInputNumber - 1] = adc_used;
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
		if (pElem->ValueStr() != "Device")
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

		if (dev == "AxeFx" || 
			dev == "Axe-Fx" || 
			dev == "AxeFx Ultra" ||
			dev == "Axe-Fx Ultra" ||
			dev == "AxeFx Std" ||
			dev == "AxeFx Standard" ||
			dev == "Axe-Fx Standard" ||
			dev == "AxeFx II" ||
			dev == "Axe-Fx II" ||
			dev == "AxeFx 2" ||
			dev == "Axe-Fx 2"
			)
		{
			AxeFxModel axeModel(AxeStd);
			int tmp = -1;
			pElem->QueryIntAttribute("model", &tmp);
			if (2 == tmp)
				axeModel = Axe2;
			else
			{
				int pos = dev.find("2");
				if (std::string::npos != pos)
					axeModel = Axe2;
				else
				{
					pos = dev.find("II");
					if (std::string::npos != pos)
						axeModel = Axe2;
				}
			}

			if (!mAxeFxManager)
			{
				const int axeCh = ::atoi(ch.c_str()) - 1;
				mAxeFxManager = new AxeFxManager(mMainDisplay, mSwitchDisplay, mTraceDisplay, mApp->ApplicationDirectory(), axeCh, axeModel);
				mAxeFxManager->AddRef();
				mAxeSyncPort = -1 == port ? 1 : port;
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
