/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2013,2015,2018,2020-2022 Sean Echevarria
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

#include <algorithm>
#include <strstream>
#include <atomic>
#include "MidiControlEngine.h"
#include "PatchBank.h"
#include "IMainDisplay.h"
#include "ISwitchDisplay.h"
#include "ITraceDisplay.h"
#include "IMidiOutGenerator.h"
#include "MetaPatch_BankNav.h"
#include "SleepCommand.h"
#include "ITrollApplication.h"
#include "HexStringUtils.h"
#ifdef _WINDOWS
	#include <windows.h>
	#define CurTime	GetTickCount // time in ms (used to measure elapsed time between events, origin doesn't matter)
#else
	#define CurTime	??
#endif // _WINDOWS
#include "MetaPatch_LoadBank.h"
#include "MetaPatch_BankHistory.h"
#include "MetaPatch_ResetBankPatches.h"
#include "MetaPatch_SyncAxeFx.h"
#include "MetaPatch_AxeFxNav.h"


#ifdef _MSC_VER
#pragma warning(disable:4482)
#endif

#ifdef ITEM_COUNTING
std::atomic<int> gMidiControlEngCnt = 0;
#endif


static bool
SortByBankNumber(const PatchBankPtr lhs, const PatchBankPtr rhs)
{
	return lhs->GetBankNumber() < rhs->GetBankNumber();
}


MidiControlEngine::MidiControlEngine(ITrollApplication * app,
									 IMainDisplay * mainDisplay, 
									 ISwitchDisplay * switchDisplay, 
									 ITraceDisplay * traceDisplay,
									 IMidiOutGenerator * midiOutGenerator,
									 IMidiOutPtr midiOut,
									 IAxeFxPtr axMgr,
									 IAxeFxPtr ax3Mgr,
									 EdpManagerPtr edpMgr,
									 int incrementSwitchNumber,
									 int decrementSwitchNumber,
									 int modeSwitchNumber) :
	mApplication(app),
	mMainDisplay(mainDisplay),
	mSwitchDisplay(switchDisplay),
	mTrace(traceDisplay),
	mActiveBank(nullptr),
	mActiveBankIndex(0),
	mBankNavigationIndex(0),
	mPowerUpBank(0),
	mIncrementSwitchNumber(incrementSwitchNumber),
	mDecrementSwitchNumber(decrementSwitchNumber),
	mModeSwitchNumber(modeSwitchNumber),
	mFilterRedundantProgramChanges(false),
	mPedalModePort(0),
	mHistoryNavMode(hmNone),
	mDirectChangeChannel(0),
	mDirectValueLastSent(0),
	mDirectValue1LastSent(0),
	mMidiOutGenerator(midiOutGenerator),
	mMidiOut(midiOut),
	mSwitchPressedEventTime(0),
	mEdpMgr(edpMgr)
{
#ifdef ITEM_COUNTING
	++gMidiControlEngCnt;
#endif

	mMode.push(emCreated);

	if (axMgr)
		mAxeMgrs.push_back(axMgr);
	if (ax3Mgr)
		mAxeMgrs.push_back(ax3Mgr);

	mBanks.reserve(999);
}

MidiControlEngine::~MidiControlEngine()
{
	Shutdown();

#ifdef ITEM_COUNTING
	--gMidiControlEngCnt;
#endif
}

PatchBankPtr
MidiControlEngine::AddBank(int number,
						   const std::string & name)
{
	if (mTrace)
	{
		for (const PatchBankPtr& curItem : mBanks)
		{
			if (curItem && curItem->GetBankNumber() == number)
			{
				std::strstream traceMsg;
				traceMsg << "Warning: multiple banks with bank number " << number << '\n' << std::ends;
				mTrace->Trace(std::string(traceMsg.str()));
			}
		}
	}

	PatchBankPtr pBank = std::make_shared<PatchBank>(number, name);
	mBanks.push_back(pBank);
	return pBank;
}

void
MidiControlEngine::AddPatch(PatchPtr patch)
{
	const int patchNum = patch->GetNumber();
	PatchPtr prev = mPatches[patchNum];
	if (prev && mTrace)
	{
		std::strstream traceMsg;
		traceMsg << "ERROR: multiple patches with patch number " << patchNum << '\n' << std::ends;
		mTrace->Trace(std::string(traceMsg.str()));
	}
	mPatches[patchNum] = patch;
}

void
MidiControlEngine::SetPowerup(int powerupBank)
{
	mPowerUpBank = powerupBank;
}

const std::string
MidiControlEngine::GetBankNameByNum(int bankNumberNotIndex)
{
	const int bankIdx = GetBankIndex(bankNumberNotIndex);
	PatchBankPtr bnk = GetBank(bankIdx);
	if (bnk)
		return bnk->GetBankName();
	return std::string();
}

int
MidiControlEngine::GetBankNumber(const std::string& name) const
{
	for (const std::shared_ptr<PatchBank>& curItem : mBanks)
	{
		if (curItem->GetBankName() == name)
			return curItem->GetBankNumber();
	}
	return -1;
}

void
MidiControlEngine::CompleteInit(const PedalCalibration * pedalCalibrationSettings, unsigned int ledColor)
{
	mEngineLedColor = ledColor;

	if (mOtherModeSwitchNumbers.empty())
	{
		// set defaults
		mOtherModeSwitchNumbers[kModeRecall] = kModeRecall;
		mOtherModeSwitchNumbers[kModeBack] = kModeBack;
		mOtherModeSwitchNumbers[kModeForward] = kModeForward;
		mOtherModeSwitchNumbers[kModeTime] = kModeTime;
		mOtherModeSwitchNumbers[kModeProgramChangeDirect] = kModeProgramChangeDirect;
		mOtherModeSwitchNumbers[kModeControlChangeDirect] = kModeControlChangeDirect;
		mOtherModeSwitchNumbers[kModeBankDesc] = kModeBankDesc;
		mOtherModeSwitchNumbers[kModeBankDirect] = kModeBankDirect;
		mOtherModeSwitchNumbers[kModeExprPedalDisplay] = kModeExprPedalDisplay;
		mOtherModeSwitchNumbers[kModeAdcOverride] = kModeAdcOverride;
		mOtherModeSwitchNumbers[kModeTestLeds] = kModeTestLeds;
		mOtherModeSwitchNumbers[kModeToggleTraceWindow] = kModeToggleTraceWindow;
		mOtherModeSwitchNumbers[kModeClockSetup] = kModeClockSetup;
		mOtherModeSwitchNumbers[kModeMidiOutSelect] = kModeMidiOutSelect;
	}

	std::sort(mBanks.begin(), mBanks.end(), SortByBankNumber);

	AddPatch(std::make_shared<MetaPatch_BankNavNext>(this, ReservedPatchNumbers::kBankNavNext, "Nav next"));
	AddPatch(std::make_shared<MetaPatch_BankNavPrevious>(this, ReservedPatchNumbers::kBankNavPrev, "Nav previous"));
	AddPatch(std::make_shared<MetaPatch_LoadNextBank>(this, ReservedPatchNumbers::kLoadNextBank, "[next]"));
	AddPatch(std::make_shared<MetaPatch_LoadPreviousBank>(this, ReservedPatchNumbers::kLoadPrevBank, "[previous]"));
	AddPatch(std::make_shared<MetaPatch_BankHistoryBackward>(this, ReservedPatchNumbers::kBankHistoryBackward, "[back]"));
	AddPatch(std::make_shared<MetaPatch_BankHistoryForward>(this, ReservedPatchNumbers::kBankHistoryForward, "[forward]"));
	AddPatch(std::make_shared<MetaPatch_BankHistoryRecall>(this, ReservedPatchNumbers::kBankHistoryRecall, "[recall]"));
	AddPatch(std::make_shared<MetaPatch_ResetBankPatches>(this, ReservedPatchNumbers::kResetBankPatches, "Reset bank patches"));
	{
		auto metPatch = std::make_shared<MetaPatch_SyncAxeFx>(ReservedPatchNumbers::kSyncAxeFx, "Sync Axe-FX");
		metPatch->AddAxeManagers(mAxeMgrs);
		AddPatch(metPatch);
	}
	{
		auto metPatch = std::make_shared<MetaPatch_AxeFxNextPreset>(ReservedPatchNumbers::kAxeFx3NextPreset, "AxeFx Next Preset");
		metPatch->AddAxeManagers(mAxeMgrs);
		AddPatch(metPatch);
	}
	{
		auto metPatch = std::make_shared<MetaPatch_AxeFxPrevPreset>(ReservedPatchNumbers::kAxeFx3PrevPreset, "AxeFx Prev Preset");
		metPatch->AddAxeManagers(mAxeMgrs);
		AddPatch(metPatch);
	}
	{
		auto metPatch = std::make_shared<MetaPatch_AxeFxNextScene>(ReservedPatchNumbers::kAxeFx3NextScene, "AxeFx Next Scene");
		metPatch->AddAxeManagers(mAxeMgrs);
		AddPatch(metPatch);
	}
	{
		auto metPatch = std::make_shared<MetaPatch_AxeFxPrevScene>(ReservedPatchNumbers::kAxeFx3PrevScene, "AxeFx Prev Scene");
		metPatch->AddAxeManagers(mAxeMgrs);
		AddPatch(metPatch);
	}

	for (Patches::iterator it = mPatches.begin();
		it != mPatches.end();
		)
	{
		PatchPtr curItem = (*it).second;
		if (curItem)
		{
			curItem->CompleteInit(this, mTrace);
			++it;
		}
		else
			mPatches.erase(it++);
	}

	PatchBankPtr defaultsBank;
	if (mBanks.begin() != mBanks.end())
	{
		defaultsBank = *mBanks.begin();
		// bank number 0 is used as the bank with the default mappings
		if (0 == defaultsBank->GetBankNumber())
			defaultsBank->InitPatches(mPatches, mTrace); // init before calling SetDefaultMapping
		else
			defaultsBank = nullptr;
	}

	PatchBankPtr tmpDefaultBank = std::make_shared<PatchBank>(0, "nav default");

	// this is how we allow users to override Next and Prev switches in their banks
	// while at the same time reserving them for use by our modes.
	// If their default bank specifies a mapping for mIncrementSwitchNumber or 
	// mDecrementSwitchNumber, they will not get Next or Prev at all.
	// CHANGED: no longer add Next and prev by default
// 	tmpDefaultBank.AddSwitchAssignment(mIncrementSwitchNumber, ReservedPatchNumbers::kBankNavNext, PatchBank::stIgnore, PatchBank::stIgnore);
// 	tmpDefaultBank.AddSwitchAssignment(mDecrementSwitchNumber, ReservedPatchNumbers::kBankNavPrev, PatchBank::stIgnore, PatchBank::stIgnore);

	if (defaultsBank)
		defaultsBank->SetDefaultMappings(tmpDefaultBank);  // add Next and Prev to their default bank
	else
		defaultsBank = tmpDefaultBank; // no default bank specified, use our manufactured one

	int itIdx = 0;
	for (Banks::iterator it = mBanks.begin();
		it != mBanks.end();
		++it, ++itIdx)
	{
		PatchBankPtr curItem = *it;
		curItem->InitPatches(mPatches, mTrace);
		curItem->SetDefaultMappings(defaultsBank);
	}

	for (const auto &it : mBankLoadSwitchBankNamesForInit)
	{
		int bankNum = GetBankNumber(it.second);
		if (-1 == bankNum)
		{
			if (mTrace)
			{
				std::strstream traceMsg;
				traceMsg << "Error: failed to locate bank referenced by LoadBank command; bank name " << it.second << '\n' << std::ends;
				mTrace->Trace(std::string(traceMsg.str()));
			}
			continue;
		}

		mBankLoadSwitchNumbers[it.first] = bankNum;
	}
	mBankLoadSwitchBankNamesForInit.clear();

	for (const auto &mBankLoadSwitchNumber : mBankLoadSwitchNumbers)
	{
		PatchBankPtr bnk = GetBank(GetBankIndex(mBankLoadSwitchNumber.second));
		if (!bnk)
		{
			if (mTrace)
			{
				std::strstream traceMsg;
				traceMsg << "Error: failed to identify name of bank referenced by LoadBank command; bank number " << mBankLoadSwitchNumber.second << '\n' << std::ends;
				mTrace->Trace(std::string(traceMsg.str()));
			}
		}
	}

	CalibrateExprSettings(pedalCalibrationSettings);
	ChangeMode(emBank);
	
	if (mTrace)
	{
		std::strstream traceMsg;
		int userDefinedPatchCnt = 0;
		std::for_each(mPatches.begin(), mPatches.end(), 
			[&userDefinedPatchCnt](const std::pair<int, PatchPtr> & pr)
		{
			if (pr.second && pr.second->GetNumber() >= 0)
				++userDefinedPatchCnt;
		});
		traceMsg << "Loaded " << mBanks.size() << " banks, " << userDefinedPatchCnt << " patches" << '\n' << std::ends;
		mTrace->Trace(std::string(traceMsg.str()));
	}

	LoadStartupBank();

	// init pedals on the wire
	for (int idx = 0; idx < ExpressionPedals::PedalCount; ++idx)
		RefirePedal(idx);
}

