/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2010 Sean Echevarria
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
#include "MidiControlEngine.h"
#include "PatchBank.h"
#include "IMainDisplay.h"
#include "ISwitchDisplay.h"
#include "ITraceDisplay.h"
#include "MetaPatch_BankNav.h"
#include "DeletePtr.h"
#include "SleepCommand.h"
#include "ITrollApplication.h"


struct DeletePatch
{
	void operator()(const std::pair<int, Patch *> & pr)
	{
		delete pr.second;
	}
};

static bool
SortByBankNumber(const PatchBank* lhs, const PatchBank* rhs)
{
	return lhs->GetBankNumber() < rhs->GetBankNumber();
}

// bad hardcoded switch grid assumptions - these need to come from one of the xml files...
// These are the switch numbers used in "Mode Select" mode
enum HardCodedSwitchNumbers
{
	kModeBankDescSwitchNumber = 0,
	kModeBankDirect,
	kModeExprPedalDisplay,
	kModeAdcOverride,
	kModeTestLeds,
	kModeToggleLedInversion,
	kModeReconnect,
	kModeToggleTraceWindow,
	kModeTime
};

const int kBankNavNextPatchNumber = -2; // reserved patch number
const int kBankNavPrevPatchNumber = -3; // reserved patch number

MidiControlEngine::MidiControlEngine(ITrollApplication * app,
									 IMainDisplay * mainDisplay, 
									 ISwitchDisplay * switchDisplay, 
									 ITraceDisplay * traceDisplay,
									 int incrementSwitchNumber,
									 int decrementSwitchNumber,
									 int modeSwitchNumber) :
	mApplication(app),
	mMainDisplay(mainDisplay),
	mSwitchDisplay(switchDisplay),
	mTrace(traceDisplay),
	mMode(emCreated),
	mActiveBank(NULL),
	mActiveBankIndex(0),
	mBankNavigationIndex(0),
	mPowerUpTimeout(0),
	mPowerUpBank(0),
	mPowerUpPatch(-1),
	mIncrementSwitchNumber(incrementSwitchNumber),
	mDecrementSwitchNumber(decrementSwitchNumber),
	mModeSwitchNumber(modeSwitchNumber),
	mFilterRedundantProgramChanges(false),
	mPedalModePort(0),
	mHistoryNavMode(hmNone)
{
	mBanks.reserve(999);
}

MidiControlEngine::~MidiControlEngine()
{
	std::for_each(mBanks.begin(), mBanks.end(), DeletePtr<PatchBank>());
	mBanks.clear();
	std::for_each(mPatches.begin(), mPatches.end(), DeletePatch());
	mPatches.clear();
}

PatchBank &
MidiControlEngine::AddBank(int number,
						   const std::string & name)
{
	PatchBank * pBank = new PatchBank(number, name);
	mBanks.push_back(pBank);
	return * pBank;
}

void
MidiControlEngine::AddPatch(Patch * patch)
{
	mPatches[patch->GetNumber()] = patch;
}

void
MidiControlEngine::SetPowerup(int powerupBank,
							  int powerupPatch,
							  int powerupTimeout)
{
	mPowerUpPatch = powerupPatch;
	mPowerUpBank = powerupBank;
	mPowerUpTimeout = powerupTimeout;
}