void
MidiControlEngine::Shutdown()
{
	gActivePatchPedals = nullptr;
	for (const auto& mgr : mAxeMgrs)
		mgr->Shutdown();
	mAxeMgrs.clear();
}

void
MidiControlEngine::CalibrateExprSettings(const PedalCalibration * pedalCalibrationSettings)
{
	mGlobalPedals.Calibrate(pedalCalibrationSettings, this, mTrace);

	for (const PatchBankPtr& curItem : mBanks)
		curItem->CalibrateExprSettings(pedalCalibrationSettings, this, mTrace);
}

void
MidiControlEngine::LoadStartupBank()
{
	int powerUpBankIndex = -1;
	int itIdx = 0;
	for (Banks::iterator it = mBanks.begin();
		it != mBanks.end();
		++it, ++itIdx)
	{
		PatchBankPtr curItem = *it;
		if (curItem->GetBankNumber() == mPowerUpBank)
		{
			powerUpBankIndex = itIdx;
			break;
		}
	}

	if (powerUpBankIndex == -1)
		powerUpBankIndex = 0;

	LoadBank(powerUpBankIndex);
}

void
MidiControlEngine::SwitchPressed(int switchNumber)
{
	if (false && mTrace)
	{
		std::strstream msg;
		msg << "SwitchPressed: " << switchNumber << '\n' << std::ends;
		mTrace->Trace(std::string(msg.str()));
	}

	mSwitchPressedEventTime = CurTime();
	const EngineMode curMode = CurrentMode();
	if (emBank == curMode)
	{
		if (switchNumber == mModeSwitchNumber)
			return;

		if (mActiveBank)
			mActiveBank->PatchSwitchPressed(switchNumber, mMainDisplay, mSwitchDisplay);
		return;
	}

	if (emBankDirect == curMode || 
		emProgramChangeDirect == curMode ||
		emControlChangeDirect == curMode ||
		emClockSetup == curMode)
	{
		bool doUpdate = false;
		if ((switchNumber >= 0 && switchNumber <= 9) ||
			(switchNumber == mDecrementSwitchNumber 
#ifndef DECREMENT_IS_BACKSPACE // emBankDirect always has decrement as Backspace
				&& emBankDirect == curMode
#endif
				) ||
			switchNumber == mIncrementSwitchNumber)
			doUpdate = true;

		if ((emProgramChangeDirect == curMode || emControlChangeDirect == curMode || emClockSetup == curMode) && 
			switchNumber >= 10 && switchNumber <= 14)
			doUpdate = true;

		if (mSwitchDisplay && doUpdate)
		{
			mSwitchDisplay->EnableDisplayUpdate(false);
			mSwitchDisplay->ForceSwitchDisplay(switchNumber, mEngineLedColor);
		}

		if (emProgramChangeDirect == curMode)
			SwitchPressed_ProgramChangeDirect(switchNumber);
		else if (emControlChangeDirect == curMode)
			SwitchPressed_ControlChangeDirect(switchNumber);
		else if (emClockSetup == curMode)
			SwitchPressed_ClockSetup(switchNumber);

		return;
	}

	// no other mode responds to SwitchPressed
}

void
MidiControlEngine::SwitchReleased(int switchNumber)
{
	if (false && mTrace)
	{
		std::strstream msg;
		msg << "SwitchReleased: " << switchNumber << '\n' << std::ends;
		mTrace->Trace(msg.str());
	}

	PatchBank::SwitchPressDuration dur;
	const int kLongPressMinDuration = 300; // milliseconds
	const int kExtendedPressMinDuration = 2000; // milliseconds
	const unsigned int kDuration = CurTime() - mSwitchPressedEventTime;
	if (kDuration > kLongPressMinDuration)
	{
		if (kDuration > kExtendedPressMinDuration)
			dur = PatchBank::spdExtended;
		else
			dur = PatchBank::spdLong;
	}
	else
		dur = PatchBank::spdShort;

	const EngineMode curMode = CurrentMode();
	switch (curMode)
	{
	case emBank:
		// default mode
		if (switchNumber == mModeSwitchNumber)
		{
			if (PatchBank::spdShort != dur)
			{
				// #consider: long-press function of mode switch could be user-definable
				HistoryBackward();
			}
			else
				ChangeMode(emModeSelect);
			return;
		}

		if (mActiveBank)
			mActiveBank->PatchSwitchReleased(switchNumber, mMainDisplay, mSwitchDisplay, dur);
		return;

	case emCreated:
		return;

	case emBankNav:
	case emBankDesc:
		SwitchReleased_NavAndDescMode(switchNumber);
		return;

	case emExprPedalDisplay:
		SwitchReleased_PedalDisplayMode(switchNumber);
		return;

	case emModeSelect:
		SwitchReleased_ModeSelect(switchNumber);
		return;

	case emAdcOverride:
		SwitchReleased_AdcOverrideMode(switchNumber);
		return;

	case emProgramChangeDirect:
		SwitchReleased_ProgramChangeDirect(switchNumber);
		return;

	case emControlChangeDirect:
		SwitchReleased_ControlChangeDirect(switchNumber);
		return;

	case emBankDirect:
		SwitchReleased_BankDirect(switchNumber);
		return;

	case emTimeDisplay:
		SwitchReleased_TimeDisplay(switchNumber);
		return;

	case emLedTests:
		SwitchReleased_LedTests(switchNumber);
		return;

	case emClockSetup:
		SwitchReleased_ClockSetup(switchNumber);
		return;

	case emMidiOutSelect:
		SwitchReleased_MidiOutSelect(switchNumber);
	}
}

void
MidiControlEngine::AdcValueChanged(int port, 
								   int newValue)
{
	_ASSERTE(port < ExpressionPedals::PedalCount);
	const EngineMode curMode = CurrentMode();
	switch (curMode)
	{
	case emExprPedalDisplay:
		if (mMainDisplay && mPedalModePort == port)
		{
			std::strstream displayMsg;
			displayMsg << "ADC port " << (int) (port+1) << " value: " << newValue << '\n' << std::ends;
			mMainDisplay->TextOut(displayMsg.str());
		}
		return;

	case emBank:
	case emProgramChangeDirect:
	case emControlChangeDirect:
	case emModeSelect:
	case emTimeDisplay:
	case emLedTests:
	case emClockSetup:
	case emMidiOutSelect:
		// forward directly to active patch
		if (!gActivePatchPedals || 
			gActivePatchPedals->AdcValueChange(mMainDisplay, port, newValue))
		{
			// process globals if no rejection
			mGlobalPedals.AdcValueChange(mMainDisplay, port, newValue);
		}
		break;
	}
}

void
MidiControlEngine::RefirePedal(int pedal)
{
	// pedal is really the adcPort
	_ASSERTE(pedal < ExpressionPedals::PedalCount);
	// forward directly to active patch
	if (!gActivePatchPedals || 
		gActivePatchPedals->Refire(mMainDisplay, pedal))
	{
		// process globals if no rejection
		mGlobalPedals.Refire(mMainDisplay, pedal);
	}
}

void
MidiControlEngine::ResetBankPatches()
{
	if (mActiveBank)
		mActiveBank->ResetPatches(mMainDisplay, mSwitchDisplay);
}

void
MidiControlEngine::ResetExclusiveGroup(int activeSwitch)
{
	if (mActiveBank)
		mActiveBank->ResetExclusiveGroup(mSwitchDisplay, activeSwitch);
}

bool
MidiControlEngine::NavigateBankRelative(int relativeBankIndex)
{
	// bank inc/dec does not commit bank
	const int kBankCnt = mBanks.size();
	if (!kBankCnt || kBankCnt == 1)
		return false;

	mBankNavigationIndex = mBankNavigationIndex + relativeBankIndex;
	if (mBankNavigationIndex < 0)
		mBankNavigationIndex = kBankCnt - 1;
	if (mBankNavigationIndex >= kBankCnt)
		mBankNavigationIndex = 0;

	if (mSwitchDisplay)
	{
		mSwitchDisplay->EnableDisplayUpdate(false);
		for (int idx = 0; idx < 64; idx++)
		{
			if (idx != mModeSwitchNumber)
			{
				mSwitchDisplay->ClearSwitchText(idx);
				mSwitchDisplay->ForceSwitchDisplay(idx, 0);
			}
		}
	}

	const EngineMode curMode = CurrentMode();
	if (emBank == curMode && mSwitchDisplay)
		mSwitchDisplay->EnableDisplayUpdate(true);

	// display bank info
	PatchBankPtr bank = GetBank(mBankNavigationIndex);
	if (!bank)
		return false;

	bank->DisplayInfo(mMainDisplay, mSwitchDisplay, true, relativeBankIndex != 0);
	return true;
}

void
MidiControlEngine::LoadBankRelative(int relativeBankIndex)
{
	// navigate and commit bank
	const int kBankCnt = mBanks.size();
	if (!kBankCnt || kBankCnt == 1)
		return;

	mBankNavigationIndex = mBankNavigationIndex + relativeBankIndex;
	if (mBankNavigationIndex < 0)
		mBankNavigationIndex = kBankCnt - 1;
	if (mBankNavigationIndex >= kBankCnt)
		mBankNavigationIndex = 0;

	ChangeMode(emBank);
	LoadBank(mBankNavigationIndex);

	PatchBankPtr bank = GetBank(mBankNavigationIndex);
	if (!bank)
	{
		if (mTrace)
		{
			std::strstream traceMsg;
			traceMsg << "Bank navigation error\n" << std::ends;
			mTrace->Trace(std::string(traceMsg.str()));
		}
		return;
	}

	bank->DisplayInfo(mMainDisplay, mSwitchDisplay, true, false);
}

void
MidiControlEngine::LoadBankByNumber(int bankNumber)
{
	int bankidx = GetBankIndex(bankNumber);
	if (-1 == bankidx)
	{
		if (mTrace)
		{
			std::strstream traceMsg;
			traceMsg << "Bank " << bankNumber << " does not exist\n" << std::ends;
			mTrace->Trace(std::string(traceMsg.str()));
		}
		return;
	}

	ChangeMode(emBank);
	LoadBank(bankidx);

	PatchBankPtr bank = GetBank(bankidx);
	if (!bank)
	{
		if (mTrace)
		{
			std::strstream traceMsg;
			traceMsg << "Bank " << bankNumber << " does not exist\n" << std::ends;
			mTrace->Trace(std::string(traceMsg.str()));
		}
		return;
	}

	bank->DisplayInfo(mMainDisplay, mSwitchDisplay, true, false);
}

int
MidiControlEngine::GetBankIndex(int bankNumber)
{
	int idx = 0;
	for (Banks::iterator it = mBanks.begin();
		it != mBanks.end();
		++it, ++idx)
	{
		PatchBankPtr curItem = *it;
		if (curItem->GetBankNumber() == bankNumber)
			return idx;
	}
	return -1;
}

PatchBankPtr
MidiControlEngine::GetBank(int bankIndex)
{
	const int kBankCnt = mBanks.size();
	if (bankIndex < 0 || bankIndex >= kBankCnt)
		return nullptr;

	PatchBankPtr bank = mBanks[bankIndex];
	return bank;
}

bool
MidiControlEngine::LoadBank(int bankIndex)
{
	PatchBankPtr bank = GetBank(bankIndex);
	if (!bank)
		return false;

	if (mActiveBank)
		mActiveBank->Unload(mMainDisplay, mSwitchDisplay);

	mActiveBank = bank;

	if (hmWentBack == mHistoryNavMode || 
		hmWentForward == mHistoryNavMode)
	{
		// leave recall mode
		mHistoryNavMode = hmNone;
	}

	if (hmNone == mHistoryNavMode)
	{
		// record bank load for Back
		if (mBackHistory.empty() || mBackHistory.top() != mActiveBankIndex)
		{
			mBackHistory.push(mActiveBankIndex);

			// invalidate Forward hist
			while (!mForwardHistory.empty())
				mForwardHistory.pop();
		}
	}

	mBankNavigationIndex = mActiveBankIndex = bankIndex;
	mActiveBank->Load(mMainDisplay, mSwitchDisplay);
	UpdateBankModeSwitchDisplay();
	
	return true;
}

void
MidiControlEngine::HistoryBackward()
{
	if (mBackHistory.empty())
		return;

	const int bankIdx = mBackHistory.top();
	mBackHistory.pop();
	mForwardHistory.push(mActiveBankIndex);

	mHistoryNavMode = hmBack;
	LoadBank(bankIdx);
	mHistoryNavMode = hmWentBack;

	PatchBankPtr bank = GetBank(bankIdx);
	if (!bank)
		return;

	bank->DisplayInfo(mMainDisplay, mSwitchDisplay, true, false);
}

void
MidiControlEngine::HistoryForward()
{
	if (mForwardHistory.empty())
		return;
	
	const int bankIdx = mForwardHistory.top();
	mForwardHistory.pop();
	mBackHistory.push(mActiveBankIndex);

	mHistoryNavMode = hmForward;
	LoadBank(bankIdx);
	mHistoryNavMode = hmWentForward;

	PatchBankPtr bank = GetBank(bankIdx);
	if (!bank)
		return;

	bank->DisplayInfo(mMainDisplay, mSwitchDisplay, true, false);
}

void
MidiControlEngine::HistoryRecall()
{
	switch (mHistoryNavMode)
	{
	case hmNone:
	case hmWentForward:
		HistoryBackward();
		break;
	case hmWentBack:
		HistoryForward();
	    break;
	case hmBack:
	case hmForward:
		_ASSERTE(!"invalid history nav mode");
		break;
	}
}

ControllerInputMonitorPtr
MidiControlEngine::GetControllerInputMonitor(int inputDevicePort)
{
	return mInputMonitors[inputDevicePort];
}

void
MidiControlEngine::AddControllerInputMonitor(int inputDevicePort, ControllerInputMonitorPtr mon)
{
	mInputMonitors[inputDevicePort] = mon;
}