void
MidiControlEngine::CompleteInit(const PedalCalibration * pedalCalibrationSettings)
{
	std::sort(mBanks.begin(), mBanks.end(), SortByBankNumber);

	PatchBank * defaultsBank = NULL;
	if (mBanks.begin() != mBanks.end())
	{
		defaultsBank = *mBanks.begin();
		// bank number 0 is used as the bank with the default mappings
		if (0 == defaultsBank->GetBankNumber())
			defaultsBank->InitPatches(mPatches, mTrace); // init before calling SetDefaultMapping
		else
			defaultsBank = NULL;
	}

	// this is how we allow users to override Next and Prev switches in their banks
	// while at the same time reserving them for use by our modes.
	// If their default bank specifies a mapping for mIncrementSwitchNumber or 
	// mDecrementSwitchNumber, they will not get Next or Prev at all.
	PatchBank tmpDefaultBank(0, "nav default");
	AddPatch(new MetaPatch_BankNavNext(this, kBankNavNextPatchNumber, "Next Bank"));
	AddPatch(new MetaPatch_BankNavPrevious(this, kBankNavPrevPatchNumber, "Prev Bank"));
	tmpDefaultBank.AddPatchMapping(mIncrementSwitchNumber, kBankNavNextPatchNumber, PatchBank::stIgnore, PatchBank::stIgnore);
	tmpDefaultBank.AddPatchMapping(mDecrementSwitchNumber, kBankNavPrevPatchNumber, PatchBank::stIgnore, PatchBank::stIgnore);

	if (defaultsBank)
		defaultsBank->SetDefaultMappings(tmpDefaultBank);  // add Next and Prev to their default bank
	else
		defaultsBank = &tmpDefaultBank; // no default bank specified, use our manufactured one

	int itIdx = 0;
	for (Banks::iterator it = mBanks.begin();
		it != mBanks.end();
		++it, ++itIdx)
	{
		PatchBank * curItem = *it;
		curItem->InitPatches(mPatches, mTrace);
		curItem->SetDefaultMappings(*defaultsBank);
	}

	CalibrateExprSettings(pedalCalibrationSettings);
	ChangeMode(emBank);
	
	if (mTrace)
	{
		std::strstream traceMsg;
		traceMsg << "Load complete: bank cnt " << mBanks.size() << ", patch cnt " << mPatches.size() << std::endl << std::ends;
		mTrace->Trace(std::string(traceMsg.str()));
	}

	LoadStartupBank();

	// init pedals on the wire
	for (int idx = 0; idx < ExpressionPedals::PedalCount; ++idx)
		RefirePedal(idx);
}

void
MidiControlEngine::CalibrateExprSettings(const PedalCalibration * pedalCalibrationSettings)
{
	mGlobalPedals.Calibrate(pedalCalibrationSettings, this, mTrace);

	for (Banks::iterator it = mBanks.begin();
		it != mBanks.end();
		++it)
	{
		PatchBank * curItem = *it;
		curItem->CalibrateExprSettings(pedalCalibrationSettings, this, mTrace);
	}
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
		PatchBank * curItem = *it;
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
	if (0 && mTrace)
	{
		std::strstream msg;
		msg << "SwitchPressed: " << switchNumber << std::endl << std::ends;
		mTrace->Trace(std::string(msg.str()));
	}

	if (emBank == mMode)
	{
		if (mActiveBank)
			mActiveBank->PatchSwitchPressed(switchNumber, mMainDisplay, mSwitchDisplay);
		return;
	}

	if (emBankDirect == mMode)
	{
		if (mSwitchDisplay)
		{
			if ((switchNumber >= 0 && switchNumber <= 9) ||
				switchNumber == mDecrementSwitchNumber || 
				switchNumber == mIncrementSwitchNumber)
				mSwitchDisplay->SetSwitchDisplay(switchNumber, true);
		}
		return;
	}

	// no other mode responds to SwitchPressed
}