// possible mode transitions:
// emCreated -> emDefault
// emDefault -> emBankNav
// emDefault -> emModeSelect
// emModeSelect -> emDefault
// emModeSelect -> emBankNav
// emModeSelect -> emBankDesc
// emModeSelect -> emBankDirect
// emModeSelect -> emProgramChangeDirect
// emModeSelect -> emControlChangeDirect
// emModeSelect -> emExprPedalDisplay
// emModeSelect -> emClockSetup
// emModeSelect -> emMidiOutSelect
// emBankNav -> emDefault
// emBankDesc -> emDefault
// emBankDirect -> emDefault
// emProgramChangeDirect -> emDefault
// emControlChangeDirect -> emDefault
// emExprPedalDisplay -> emDefault
// emClockSetup -> emDefault
// emMidiOutSelect -> (emDefault or pop back to emProgramChangeDirect/emControlChangeDirect/emClockSetup)
void
MidiControlEngine::ChangeMode(EngineMode newMode)
{
	mMode.top() = newMode;
	const EngineMode curMode = CurrentMode();
	_ASSERTE(curMode == newMode);

	if (mSwitchDisplay)
	{
		mSwitchDisplay->EnableDisplayUpdate(false);
		for (int idx = 0; idx < 64; idx++)
		{
			mSwitchDisplay->ClearSwitchText(idx);
			mSwitchDisplay->ForceSwitchDisplay(idx, 0);
		}
	}

	bool showModeInMainDisplay = true;
	std::string msg;

	switch (curMode)
	{
	case emBank:
		msg = "Bank";
		if (mSwitchDisplay)
			mSwitchDisplay->EnableDisplayUpdate(true);
		if (mActiveBank)
		{
			// caller changing to emBank will update mainDisplay - reduce flicker
			showModeInMainDisplay = false;
			UpdateBankModeSwitchDisplay();
			if (mSwitchDisplay)
				msg.clear();
		}
		break;
	case emBankNav:
		msg = "Bank Navigation";
		if (mActiveBank)
			showModeInMainDisplay = false;
		if (mSwitchDisplay)
		{
			// override possible user override in this mode
			mSwitchDisplay->SetSwitchText(mIncrementSwitchNumber, "Next Bank");
			mSwitchDisplay->SetSwitchText(mDecrementSwitchNumber, "Prev Bank");
		}
		break;
	case emBankDesc:
		msg = "Bank and Switch Description";
		if (mSwitchDisplay)
		{
			// override possible user override in this mode
			mSwitchDisplay->SetSwitchText(mIncrementSwitchNumber, "Next Bank");
			mSwitchDisplay->SetSwitchText(mDecrementSwitchNumber, "Prev Bank");
		}
		break;
	case emModeSelect:
		msg = "Menu";
		if (mSwitchDisplay)
		{
			// override possible user override in this mode
			mSwitchDisplay->SetSwitchText(mIncrementSwitchNumber, "Next Bank");
			mSwitchDisplay->SetSwitchText(mDecrementSwitchNumber, "Prev Bank");

			SetupModeSelectSwitch(kModeRecall);
			if (!mBackHistory.empty())
				SetupModeSelectSwitch(kModeBack);
			if (!mForwardHistory.empty())
				SetupModeSelectSwitch(kModeForward);
			SetupModeSelectSwitch(kModeBankDesc);
			SetupModeSelectSwitch(kModeBankDirect);
			SetupModeSelectSwitch(kModeProgramChangeDirect);
			SetupModeSelectSwitch(kModeControlChangeDirect);
			SetupModeSelectSwitch(kModeExprPedalDisplay);
			SetupModeSelectSwitch(kModeTestLeds);
			SetupModeSelectSwitch(kModeToggleTraceWindow);
			SetupModeSelectSwitch(kModeAdcOverride);
			SetupModeSelectSwitch(kModeTime);
			SetupModeSelectSwitch(kModeClockSetup);
			SetupModeSelectSwitch(kModeMidiOutSelect);

			mSwitchDisplay->EnableDisplayUpdate(false);
			for (std::map<int, int>::const_iterator it = mBankLoadSwitchNumbers.begin();
				it != mBankLoadSwitchNumbers.end(); ++it)
			{
				PatchBankPtr bnk = GetBank(GetBankIndex(it->second));
				if (bnk)
				{
					std::string txt("[" + bnk->GetBankName() + "]");
					mSwitchDisplay->SetSwitchText(it->first, txt);
					mSwitchDisplay->ForceSwitchDisplay(it->first, mEngineLedColor);
				}
			}
		}
		break;
	case emTimeDisplay:
		{
			msg = "Time Display";
			showModeInMainDisplay = false;
			int switchNumber = GetSwitchNumber(kModeTime);
			if (kUnassignedSwitchNumber != switchNumber)
			{
				mSwitchDisplay->EnableDisplayUpdate(false);

				mSwitchDisplay->SetSwitchText(switchNumber, "Exit Time Display");
				mSwitchDisplay->ForceSwitchDisplay(switchNumber, mEngineLedColor);
				mSwitchDisplay->SetSwitchText(switchNumber + 1, "Pause/Resume time");
				mSwitchDisplay->ForceSwitchDisplay(switchNumber + 1, mEngineLedColor);
				mSwitchDisplay->SetSwitchText(switchNumber + 2, "Reset elapsed time");
				mSwitchDisplay->ForceSwitchDisplay(switchNumber + 2, mEngineLedColor);
				mSwitchDisplay->SetSwitchText(switchNumber + 3, "Exit mTroll");
				mSwitchDisplay->ForceSwitchDisplay(switchNumber + 3, mEngineLedColor);
				mSwitchDisplay->SetSwitchText(switchNumber + 4, "Exit + sleep");
				mSwitchDisplay->ForceSwitchDisplay(switchNumber + 4, mEngineLedColor);
				mSwitchDisplay->SetSwitchText(switchNumber + 5, "Exit + hibernate");
				mSwitchDisplay->ForceSwitchDisplay(switchNumber + 5, mEngineLedColor);
			}
			if (!mApplication || !mApplication->EnableTimeDisplay(true))
				EscapeToDefaultMode();
		}
		break;
	case emLedTests:
		{
			msg = "LED Tests";
			mSwitchDisplay->EnableDisplayUpdate(false);

			int switchNumber = GetSwitchNumber(kModeTestLeds);
			if (kUnassignedSwitchNumber != switchNumber)
			{
				mSwitchDisplay->SetSwitchText(switchNumber, "Exit LED Tests");
				mSwitchDisplay->ForceSwitchDisplay(switchNumber, mEngineLedColor);
			}

			switchNumber = 0;
			mSwitchDisplay->SetSwitchText(switchNumber, "Exit LED Tests");
			mSwitchDisplay->ForceSwitchDisplay(switchNumber, mEngineLedColor);
			mSwitchDisplay->SetSwitchText(switchNumber + 1, "mTroll Quick Blue test");
			mSwitchDisplay->ForceSwitchDisplay(switchNumber + 1, mEngineLedColor);
			mSwitchDisplay->SetSwitchText(switchNumber + 2, "Color presets");
			mSwitchDisplay->ForceSwitchDisplay(switchNumber + 2, mEngineLedColor);
			mSwitchDisplay->SetSwitchText(switchNumber + 3, "monome RGB rows");
			mSwitchDisplay->ForceSwitchDisplay(switchNumber + 3, mEngineLedColor);
			mSwitchDisplay->SetSwitchText(switchNumber + 4, "monome RGB rows and columns");
			mSwitchDisplay->ForceSwitchDisplay(switchNumber + 4, mEngineLedColor);
		}
		break;
	case emMidiOutSelect:
		{
			msg = "Select MIDI Out Port";
			mSwitchDisplay->EnableDisplayUpdate(false);

			// iterate over midi out devices
			if (mMidiOutGenerator)
			{
				for (int idx = 0; idx < 15; ++idx)
				{
					IMidiOutPtr m = mMidiOutGenerator->GetMidiOut(idx);
					if (m && m->IsMidiOutOpen())
					{
						std::string curName(m->GetMidiOutDeviceName());
						mSwitchDisplay->SetSwitchText(idx, curName);
						mSwitchDisplay->ForceSwitchDisplay(idx, mMidiOut && mMidiOut == m ? kFirstColorPreset + 1 : mEngineLedColor);
					}
				}
			}
		}
		break;
	case emExprPedalDisplay:
		msg = "Raw ADC values";
		mPedalModePort = 0;
		if (mSwitchDisplay)
		{
			mSwitchDisplay->SetSwitchText(0, "Pedal 1");
			mSwitchDisplay->EnableDisplayUpdate(false);
			mSwitchDisplay->ForceSwitchDisplay(0, mEngineLedColor);
			mSwitchDisplay->SetSwitchText(1, "Pedal 2");
			mSwitchDisplay->SetSwitchText(2, "Pedal 3");
			mSwitchDisplay->SetSwitchText(3, "Pedal 4");
		}
		break;
	case emAdcOverride:
		msg = "Override ADCs";
		mPedalModePort = 0;
		if (mSwitchDisplay && mApplication)
		{
			mSwitchDisplay->EnableDisplayUpdate(false);
			for (int idx = 0; idx < 4; ++idx)
			{
				std::strstream msg2;
				msg2 << "ADC " << idx;
				if (mApplication->IsAdcOverridden(idx))
				{
					msg2 << " forced off" << std::ends;
					mSwitchDisplay->SetSwitchText(idx, msg2.str());
					mSwitchDisplay->ForceSwitchDisplay(idx, mEngineLedColor);
				}
				else
				{
					msg2 << " normal" << std::ends;
					mSwitchDisplay->SetSwitchText(idx, msg2.str());
					mSwitchDisplay->ForceSwitchDisplay(idx, 0);
				}
			}
		}
		break;
	case emProgramChangeDirect:
	case emControlChangeDirect:
	case emClockSetup:
		if (!mMidiOut)
		{
			if (mMainDisplay)
				mMainDisplay->TextOut("Program change, control change, and MIDI clock not available without MIDI out");
			break;
		}
		// fall-through
	case emBankDirect:
		mDirectNumber.clear();
		if (emBankDirect == curMode)
			msg = "Bank Direct";
		else if (emProgramChangeDirect == curMode)
			msg = "Manual Program Change";
		else if (emControlChangeDirect == curMode)
			msg = "Manual Control Change";
		else if (emClockSetup == curMode)
			msg = "MIDI beat clock";

		if (mSwitchDisplay)
		{
			mSwitchDisplay->SetSwitchText(0, "1");
			mSwitchDisplay->ForceSwitchDisplay(0, mEngineLedColor);
			mSwitchDisplay->SetSwitchText(1, "2");
			mSwitchDisplay->ForceSwitchDisplay(1, mEngineLedColor);
			mSwitchDisplay->SetSwitchText(2, "3");
			mSwitchDisplay->ForceSwitchDisplay(2, mEngineLedColor);
			mSwitchDisplay->SetSwitchText(3, "4");
			mSwitchDisplay->ForceSwitchDisplay(3, mEngineLedColor);
			mSwitchDisplay->SetSwitchText(4, "5");
			mSwitchDisplay->ForceSwitchDisplay(4, mEngineLedColor);
			mSwitchDisplay->SetSwitchText(5, "6");
			mSwitchDisplay->ForceSwitchDisplay(5, mEngineLedColor);
			mSwitchDisplay->SetSwitchText(6, "7");
			mSwitchDisplay->ForceSwitchDisplay(6, mEngineLedColor);
			mSwitchDisplay->SetSwitchText(7, "8");
			mSwitchDisplay->ForceSwitchDisplay(7, mEngineLedColor);
			mSwitchDisplay->SetSwitchText(8, "9");
			mSwitchDisplay->ForceSwitchDisplay(8, mEngineLedColor);
			mSwitchDisplay->SetSwitchText(9, "0");
			mSwitchDisplay->ForceSwitchDisplay(9, mEngineLedColor);
			if (emBankDirect == curMode)
			{
				mSwitchDisplay->SetSwitchText(mDecrementSwitchNumber, "Backspace");
				mSwitchDisplay->ForceSwitchDisplay(mDecrementSwitchNumber, mEngineLedColor);
				mSwitchDisplay->SetSwitchText(mIncrementSwitchNumber, "Commit");
				mSwitchDisplay->ForceSwitchDisplay(mIncrementSwitchNumber, mEngineLedColor);
			}
			else
			{
#ifdef DECREMENT_IS_BACKSPACE
				mSwitchDisplay->SetSwitchText(mDecrementSwitchNumber, "Backspace");
#else
				mSwitchDisplay->SetSwitchText(mDecrementSwitchNumber, "Select MIDI Out...");
#endif
				mSwitchDisplay->ForceSwitchDisplay(mDecrementSwitchNumber, mEngineLedColor);

				if (emProgramChangeDirect == curMode)
				{
					mSwitchDisplay->SetSwitchText(mIncrementSwitchNumber, "Clear");
					mSwitchDisplay->ForceSwitchDisplay(mIncrementSwitchNumber, mEngineLedColor);
					mSwitchDisplay->SetSwitchText(10, "Set channel");
					mSwitchDisplay->ForceSwitchDisplay(10, mEngineLedColor);
					mSwitchDisplay->SetSwitchText(11, "Send bank select");
					mSwitchDisplay->ForceSwitchDisplay(11, mEngineLedColor);
					mSwitchDisplay->SetSwitchText(12, "Send program change");
					mSwitchDisplay->ForceSwitchDisplay(12, mEngineLedColor);
					mSwitchDisplay->SetSwitchText(13, "Decrement program");
					mSwitchDisplay->ForceSwitchDisplay(13, mEngineLedColor);
					mSwitchDisplay->SetSwitchText(14, "Increment program");
					mSwitchDisplay->ForceSwitchDisplay(14, mEngineLedColor);
				}
				else if (emControlChangeDirect == curMode)
				{
					mSwitchDisplay->SetSwitchText(mIncrementSwitchNumber, "Clear");
					mSwitchDisplay->ForceSwitchDisplay(mIncrementSwitchNumber, mEngineLedColor);
					mSwitchDisplay->SetSwitchText(10, "Set channel");
					mSwitchDisplay->ForceSwitchDisplay(10, mEngineLedColor);
					mSwitchDisplay->SetSwitchText(11, "Set controller");
					mSwitchDisplay->ForceSwitchDisplay(11, mEngineLedColor);
					mSwitchDisplay->SetSwitchText(12, "Send control change");
					mSwitchDisplay->ForceSwitchDisplay(12, mEngineLedColor);
					mSwitchDisplay->SetSwitchText(13, "Decrement value");
					mSwitchDisplay->ForceSwitchDisplay(13, mEngineLedColor);
					mSwitchDisplay->SetSwitchText(14, "Increment value");
					mSwitchDisplay->ForceSwitchDisplay(14, mEngineLedColor);
				}
				else if (emClockSetup == curMode)
				{
					// clock mode: 
					// 	1 		2 		3 		4 		5
					// 	6		7		8		9		0
					// 	Commit	Toggle	(tap)	Decr	Incr
					mSwitchDisplay->SetSwitchText(mIncrementSwitchNumber, "Clear");
					mSwitchDisplay->ForceSwitchDisplay(mIncrementSwitchNumber, mEngineLedColor);
					mSwitchDisplay->SetSwitchText(10, "Set tempo");
					mSwitchDisplay->ForceSwitchDisplay(10, mEngineLedColor);
					mSwitchDisplay->SetSwitchText(11, "Toggle clock on/off");
					mSwitchDisplay->ForceSwitchDisplay(11, mEngineLedColor);
					mSwitchDisplay->SetSwitchText(12, ""); // maybe tap tempo??
					// mSwitchDisplay->ForceSwitchDisplay(12, mEngineLedColor);
					mSwitchDisplay->SetSwitchText(13, "Decrement tempo");
					mSwitchDisplay->ForceSwitchDisplay(13, mEngineLedColor);
					mSwitchDisplay->SetSwitchText(14, "Increment tempo");
					mSwitchDisplay->ForceSwitchDisplay(14, mEngineLedColor);

					if (mMidiOut)
					{
						// update saved val with current tempo
						mDirectValueLastSent = mMidiOut->GetTempo();
					}
				}
			}
		}
		break;
	default:
		msg = "Invalid";
	    break;
	}

	if (showModeInMainDisplay && mMainDisplay && !msg.empty())
	{
		mMainDisplay->TextOut("mode: " + msg);

		if (emProgramChangeDirect == curMode ||
			emControlChangeDirect == curMode ||
			emClockSetup == curMode)
		{
			if (mMidiOut)
			{
				std::string curName(mMidiOut->GetMidiOutDeviceName());
				if (!curName.empty())
				{
					std::string portInfo("\r\nport: " + curName);
					mMainDisplay->AppendText(portInfo);
				}
			}
		}
	}

	if (mSwitchDisplay)
	{
		if (!msg.empty())
		{
			if (emBankNav == curMode)
				msg = "Escape Navigation";
			else if (emBank != curMode)
				msg = "Exit " + msg;
			mSwitchDisplay->SetSwitchText(mModeSwitchNumber, msg);
		}

		switch (curMode)
		{
		case emBank:
			mSwitchDisplay->EnableDisplayUpdate(true);
			mSwitchDisplay->TurnOffSwitchDisplay(mModeSwitchNumber);
			break;
		case emModeSelect:
		case emBankNav:
			mSwitchDisplay->EnableDisplayUpdate(false);
			mSwitchDisplay->ForceSwitchDisplay(mModeSwitchNumber, mEngineLedColor);
			mSwitchDisplay->ForceSwitchDisplay(mDecrementSwitchNumber, mEngineLedColor);
			mSwitchDisplay->ForceSwitchDisplay(mIncrementSwitchNumber, mEngineLedColor);
			break;
		default:
			mSwitchDisplay->EnableDisplayUpdate(false);
			mSwitchDisplay->ForceSwitchDisplay(mModeSwitchNumber, mEngineLedColor);
		}
	}
}

void
MidiControlEngine::UpdateBankModeSwitchDisplay()
{
	if (!mSwitchDisplay)
		return;

	const EngineMode curMode = CurrentMode();
	_ASSERTE(emBank == curMode);
	if (mActiveBank)
	{
		std::strstream msg;
		msg << mActiveBank->GetBankNumber() << ": " << mActiveBank->GetBankName() << std::ends;
		mSwitchDisplay->SetSwitchText(mModeSwitchNumber, msg.str());
	}
	else
	{
		mSwitchDisplay->SetSwitchText(mModeSwitchNumber, "Bank");
	}
}

void
MidiControlEngine::EscapeToDefaultMode()
{
	ChangeMode(emBank);
	mBankNavigationIndex = mActiveBankIndex;
	NavigateBankRelative(0);
}

PatchPtr
MidiControlEngine::GetPatch(int number)
{
	return mPatches[number];
}

int
MidiControlEngine::GetPatchNumber(const std::string & name) const
{
	for (const auto& curItem : mPatches)
	{
		if (curItem.second->GetName() == name)
			return curItem.second->GetNumber();
	}
	return -1;
}

void
MidiControlEngine::AssignCustomBankLoad(int switchNumber, int bankNumber)
{
	mBankLoadSwitchNumbers[switchNumber] = bankNumber;
}