void
MidiControlEngine::SwitchReleased(int switchNumber)
{
	if (0 && mTrace)
	{
		std::strstream msg;
		msg << "SwitchReleased: " << switchNumber << std::endl << std::ends;
		mTrace->Trace(msg.str());
	}

	if (emCreated == mMode)
		return;

	if (emBank == mMode)
	{
		if (switchNumber == mModeSwitchNumber)
		{
			ChangeMode(emModeSelect);
			return;
		}

		if (mActiveBank)
			mActiveBank->PatchSwitchReleased(switchNumber, mMainDisplay, mSwitchDisplay);

		return;
	}

	if (emBankNav == mMode ||
		emBankDesc == mMode)
	{
		if (switchNumber == mIncrementSwitchNumber)
		{
			// bank inc/dec does not commit bank
			NavigateBankRelative(1);
		}
		else if (switchNumber == mDecrementSwitchNumber)
		{
			// bank inc/dec does not commit bank
			NavigateBankRelative(-1);
		}
		else if (switchNumber == mModeSwitchNumber)
		{
			EscapeToDefaultMode();
		}
		else if (emBankNav == mMode)
		{
			// any switch release (except inc/dec/util) after bank inc/dec commits bank
			// reset to default mode when in bankNav mode
			ChangeMode(emBank);
			LoadBank(mBankNavigationIndex);
		}
		else if (emBankDesc == mMode)
		{
			PatchBank * bank = GetBank(mBankNavigationIndex);
			if (bank)
			{
				bank->DisplayInfo(mMainDisplay, mSwitchDisplay, true, true);
				bank->DisplayDetailedPatchInfo(switchNumber, mMainDisplay);
			}
		}

		return;
	}

	if (emModeSelect == mMode)
	{
		if (switchNumber == mModeSwitchNumber)
			EscapeToDefaultMode();
		else if (switchNumber == mDecrementSwitchNumber)
		{
			ChangeMode(emBankNav);
			mBankNavigationIndex = mActiveBankIndex;
			NavigateBankRelative(-1);
		}
		else if (switchNumber == mIncrementSwitchNumber)
		{
			ChangeMode(emBankNav);
			mBankNavigationIndex = mActiveBankIndex;
			NavigateBankRelative(1);
		}
		else
		{
			switch (switchNumber)
			{
			case kModeBankDescSwitchNumber:
				ChangeMode(emBankDesc);
				mBankNavigationIndex = mActiveBankIndex;
				NavigateBankRelative(0);
				break;
			case kModeBankDirect:
				ChangeMode(emBankDirect);
				break;
			case kModeExprPedalDisplay:
				ChangeMode(emExprPedalDisplay);
				break;
			case kModeToggleLedInversion:
				if (mSwitchDisplay)
					mSwitchDisplay->InvertLeds(!mSwitchDisplay->IsInverted());
				EscapeToDefaultMode();
				break;
			case kModeReconnect:
				if (mApplication)
					mApplication->Reconnect();
				EscapeToDefaultMode();
				break;
			case kModeTestLeds:
				if (mSwitchDisplay)
					mSwitchDisplay->TestLeds();
				EscapeToDefaultMode();
				break;
			case kModeToggleTraceWindow:
				if (mApplication)
					mApplication->ToggleTraceWindow();
				break;
			case kModeTime:
				ChangeMode(emTimeDisplay);
				break;
			case kModeAdcOverride:
				ChangeMode(emAdcOverride);
				break;
			}
		}

		return;
	}

	if (emAdcOverride == mMode)
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

		return;
	}

	if (emExprPedalDisplay == mMode)
	{
		if (switchNumber == mModeSwitchNumber)
		{
			EscapeToDefaultMode();
		}
		else if (switchNumber >= 0 && switchNumber <= 3)
		{
			if (mSwitchDisplay)
				mSwitchDisplay->SetSwitchDisplay(mPedalModePort, false);
			mPedalModePort = switchNumber;
			if (mSwitchDisplay)
				mSwitchDisplay->SetSwitchDisplay(mPedalModePort, true);

			if (mMainDisplay)
			{
				std::strstream displayMsg;
				displayMsg << "ADC port " << (int) (mPedalModePort + 1) << " monitor" << std::endl << std::ends;
				mMainDisplay->TextOut(displayMsg.str());
			}
		}

		return;
	}

	if (emBankDirect == mMode)
	{
		bool updateMainDisplay = true;
		switch (switchNumber)
		{
		case 0:		mBankDirectNumber += "1";	break;
		case 1:		mBankDirectNumber += "2";	break;
		case 2:		mBankDirectNumber += "3";	break;
		case 3:		mBankDirectNumber += "4";	break;
		case 4:		mBankDirectNumber += "5";	break;
		case 5:		mBankDirectNumber += "6";	break;
		case 6:		mBankDirectNumber += "7";	break;
		case 7:		mBankDirectNumber += "8";	break;
		case 8:		mBankDirectNumber += "9";	break;
		case 9:		mBankDirectNumber += "0";	break;
		}

		if (switchNumber == mModeSwitchNumber)
		{
			EscapeToDefaultMode();
			updateMainDisplay = false;
		}
		else if (switchNumber == mDecrementSwitchNumber)
		{
			// remove last char
			if (mBankDirectNumber.length())
				mBankDirectNumber = mBankDirectNumber.erase(mBankDirectNumber.length() - 1);
		}
		else if (switchNumber == mIncrementSwitchNumber)
		{
			// commit
			ChangeMode(emBank);
			mBankNavigationIndex = mActiveBankIndex;
			const int bnkIdx = GetBankIndex(::atoi(mBankDirectNumber.c_str()));
			if (bnkIdx != -1)
				LoadBank(bnkIdx);
			else if (mMainDisplay)
				mMainDisplay->TextOut("Invalid bank number");
			updateMainDisplay = false;
		}

		if (mSwitchDisplay)
		{
			if ((switchNumber >= 0 && switchNumber <= 9) ||
				switchNumber == mDecrementSwitchNumber || 
				switchNumber == mIncrementSwitchNumber)
				mSwitchDisplay->SetSwitchDisplay(switchNumber, false);
		}

		if (mMainDisplay && updateMainDisplay)
		{
			const int bnkIdx = GetBankIndex(::atoi(mBankDirectNumber.c_str()));
			if (bnkIdx == -1)
			{
				mMainDisplay->TextOut(mBankDirectNumber + " (invalid bank number)");
			}
			else
			{
				PatchBank * bnk = GetBank(bnkIdx);
				_ASSERTE(bnk);
				mMainDisplay->TextOut(mBankDirectNumber + " " + bnk->GetBankName());
			}
		}

		return;
	}

	if (emTimeDisplay == mMode)
	{
		// any switch press/release cancels mode
		mApplication->EnableTimeDisplay(false);
		EscapeToDefaultMode();
		return;
	}
}