void
MidiControlEngine::AssignCustomBankLoad(int switchNumber, const std::string &bankName)
{
	mBankLoadSwitchBankNamesForInit[switchNumber] = bankName;
}

void
MidiControlEngine::AssignModeSwitchNumber(EngineModeSwitch mode, int switchNumber)
{
	mOtherModeSwitchNumbers[mode] = switchNumber;
}

int
MidiControlEngine::GetSwitchNumber(EngineModeSwitch m) const
{
	std::map<EngineModeSwitch, int>::const_iterator it = mOtherModeSwitchNumbers.find(m);
	if (it == mOtherModeSwitchNumbers.end())
		return -1;
	return it->second;
}

void
MidiControlEngine::SetupModeSelectSwitch(EngineModeSwitch m)
{
	const int switchNumber = GetSwitchNumber(m);
	if (kUnassignedSwitchNumber == switchNumber)
		return;

	std::string txt;

	switch (m)
	{
	case kModeRecall:
		txt = "[recall]";
		break;
	case kModeBack:
		txt = "[back (bank history)]";
		break;
	case kModeForward:
		txt = "[forward (bank history)]";
		break;
	case kModeBankDesc:
		txt = "Describe banks...";
		break;
	case kModeBankDirect:
		txt = "Bank direct access...";
		break;
	case kModeProgramChangeDirect:
		txt = "Manual program change...";
		break;
	case kModeControlChangeDirect:
		txt = "Manual control change...";
		break;
	case kModeExprPedalDisplay:
		txt = "Raw ADC values...";
		break;
	case kModeTestLeds:
		txt = "Test LEDs...";
		break;
	case kModeToggleTraceWindow:
		txt = "Toggle Trace Window";
		break;
	case kModeAdcOverride:
		txt = "ADC overrides...";
		break;
	case kModeTime:
		txt = "Time + utils...";
		break;
	case kModeClockSetup:
		txt = "MIDI beat clock...";
		break;
	case kModeMidiOutSelect:
		txt = "Select MIDI Out...";
		break;
	default:
		_ASSERTE(!"unhandled EngineModeSwitch");
		return;
	}

	mSwitchDisplay->SetSwitchText(switchNumber, txt);
	mSwitchDisplay->EnableDisplayUpdate(false);
	mSwitchDisplay->ForceSwitchDisplay(switchNumber, mEngineLedColor);
}

void
MidiControlEngine::SwitchReleased_PedalDisplayMode(int switchNumber)
{
	if (switchNumber == mModeSwitchNumber)
	{
		EscapeToDefaultMode();
	}
	else if (switchNumber >= 0 && switchNumber <= 3)
	{
		mSwitchDisplay->EnableDisplayUpdate(false);
		if (mSwitchDisplay)
			mSwitchDisplay->ForceSwitchDisplay(mPedalModePort, 0);
		mPedalModePort = switchNumber;
		if (mSwitchDisplay)
			mSwitchDisplay->ForceSwitchDisplay(mPedalModePort, mEngineLedColor);

		if (mMainDisplay)
		{
			std::strstream displayMsg;
			displayMsg << "ADC port " << (int) (mPedalModePort + 1) << " monitor\n" << std::ends;
			mMainDisplay->TextOut(displayMsg.str());
		}
	}
}

void
MidiControlEngine::SwitchReleased_AdcOverrideMode(int switchNumber)
{
	if (switchNumber == mModeSwitchNumber)
	{
		EscapeToDefaultMode();
	}
	else if (switchNumber >= 0 && switchNumber <= 3)
	{
		if (mApplication)
			mApplication->ToggleAdcOverride(switchNumber);
		ChangeMode(emAdcOverride);
	}
}

void
MidiControlEngine::SwitchReleased_TimeDisplay(int switchNumber)
{
	int timeModeSwitchNumber = GetSwitchNumber(kModeTime);
	if (kUnassignedSwitchNumber == timeModeSwitchNumber)
		return;

	if (timeModeSwitchNumber == switchNumber ||
		mModeSwitchNumber == switchNumber)
	{
		mApplication->EnableTimeDisplay(false);
		EscapeToDefaultMode();
	}
	else if (switchNumber == timeModeSwitchNumber + 1)
		mApplication->PauseOrResumeTime();
	else if (switchNumber == timeModeSwitchNumber + 2)
		mApplication->ResetTime();
	else if (switchNumber == timeModeSwitchNumber + 3)
		mApplication->Exit(ITrollApplication::soeExit);
	else if (switchNumber == timeModeSwitchNumber + 4)
		mApplication->Exit(ITrollApplication::soeExitAndSleep);
	else if (switchNumber == timeModeSwitchNumber + 5)
		mApplication->Exit(ITrollApplication::soeExitAndHibernate);
}

void
MidiControlEngine::SwitchReleased_LedTests(int switchNumber)
{
	const int ledTestsModeSwitchNumber = GetSwitchNumber(kModeTestLeds);
	const int cmdSwitchNumberBase = 0;
	if (!mSwitchDisplay)
	{
		EscapeToDefaultMode();
		return;
	}

	mSwitchDisplay->EnableDisplayUpdate(true);

	if (ledTestsModeSwitchNumber == switchNumber || mModeSwitchNumber == switchNumber || cmdSwitchNumberBase == switchNumber)
		EscapeToDefaultMode();
	else if (switchNumber == cmdSwitchNumberBase + 1)
	{
		mSwitchDisplay->TestLeds(0);
		// ok to restore mode since pattern 0 is software-defined, the other patterns are firmware-defined
		ChangeMode(emLedTests);
	}
	else if (switchNumber == cmdSwitchNumberBase + 2)
	{
		mSwitchDisplay->TestLeds(1);
		// ok to restore mode since pattern 1 is software-defined, the other patterns are firmware-defined
		ChangeMode(emLedTests);
	}
	else if (switchNumber == cmdSwitchNumberBase + 3)
		mSwitchDisplay->TestLeds(10); // don't reset mode since the call happens on the hardware without blocking
	else if (switchNumber == cmdSwitchNumberBase + 4)
		mSwitchDisplay->TestLeds(11); // don't reset mode since the call happens on the hardware without blocking
}

void
MidiControlEngine::SwitchReleased_NavAndDescMode(int switchNumber)
{
	const EngineMode curMode = CurrentMode();
	if (switchNumber == mIncrementSwitchNumber)
	{
		// bank inc/dec does not commit bank
		NavigateBankRelative(1);
		mSwitchDisplay->ForceSwitchDisplay(mDecrementSwitchNumber, mEngineLedColor);
		mSwitchDisplay->ForceSwitchDisplay(mIncrementSwitchNumber, mEngineLedColor);
	}
	else if (switchNumber == mDecrementSwitchNumber)
	{
		// bank inc/dec does not commit bank
		NavigateBankRelative(-1);
		mSwitchDisplay->ForceSwitchDisplay(mDecrementSwitchNumber, mEngineLedColor);
		mSwitchDisplay->ForceSwitchDisplay(mIncrementSwitchNumber, mEngineLedColor);
	}
	else if (switchNumber == mModeSwitchNumber)
	{
		EscapeToDefaultMode();
	}
	else if (emBankNav == curMode)
	{
		// any switch release (except inc/dec/util) after bank inc/dec commits bank
		// reset to default mode when in bankNav mode
		ChangeMode(emBank);
		LoadBank(mBankNavigationIndex);
	}
	else if (emBankDesc == curMode)
	{
		PatchBankPtr bank = GetBank(mBankNavigationIndex);
		if (bank)
		{
			bank->DisplayInfo(mMainDisplay, mSwitchDisplay, true, true);
			bank->DisplayDetailedPatchInfo(switchNumber, mMainDisplay);
		}
	}
}

void
MidiControlEngine::SwitchReleased_ModeSelect(int switchNumber)
{
	if (switchNumber == mModeSwitchNumber)
		EscapeToDefaultMode();
	else if (switchNumber == mDecrementSwitchNumber)
	{
		ChangeMode(emBankNav);
		mBankNavigationIndex = mActiveBankIndex;
		NavigateBankRelative(-1);
		mSwitchDisplay->ForceSwitchDisplay(mDecrementSwitchNumber, mEngineLedColor);
		mSwitchDisplay->ForceSwitchDisplay(mIncrementSwitchNumber, mEngineLedColor);
	}
	else if (switchNumber == mIncrementSwitchNumber)
	{
		ChangeMode(emBankNav);
		mBankNavigationIndex = mActiveBankIndex;
		NavigateBankRelative(1);
		mSwitchDisplay->ForceSwitchDisplay(mDecrementSwitchNumber, mEngineLedColor);
		mSwitchDisplay->ForceSwitchDisplay(mIncrementSwitchNumber, mEngineLedColor);
	}
	else if (switchNumber == GetSwitchNumber(kModeRecall))
	{
		EscapeToDefaultMode();
		HistoryRecall();
	}
	else if (switchNumber == GetSwitchNumber(kModeBack))
	{
		EscapeToDefaultMode();
		HistoryBackward();
	}
	else if (switchNumber == GetSwitchNumber(kModeForward))
	{
		EscapeToDefaultMode();
		HistoryForward();
	}
	else if (switchNumber == GetSwitchNumber(kModeBankDesc))
	{
		ChangeMode(emBankDesc);
		mBankNavigationIndex = mActiveBankIndex;
		NavigateBankRelative(0);
		mSwitchDisplay->ForceSwitchDisplay(mDecrementSwitchNumber, mEngineLedColor);
		mSwitchDisplay->ForceSwitchDisplay(mIncrementSwitchNumber, mEngineLedColor);
	}
	else if (switchNumber == GetSwitchNumber(kModeBankDirect))
		ChangeMode(emBankDirect);
	else if (switchNumber == GetSwitchNumber(kModeProgramChangeDirect))
		ChangeMode(emProgramChangeDirect);
	else if (switchNumber == GetSwitchNumber(kModeControlChangeDirect))
		ChangeMode(emControlChangeDirect);
	else if (switchNumber == GetSwitchNumber(kModeExprPedalDisplay))
		ChangeMode(emExprPedalDisplay);
	else if (switchNumber == GetSwitchNumber(kModeTestLeds))
		ChangeMode(emLedTests);
	else if (switchNumber == GetSwitchNumber(kModeToggleTraceWindow))
	{
		if (mApplication)
			mApplication->ToggleTraceWindow();
	}
	else if (switchNumber == GetSwitchNumber(kModeTime))
		ChangeMode(emTimeDisplay);
	else if (switchNumber == GetSwitchNumber(kModeAdcOverride))
		ChangeMode(emAdcOverride);
	else if (switchNumber == GetSwitchNumber(kModeClockSetup))
		ChangeMode(emClockSetup);
	else if (switchNumber == GetSwitchNumber(kModeMidiOutSelect))
		ChangeMode(emMidiOutSelect);
	else
	{
		std::map<int, int>::const_iterator it = mBankLoadSwitchNumbers.find(switchNumber);
		if (it != mBankLoadSwitchNumbers.end())
		{
			EscapeToDefaultMode();
			LoadBankByNumber(it->second);
		}
	}
}

void
MidiControlEngine::SwitchReleased_BankDirect(int switchNumber)
{
	bool updateMainDisplay = true;
	switch (switchNumber)
	{
	case 0:		mDirectNumber += "1";	break;
	case 1:		mDirectNumber += "2";	break;
	case 2:		mDirectNumber += "3";	break;
	case 3:		mDirectNumber += "4";	break;
	case 4:		mDirectNumber += "5";	break;
	case 5:		mDirectNumber += "6";	break;
	case 6:		mDirectNumber += "7";	break;
	case 7:		mDirectNumber += "8";	break;
	case 8:		mDirectNumber += "9";	break;
	case 9:		mDirectNumber += "0";	break;
	default:
		if (switchNumber == mModeSwitchNumber)
		{
			EscapeToDefaultMode();
			updateMainDisplay = false;
		}
		else if (switchNumber == mDecrementSwitchNumber)
		{
			// remove last char
			if (mDirectNumber.length())
				mDirectNumber = mDirectNumber.erase(mDirectNumber.length() - 1);
		}
		else if (switchNumber == mIncrementSwitchNumber)
		{
			const int bnkIdx = GetBankIndex(::atoi(mDirectNumber.c_str()));
			if (bnkIdx != -1)
			{
				// commit
				ChangeMode(emBank);
				mBankNavigationIndex = mActiveBankIndex;
				LoadBank(bnkIdx);
			}
			else if (mMainDisplay)
				mMainDisplay->TextOut("Invalid bank number");
			return;
		}
	}

	if (mSwitchDisplay)
	{
		mSwitchDisplay->EnableDisplayUpdate(false);
		mSwitchDisplay->ForceSwitchDisplay(switchNumber, 0);
	}

	if (mMainDisplay && updateMainDisplay)
	{
		const int bnkIdx = GetBankIndex(::atoi(mDirectNumber.c_str()));
		if (bnkIdx == -1)
		{
			mMainDisplay->TextOut(mDirectNumber + " (invalid bank number)");
		}
		else
		{
			PatchBankPtr bnk = GetBank(bnkIdx);
			_ASSERTE(bnk);
			if (bnk)
				mMainDisplay->TextOut(mDirectNumber + " " + bnk->GetBankName());
		}
	}
}

void
MidiControlEngine::SwitchPressed_ProgramChangeDirect(int switchNumber)
{
	std::string msg;
	Bytes bytes;
	int program = -1;
	static bool sJustDidProgramChange = false;

	if (sJustDidProgramChange)
	{
		if (switchNumber >= 0 && switchNumber <= 9)
		{
			// clear buffer for entry restart - triggered by press of number switch after a send
			// need to otherwise retain value of mDirectNumber for inc/dec
			mDirectNumber.clear();
		}

		sJustDidProgramChange = false;
	}

	switch (switchNumber)
	{
	case 0:		mDirectNumber += "1";	break;
	case 1:		mDirectNumber += "2";	break;
	case 2:		mDirectNumber += "3";	break;
	case 3:		mDirectNumber += "4";	break;
	case 4:		mDirectNumber += "5";	break;
	case 5:		mDirectNumber += "6";	break;
	case 6:		mDirectNumber += "7";	break;
	case 7:		mDirectNumber += "8";	break;
	case 8:		mDirectNumber += "9";	break;
	case 9:		mDirectNumber += "0";	break;
	case 10:
		// Set channel
		mDirectChangeChannel = ::atoi(mDirectNumber.c_str());
		--mDirectChangeChannel;
		if (mDirectChangeChannel > 15)
			mDirectChangeChannel = 15;
		else if (mDirectChangeChannel < 0)
			mDirectChangeChannel = 0;
		msg += "set channel to " + mDirectNumber;
		mDirectNumber.clear();
		break;
	case 11:
		// bank select
		{
			int bank = ::atoi(mDirectNumber.c_str());
			if (bank > 127)
				bank = 127;
			else if (bank < 0)
				bank = 0;
			bytes.push_back(0xb0 | mDirectChangeChannel);
			bytes.push_back(0);
			bytes.push_back(bank);
		}
		msg += "send bank select " + mDirectNumber;
		mDirectNumber.clear();
		break;
	case 12:
		// program change
		if (mDirectNumber.empty())
			program = mDirectValueLastSent;
		else
			program = ::atoi(mDirectNumber.c_str());
		break;
	case 13:
	case 14:
		// dec/inc prog change
		if (mDirectNumber.empty())
			program = mDirectValueLastSent;
		else
			program = ::atoi(mDirectNumber.c_str());

		if (13 == switchNumber)
		{
			--program;
			if (program < 0)
				program = 127;
		}
		else
			++program;

		// update mDirectNumber with changed val
		{
			std::strstream strm;
			strm << program << std::ends;
			mDirectNumber = strm.str();
		}
		break;
	default:
		if (switchNumber == mDecrementSwitchNumber)
		{
#ifdef DECREMENT_IS_BACKSPACE
			// remove last char
			if (mDirectNumber.length())
				mDirectNumber = mDirectNumber.erase(mDirectNumber.length() - 1);
#else
			mMode.push(emMidiOutSelect);
			ChangeMode(emMidiOutSelect);
			return;
#endif
		}
		else if (switchNumber == mIncrementSwitchNumber)
		{
			// clear buf
			mDirectNumber.clear();
		}
		else 
			return;
	}

	if (-1 != program)
	{
		msg += "send ";
		if (program > 127)
		{
			int bank = 0;
			while (program > 127)
			{
				program -= 128;
				++bank;
			}

			// bank select
			bytes.push_back(0xb0 | mDirectChangeChannel);
			bytes.push_back(0);
			bytes.push_back(bank);
			msg += "bank and ";
		}

		bytes.push_back(0xc0 | mDirectChangeChannel);
		bytes.push_back(program);
		mDirectValueLastSent = program;
		msg += "program change ";
		sJustDidProgramChange = true;
	}

	if (mMainDisplay)
		mMainDisplay->TextOut(msg + mDirectNumber);

	if (mMidiOut)
	{
		if (!bytes.empty())
		{
			if (mMainDisplay)
			{
				const std::string byteDump("\r\n" + ::GetAsciiHexStr(bytes, true) + "\r\n");
				mMainDisplay->AppendText(byteDump);
			}
			mMidiOut->MidiOut(bytes);

			if (sJustDidProgramChange)
			{
				for (const auto& mgr : mAxeMgrs)
				{
					if (mgr->GetChannel() == mDirectChangeChannel)
						mgr->DelayedNameSyncFromAxe();
				}
			}
		}
	}
	else if (mMainDisplay)
		mMainDisplay->AppendText("\r\nNo midi out available for program changes");
}