void
MidiControlEngine::AdcValueChanged(int port, 
								   int newValue)
{
	_ASSERTE(port < ExpressionPedals::PedalCount);
	if (emExprPedalDisplay == mMode)
	{
		if (mMainDisplay && mPedalModePort == port)
		{
			std::strstream displayMsg;
			displayMsg << "ADC port " << (int) (port+1) << " value: " << newValue << std::endl << std::ends;
			mMainDisplay->TextOut(displayMsg.str());
		}
		return;
	}

	// forward directly to active patch
	if (!gActivePatchPedals || 
		gActivePatchPedals->AdcValueChange(mMainDisplay, port, newValue))
	{
		// process globals if no rejection
		mGlobalPedals.AdcValueChange(mMainDisplay, port, newValue);
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
		for (int idx = 0; idx < 64; idx++)
		{
			if (idx != mModeSwitchNumber)
			{
				mSwitchDisplay->ClearSwitchText(idx);
				mSwitchDisplay->SetSwitchDisplay(idx, false);
			}
		}
	}

	// display bank info
	PatchBank * bank = GetBank(mBankNavigationIndex);
	if (!bank)
		return false;

	bank->DisplayInfo(mMainDisplay, mSwitchDisplay, true, relativeBankIndex != 0);
	return true;
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
			traceMsg << "Bank " << bankNumber << " does not exist" << std::endl << std::ends;
			mTrace->Trace(std::string(traceMsg.str()));
		}
		return;
	}

	ChangeMode(emBank);
	LoadBank(bankidx);

	PatchBank * bank = GetBank(bankidx);
	if (!bank)
	{
		if (mTrace)
		{
			std::strstream traceMsg;
			traceMsg << "Bank " << bankNumber << " does not exist" << std::endl << std::ends;
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
		PatchBank * curItem = *it;
		if (curItem->GetBankNumber() == bankNumber)
			return idx;
	}
	return -1;
}

PatchBank *
MidiControlEngine::GetBank(int bankIndex)
{
	const int kBankCnt = mBanks.size();
	if (bankIndex < 0 || bankIndex >= kBankCnt)
		return NULL;

	PatchBank * bank = mBanks[bankIndex];
	return bank;
}

bool
MidiControlEngine::LoadBank(int bankIndex)
{
	PatchBank * bank = GetBank(bankIndex);
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

	PatchBank * bank = GetBank(bankIdx);
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

	PatchBank * bank = GetBank(bankIdx);
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

// possible mode transitions:
// emCreated -> emDefault
// emDefault -> emBankNav
// emDefault -> emModeSelect
// emModeSelect -> emDefault
// emModeSelect -> emBankNav
// emModeSelect -> emBankDesc
// emModeSelect -> emBankDirect
// emModeSelect -> emExprPedalDisplay
// emBankNav -> emDefault
// emBankDesc -> emDefault
// emBankDirect -> emDefault
// emExprPedalDisplay -> emDefault
void
MidiControlEngine::ChangeMode(EngineMode newMode)
{
	mMode = newMode;

	if (mSwitchDisplay)
	{
		for (int idx = 0; idx < 64; idx++)
		{
			mSwitchDisplay->ClearSwitchText(idx);
			mSwitchDisplay->SetSwitchDisplay(idx, false);
		}
	}

	bool showModeInMainDisplay = true;
	std::string msg;
	switch (mMode)
	{
	case emBank:
		msg = "Bank";
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
		msg = "Mode Select";
		if (mSwitchDisplay)
		{
			// override possible user override in this mode
			mSwitchDisplay->SetSwitchText(mIncrementSwitchNumber, "Next Bank");
			mSwitchDisplay->SetSwitchText(mDecrementSwitchNumber, "Prev Bank");

			mSwitchDisplay->SetSwitchText(kModeBankDescSwitchNumber, "Bank Description...");
			mSwitchDisplay->SetSwitchDisplay(kModeBankDescSwitchNumber, true);
			mSwitchDisplay->SetSwitchText(kModeBankDirect, "Bank Direct...");
			mSwitchDisplay->SetSwitchDisplay(kModeBankDirect, true);
			mSwitchDisplay->SetSwitchText(kModeExprPedalDisplay, "Raw ADC Values...");
			mSwitchDisplay->SetSwitchDisplay(kModeExprPedalDisplay, true);
			mSwitchDisplay->SetSwitchText(kModeTestLeds, "Test LEDs");
			mSwitchDisplay->SetSwitchDisplay(kModeTestLeds, true);
			mSwitchDisplay->SetSwitchText(kModeToggleLedInversion, "Toggle LED Inversion");
			mSwitchDisplay->SetSwitchDisplay(kModeToggleLedInversion, true);
			mSwitchDisplay->SetSwitchText(kModeReconnect, "Reconnect to Monome");
			mSwitchDisplay->SetSwitchDisplay(kModeReconnect, true);
			mSwitchDisplay->SetSwitchText(kModeToggleTraceWindow, "Toggle Trace Window");
			mSwitchDisplay->SetSwitchDisplay(kModeToggleTraceWindow, true);
			mSwitchDisplay->SetSwitchText(kModeAdcOverride, "ADC overrides...");
			mSwitchDisplay->SetSwitchDisplay(kModeAdcOverride, true);
			mSwitchDisplay->SetSwitchText(kModeTime, "Display Current Time");
			mSwitchDisplay->SetSwitchDisplay(kModeTime, true);
		}
		break;
	case emTimeDisplay:
		msg = "Time Display";
		showModeInMainDisplay = false;
		mSwitchDisplay->SetSwitchText(kModeTime, "Any button to exit...");
		mSwitchDisplay->SetSwitchDisplay(kModeTime, true);
		if (!mApplication || !mApplication->EnableTimeDisplay(true))
			EscapeToDefaultMode();
		break;
	case emExprPedalDisplay:
		msg = "Raw ADC values";
		mPedalModePort = 0;
		if (mSwitchDisplay)
		{
			mSwitchDisplay->SetSwitchText(mIncrementSwitchNumber, "");
			mSwitchDisplay->SetSwitchText(mDecrementSwitchNumber, "");
			mSwitchDisplay->SetSwitchText(0, "Pedal 1");
			mSwitchDisplay->SetSwitchDisplay(0, true);
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
			mSwitchDisplay->SetSwitchText(mIncrementSwitchNumber, "");
			mSwitchDisplay->SetSwitchText(mDecrementSwitchNumber, "");

			for (int idx = 0; idx < 4; ++idx)
			{
				std::strstream msg;
				msg << "ADC " << idx;
				if (mApplication->IsAdcOverridden(idx))
				{
					msg << " forced off" << std::endl << std::ends;
					mSwitchDisplay->SetSwitchText(idx, msg.str());
					mSwitchDisplay->SetSwitchDisplay(idx, true);
				}
				else
				{
					msg << " normal" << std::endl << std::ends;
					mSwitchDisplay->SetSwitchText(idx, msg.str());
					mSwitchDisplay->SetSwitchDisplay(idx, false);
				}
			}
		}
		break;
	case emBankDirect:
		mBankDirectNumber.clear();
		msg = "Bank Direct";
		if (mSwitchDisplay)
		{
			mSwitchDisplay->SetSwitchText(0, "1");
			mSwitchDisplay->SetSwitchText(1, "2");
			mSwitchDisplay->SetSwitchText(2, "3");
			mSwitchDisplay->SetSwitchText(3, "4");
			mSwitchDisplay->SetSwitchText(4, "5");
			mSwitchDisplay->SetSwitchText(5, "6");
			mSwitchDisplay->SetSwitchText(6, "7");
			mSwitchDisplay->SetSwitchText(7, "8");
			mSwitchDisplay->SetSwitchText(8, "9");
			mSwitchDisplay->SetSwitchText(9, "0");
			mSwitchDisplay->SetSwitchText(mIncrementSwitchNumber, "Commit");
			mSwitchDisplay->SetSwitchText(mDecrementSwitchNumber, "Backspace");
		}
		break;
	default:
		msg = "Invalid";
	    break;
	}

	if (showModeInMainDisplay && mMainDisplay && !msg.empty())
		mMainDisplay->TextOut("mode: " +  msg);

	if (mSwitchDisplay)
	{
		if (!msg.empty())
		{
			if (emBank != mMode)
				msg = "Exit " + msg;
			mSwitchDisplay->SetSwitchText(mModeSwitchNumber, msg);
		}
		mSwitchDisplay->SetSwitchDisplay(mModeSwitchNumber, mMode == emBank ? true : false);
	}
}

void
MidiControlEngine::UpdateBankModeSwitchDisplay()
{
	if (!mSwitchDisplay)
		return;

	_ASSERTE(emBank == mMode);
	if (mActiveBank)
	{
		std::strstream msg;
		msg << mActiveBank->GetBankNumber() << ": " << mActiveBank->GetBankName() << std::endl << std::ends;
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

Patch *
MidiControlEngine::GetPatch(int number)
{
	return mPatches[number];
}