void
MidiControlEngine::SwitchReleased_ProgramChangeDirect(int switchNumber)
{
	if (switchNumber == mModeSwitchNumber)
	{
		EscapeToDefaultMode();
	}
	else if (mSwitchDisplay)
	{
		mSwitchDisplay->EnableDisplayUpdate(false);
	}
}

void
MidiControlEngine::SwitchPressed_ControlChangeDirect(int switchNumber)
{
	std::string msg;
	Bytes bytes;
	int ctrlValue = -1;
	static bool sJustDidControlChange = false;

	if (sJustDidControlChange)
	{
		if (switchNumber >= 0 && switchNumber <= 9)
		{
			// clear buffer for entry restart - triggered by press of number switch after a send
			// need to otherwise retain value of mDirectNumber for inc/dec
			mDirectNumber.clear();
		}

		sJustDidControlChange = false;
	}

	switch (switchNumber)
	{
	case 0:		mDirectNumber += "1";	break;
	case 1:		mDirectNumber += "2";	break;
	case 2:		mDirectNumber += "3";	break;
	case 3:		mDirectNumber += "4";	break;
	case 4:		mDirectNumber += "5";	break;
	case 5:		mDirectNumber += "6";	break;
	case 6:		mDirectNumber += "7";	break;
	case 7:		mDirectNumber += "8";	break;
	case 8:		mDirectNumber += "9";	break;
	case 9:		mDirectNumber += "0";	break;
	case 10:
		// Set channel
		mDirectChangeChannel = ::atoi(mDirectNumber.c_str());
		--mDirectChangeChannel;
		if (mDirectChangeChannel > 15)
			mDirectChangeChannel = 15;
		else if (mDirectChangeChannel < 0)
			mDirectChangeChannel = 0;
		msg += "set channel to " + mDirectNumber;
		mDirectNumber.clear();
		break;
	case 11:
		// set controller
		{
			int controller = ::atoi(mDirectNumber.c_str());
			if (controller > 127)
				controller = 127;
			else if (controller < 0)
				controller = 0;
			mDirectValue1LastSent = controller;
		}
		msg += "set controller to " + mDirectNumber;
		mDirectNumber.clear();
		break;
	case 12:
		// control change
		if (mDirectNumber.empty())
			ctrlValue = mDirectValueLastSent;
		else
			ctrlValue = ::atoi(mDirectNumber.c_str());
		break;
	case 13:
	case 14:
		// dec/inc control change
		if (mDirectNumber.empty())
			ctrlValue = mDirectValueLastSent;
		else
			ctrlValue = ::atoi(mDirectNumber.c_str());

		if (13 == switchNumber)
		{
			--ctrlValue;
			if (ctrlValue < 0)
				ctrlValue = 127;
		}
		else
		{
			++ctrlValue;
			if (ctrlValue >= 128)
				ctrlValue = 0;
		}

		// update mDirectNumber with changed val
		{
			std::strstream strm;
			strm << ctrlValue << std::ends;
			mDirectNumber = strm.str();
		}
		break;
	default:
		if (switchNumber == mDecrementSwitchNumber)
		{
#ifdef DECREMENT_IS_BACKSPACE
			// remove last char
			if (mDirectNumber.length())
				mDirectNumber = mDirectNumber.erase(mDirectNumber.length() - 1);
#else
			mMode.push(emMidiOutSelect);
			ChangeMode(emMidiOutSelect);
			return;
#endif
		}
		else if (switchNumber == mIncrementSwitchNumber)
		{
			// clear buf
			mDirectNumber.clear();
		}
		else 
			return;
	}

	if (-1 != ctrlValue)
	{
		bytes.push_back(0xb0 | mDirectChangeChannel);
		bytes.push_back(mDirectValue1LastSent);
		bytes.push_back(ctrlValue);
		mDirectValueLastSent = ctrlValue;
		msg += "send control value ";
		sJustDidControlChange = true;
	}

	if (mMainDisplay)
		mMainDisplay->TextOut(msg + mDirectNumber);

	if (mMidiOut)
	{
		if (!bytes.empty())
		{
			if (mMainDisplay)
			{
				const std::string byteDump("\r\n" + ::GetAsciiHexStr(bytes, true) + "\r\n");
				mMainDisplay->AppendText(byteDump);
			}
			mMidiOut->MidiOut(bytes);
		}
	}
	else if (mMainDisplay)
		mMainDisplay->AppendText("\r\nNo midi out available for control changes");
}

void
MidiControlEngine::SwitchReleased_ControlChangeDirect(int switchNumber)
{
	if (switchNumber == mModeSwitchNumber)
	{
		EscapeToDefaultMode();
	}
	else if (mSwitchDisplay)
	{
		mSwitchDisplay->EnableDisplayUpdate(false);
	}
}

void
MidiControlEngine::SwitchPressed_ClockSetup(int switchNumber)
{
	std::string msg;
	Bytes bytes;
	int clockTempo = -1;

	switch (switchNumber)
	{
	case 0:		mDirectNumber += "1";	break;
	case 1:		mDirectNumber += "2";	break;
	case 2:		mDirectNumber += "3";	break;
	case 3:		mDirectNumber += "4";	break;
	case 4:		mDirectNumber += "5";	break;
	case 5:		mDirectNumber += "6";	break;
	case 6:		mDirectNumber += "7";	break;
	case 7:		mDirectNumber += "8";	break;
	case 8:		mDirectNumber += "9";	break;
	case 9:		mDirectNumber += "0";	break;
	case 10:
		// commit tempo
		if (mDirectNumber.empty())
			clockTempo = mDirectValueLastSent;
		else
			clockTempo = ::atoi(mDirectNumber.c_str());
		mDirectNumber.clear();
		break;
	case 11:
		// toggle clock on/off
		if (mMidiOut)
		{
			mMidiOut->EnableMidiClock(!mMidiOut->IsMidiClockEnabled());
			if (mMainDisplay)
			{
				msg += "MIDI clock ";
				if (mMidiOut->IsMidiClockEnabled())
				{
					std::strstream strm;
					strm << mMidiOut->GetTempo() << std::ends;

					msg += "enabled\r\n";
					msg += strm.str();
					msg += " BPM";
				}
				else
					msg += "disabled";

				mMainDisplay->TextOut(msg);
			}
		}
		else if (mMainDisplay)
			mMainDisplay->AppendText("\r\nNo midi out available for clock beat");
		return;
	case 12:
		// maybe tap tempo...
		return;
	case 13:
	case 14:
		// dec/inc clock
		if (mDirectNumber.empty())
			clockTempo = mDirectValueLastSent;
		else
			clockTempo = ::atoi(mDirectNumber.c_str());

		if (13 == switchNumber)
		{
			--clockTempo;
			if (clockTempo < 0)
				clockTempo = 200;
		}
		else
			++clockTempo;
		break;
	default:
		if (switchNumber == mDecrementSwitchNumber)
		{
#ifdef DECREMENT_IS_BACKSPACE
			// remove last char
			if (mDirectNumber.length())
				mDirectNumber = mDirectNumber.erase(mDirectNumber.length() - 1);
#else
			mMode.push(emMidiOutSelect);
			ChangeMode(emMidiOutSelect);
			return;
#endif
		}
		else if (switchNumber == mIncrementSwitchNumber)
		{
			// clear buf
			mDirectNumber.clear();
			msg = " ";
		}
		else
			return;
	}

	if (-1 != clockTempo)
	{
		if (mMidiOut)
		{
			mMidiOut->SetTempo(clockTempo);
			clockTempo = mMidiOut->GetTempo();
		}

		mDirectValueLastSent = clockTempo;

		if (mMainDisplay)
		{
			std::strstream strm;
			strm << clockTempo << std::ends;

			msg += "set tempo: ";
			msg += strm.str();
			msg += " BPM";

			if (mMidiOut)
			{
				msg += "\r\nclock is ";
				msg += mMidiOut->IsMidiClockEnabled() ? "enabled" : "disabled";
			}

			mMainDisplay->TextOut(msg);
		}

	}
	else if (mMainDisplay)
		mMainDisplay->TextOut(msg + mDirectNumber);

	if (!mMidiOut && mMainDisplay)
		mMainDisplay->AppendText("\r\nNo midi out available for clock beat");
}

void
MidiControlEngine::SwitchReleased_ClockSetup(int switchNumber)
{
	if (switchNumber == mModeSwitchNumber)
	{
		EscapeToDefaultMode();
	}
	else if (mSwitchDisplay)
	{
		mSwitchDisplay->EnableDisplayUpdate(false);
	}
}

void
MidiControlEngine::SwitchReleased_MidiOutSelect(int switchNumber)
{
	bool exitMode = false;
	if (mModeSwitchNumber == switchNumber)
	{
		exitMode = true;
	}
	else if (mMidiOutGenerator)
	{
		if (switchNumber < 15)
		{
			IMidiOutPtr m = mMidiOutGenerator->GetMidiOut(switchNumber);
			if (m)
			{
				mMidiOut = m;
				exitMode = true;
			}
		}
	}

	if (exitMode)
	{
		if (1 == mMode.size())
		{
			if (mSwitchDisplay)
				mSwitchDisplay->EnableDisplayUpdate(true);

			EscapeToDefaultMode();
		}
		else
		{
			mMode.pop();
			ChangeMode(mMode.top());
		}
	}
}
