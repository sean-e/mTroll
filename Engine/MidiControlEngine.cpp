/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2012 Sean Echevarria
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
#include "HexStringUtils.h"
#include "AxeFxManager.h"
#ifdef _WINDOWS
	#include <windows.h>
	#define CurTime	GetTickCount // time in ms (used to measure elapsed time between events, origin doesn't matter)
#else
	#define CurTime	??
#endif // _WINDOWS


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

const int kBankNavNextPatchNumber = -2; // reserved patch number
const int kBankNavPrevPatchNumber = -3; // reserved patch number

MidiControlEngine::MidiControlEngine(ITrollApplication * app,
									 IMainDisplay * mainDisplay, 
									 ISwitchDisplay * switchDisplay, 
									 ITraceDisplay * traceDisplay,
									 IMidiOut * midiOut,
									 AxeFxManager * axMgr,
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
	mHistoryNavMode(hmNone),
	mDirectChangeChannel(0),
	mDirectValueLastSent(0),
	mDirectValue1LastSent(0),
	mMidiOut(midiOut),
	mAxeMgr(axMgr),
	mSwitchPressedEventTime(0)
{
	if (mAxeMgr)
		mAxeMgr->AddRef();
	mBanks.reserve(999);
}

MidiControlEngine::~MidiControlEngine()
{
	if (mAxeMgr)
		mAxeMgr->Release();
	std::for_each(mBanks.begin(), mBanks.end(), DeletePtr<PatchBank>());
	mBanks.clear();
	std::for_each(mPatches.begin(), mPatches.end(), DeletePatch());
	mPatches.clear();
}

PatchBank &
MidiControlEngine::AddBank(int number,
						   const std::string & name)
{
	if (mTrace)
	{
		for (Banks::iterator it = mBanks.begin();
			it != mBanks.end();
			++it)
		{
			PatchBank * curItem = *it;
			if (curItem && curItem->GetBankNumber() == number)
			{
				std::strstream traceMsg;
				traceMsg << "Warning: multiple banks with bank number " << number << std::endl << std::ends;
				mTrace->Trace(std::string(traceMsg.str()));
			}
		}
	}

	PatchBank * pBank = new PatchBank(number, name);
	mBanks.push_back(pBank);
	return * pBank;
}

void
MidiControlEngine::AddPatch(Patch * patch)
{
	const int patchNum = patch->GetNumber();
	Patch * prev = mPatches[patchNum];
	if (prev && mTrace)
	{
		std::strstream traceMsg;
		traceMsg << "ERROR: multiple patches with patch number " << patchNum << std::endl << std::ends;
		mTrace->Trace(std::string(traceMsg.str()));
	}
	mPatches[patchNum] = patch;
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
	if (!mOtherModeSwitchNumbers.size())
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
		mOtherModeSwitchNumbers[kModeToggleLedInversion] = kModeToggleLedInversion;
	}

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

	PatchBank tmpDefaultBank(0, "nav default");
	AddPatch(new MetaPatch_BankNavNext(this, kBankNavNextPatchNumber, "Next Bank"));
	AddPatch(new MetaPatch_BankNavPrevious(this, kBankNavPrevPatchNumber, "Prev Bank"));

	// this is how we allow users to override Next and Prev switches in their banks
	// while at the same time reserving them for use by our modes.
	// If their default bank specifies a mapping for mIncrementSwitchNumber or 
	// mDecrementSwitchNumber, they will not get Next or Prev at all.
	// CHANGED: no longer add Next and prev by default
// 	tmpDefaultBank.AddPatchMapping(mIncrementSwitchNumber, kBankNavNextPatchNumber, PatchBank::stIgnore, PatchBank::stIgnore);
// 	tmpDefaultBank.AddPatchMapping(mDecrementSwitchNumber, kBankNavPrevPatchNumber, PatchBank::stIgnore, PatchBank::stIgnore);

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
		traceMsg << "Loaded " << mBanks.size() << " banks, " << mPatches.size() << " patches" << std::endl << std::ends;
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

	mSwitchPressedEventTime = CurTime();
	if (emBank == mMode)
	{
		if (switchNumber == mModeSwitchNumber)
			return;

		if (mActiveBank)
			mActiveBank->PatchSwitchPressed(switchNumber, mMainDisplay, mSwitchDisplay);
		return;
	}

	if (emBankDirect == mMode || 
		emProgramChangeDirect == mMode ||
		emControlChangeDirect == mMode)
	{
		bool doUpdate = false;
		if ((switchNumber >= 0 && switchNumber <= 9) ||
			switchNumber == mDecrementSwitchNumber || 
			switchNumber == mIncrementSwitchNumber)
			doUpdate = true;

		if ((emProgramChangeDirect == mMode || emControlChangeDirect == mMode) && 
			switchNumber >= 10 && switchNumber <= 14)
			doUpdate = true;

		if (mSwitchDisplay && doUpdate)
		{
			mSwitchDisplay->EnableDisplayUpdate(false);
			mSwitchDisplay->ForceSwitchDisplay(switchNumber, true);
		}

		if (emProgramChangeDirect == mMode)
			SwitchPressed_ProgramChangeDirect(switchNumber);
		else if (emControlChangeDirect == mMode)
			SwitchPressed_ControlChangeDirect(switchNumber);

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

	const int kLongPressMinDuration = 500; // milliseconds
	const bool kIsLongPress = (CurTime() - mSwitchPressedEventTime) > kLongPressMinDuration;
	switch (mMode)
	{
	case emBank:
		// default mode
		if (switchNumber == mModeSwitchNumber)
		{
			if (kIsLongPress)
			{
				// TODO: long-press function of mode switch should be user-definable
				HistoryBackward();
			}
			else
				ChangeMode(emModeSelect);
			return;
		}

		if (mActiveBank)
			mActiveBank->PatchSwitchReleased(switchNumber, mMainDisplay, mSwitchDisplay);
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
	switch (mMode)
	{
	case emExprPedalDisplay:
		if (mMainDisplay && mPedalModePort == port)
		{
			std::strstream displayMsg;
			displayMsg << "ADC port " << (int) (port+1) << " value: " << newValue << std::endl << std::ends;
			mMainDisplay->TextOut(displayMsg.str());
		}
		return;

	case emBank:
	case emProgramChangeDirect:
	case emControlChangeDirect:
	case emModeSelect:
	case emTimeDisplay:
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
				mSwitchDisplay->ForceSwitchDisplay(idx, false);
			}
		}
	}

	if (emBank == mMode && mSwitchDisplay)
		mSwitchDisplay->EnableDisplayUpdate(true);

	// display bank info
	PatchBank * bank = GetBank(mBankNavigationIndex);
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

	PatchBank * bank = GetBank(mBankNavigationIndex);
	if (!bank)
	{
		if (mTrace)
		{
			std::strstream traceMsg;
			traceMsg << "Bank navigation error" << std::endl << std::ends;
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
// emModeSelect -> emProgramChangeDirect
// emModeSelect -> emControlChangeDirect
// emModeSelect -> emExprPedalDisplay
// emBankNav -> emDefault
// emBankDesc -> emDefault
// emBankDirect -> emDefault
// emProgramChangeDirect -> emDefault
// emControlChangeDirect -> emDefault
// emExprPedalDisplay -> emDefault
void
MidiControlEngine::ChangeMode(EngineMode newMode)
{
	mMode = newMode;

	if (mSwitchDisplay)
	{
		mSwitchDisplay->EnableDisplayUpdate(false);
		for (int idx = 0; idx < 64; idx++)
		{
			mSwitchDisplay->ClearSwitchText(idx);
			mSwitchDisplay->ForceSwitchDisplay(idx, false);
		}
	}

	bool showModeInMainDisplay = true;
	std::string msg;
	switch (mMode)
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
		msg = "Mode Select";
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
			SetupModeSelectSwitch(kModeToggleLedInversion);

			mSwitchDisplay->EnableDisplayUpdate(false);
			for (std::map<int, int>::const_iterator it = mBankLoadSwitchNumbers.begin();
				it != mBankLoadSwitchNumbers.end(); ++it)
			{
				PatchBank * bnk = GetBank(GetBankIndex(it->second));
				if (bnk)
				{
					std::string txt("Bank: ");
					txt += bnk->GetBankName();
					mSwitchDisplay->SetSwitchText(it->first, txt);
					mSwitchDisplay->ForceSwitchDisplay(it->first, true);
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
				mSwitchDisplay->SetSwitchText(switchNumber, "Any button to exit");
				mSwitchDisplay->ForceSwitchDisplay(switchNumber, true);
			}
			if (!mApplication || !mApplication->EnableTimeDisplay(true))
				EscapeToDefaultMode();
		}
		break;
	case emExprPedalDisplay:
		msg = "Raw ADC values";
		mPedalModePort = 0;
		if (mSwitchDisplay)
		{
			mSwitchDisplay->SetSwitchText(0, "Pedal 1");
			mSwitchDisplay->EnableDisplayUpdate(false);
			mSwitchDisplay->ForceSwitchDisplay(0, true);
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
				std::strstream msg;
				msg << "ADC " << idx;
				if (mApplication->IsAdcOverridden(idx))
				{
					msg << " forced off" << std::endl << std::ends;
					mSwitchDisplay->SetSwitchText(idx, msg.str());
					mSwitchDisplay->ForceSwitchDisplay(idx, true);
				}
				else
				{
					msg << " normal" << std::endl << std::ends;
					mSwitchDisplay->SetSwitchText(idx, msg.str());
					mSwitchDisplay->ForceSwitchDisplay(idx, false);
				}
			}
		}
		break;
	case emProgramChangeDirect:
	case emControlChangeDirect:
		if (!mMidiOut)
		{
			if (mMainDisplay)
				mMainDisplay->TextOut("Program or control change not available without MIDI out");
			break;
		}
		// fall-through
	case emBankDirect:
		mDirectNumber.clear();
		if (emBankDirect == mMode)
			msg = "Bank Direct";
		else if (emProgramChangeDirect == mMode)
			msg = "Manual Program Changes";
		else if (emControlChangeDirect == mMode)
			msg = "Manual Control Changes";

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
			mSwitchDisplay->SetSwitchText(mDecrementSwitchNumber, "Backspace");
			if (emBankDirect == mMode)
				mSwitchDisplay->SetSwitchText(mIncrementSwitchNumber, "Commit");
			else if (emProgramChangeDirect == mMode)
			{
				mSwitchDisplay->SetSwitchText(mIncrementSwitchNumber, "Clear");
				mSwitchDisplay->SetSwitchText(10, "Set channel");
				mSwitchDisplay->SetSwitchText(11, "Send bank select");
				mSwitchDisplay->SetSwitchText(12, "Send program change");
				mSwitchDisplay->SetSwitchText(13, "Decrement program");
				mSwitchDisplay->SetSwitchText(14, "Increment program");
			}
			else if (emControlChangeDirect == mMode)
			{
				mSwitchDisplay->SetSwitchText(mIncrementSwitchNumber, "Clear");
				mSwitchDisplay->SetSwitchText(10, "Set channel");
				mSwitchDisplay->SetSwitchText(11, "Set controller");
				mSwitchDisplay->SetSwitchText(12, "Send control change");
				mSwitchDisplay->SetSwitchText(13, "Decrement value");
				mSwitchDisplay->SetSwitchText(14, "Increment value");
			}
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
			if (emBankNav == mMode)
				msg = "Escape Navigation";
			else if (emBank != mMode)
				msg = "Exit " + msg;
			mSwitchDisplay->SetSwitchText(mModeSwitchNumber, msg);
		}

		switch (mMode)
		{
		case emBank:
			mSwitchDisplay->EnableDisplayUpdate(true);
			mSwitchDisplay->SetSwitchDisplay(mModeSwitchNumber, false);
			break;
		case emModeSelect:
		case emBankNav:
			mSwitchDisplay->EnableDisplayUpdate(false);
			mSwitchDisplay->ForceSwitchDisplay(mModeSwitchNumber, true);
			mSwitchDisplay->ForceSwitchDisplay(mDecrementSwitchNumber, true);
			mSwitchDisplay->ForceSwitchDisplay(mIncrementSwitchNumber, true);
			break;
		default:
			mSwitchDisplay->EnableDisplayUpdate(false);
			mSwitchDisplay->ForceSwitchDisplay(mModeSwitchNumber, true);
		}
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

void
MidiControlEngine::AssignCustomBankLoad(int switchNumber, int bankNumber)
{
	mBankLoadSwitchNumbers[switchNumber] = bankNumber;
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
		txt = "Recall last bank";
		break;
	case kModeBack:
		txt = "Go back (bank history)";
		break;
	case kModeForward:
		txt = "Go forward (bank history)";
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
		txt = "Test LEDs";
		break;
	case kModeToggleTraceWindow:
		txt = "Toggle Trace Window";
		break;
	case kModeAdcOverride:
		txt = "ADC overrides...";
		break;
	case kModeTime:
		txt = "Display time...";
		break;
	case kModeToggleLedInversion:
		txt = "Toggle LED Inversion";
		break;
	default:
		_ASSERTE(!"unhandled EngineModeSwitch");
		return;
	}

	mSwitchDisplay->SetSwitchText(switchNumber, txt);
	mSwitchDisplay->EnableDisplayUpdate(false);
	mSwitchDisplay->ForceSwitchDisplay(switchNumber, true);
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
			mSwitchDisplay->ForceSwitchDisplay(mPedalModePort, false);
		mPedalModePort = switchNumber;
		if (mSwitchDisplay)
			mSwitchDisplay->ForceSwitchDisplay(mPedalModePort, true);

		if (mMainDisplay)
		{
			std::strstream displayMsg;
			displayMsg << "ADC port " << (int) (mPedalModePort + 1) << " monitor" << std::endl << std::ends;
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
MidiControlEngine::SwitchReleased_NavAndDescMode(int switchNumber)
{
	if (switchNumber == mIncrementSwitchNumber)
	{
		// bank inc/dec does not commit bank
		NavigateBankRelative(1);
		mSwitchDisplay->ForceSwitchDisplay(mDecrementSwitchNumber, true);
		mSwitchDisplay->ForceSwitchDisplay(mIncrementSwitchNumber, true);
	}
	else if (switchNumber == mDecrementSwitchNumber)
	{
		// bank inc/dec does not commit bank
		NavigateBankRelative(-1);
		mSwitchDisplay->ForceSwitchDisplay(mDecrementSwitchNumber, true);
		mSwitchDisplay->ForceSwitchDisplay(mIncrementSwitchNumber, true);
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
		mSwitchDisplay->ForceSwitchDisplay(mDecrementSwitchNumber, true);
		mSwitchDisplay->ForceSwitchDisplay(mIncrementSwitchNumber, true);
	}
	else if (switchNumber == mIncrementSwitchNumber)
	{
		ChangeMode(emBankNav);
		mBankNavigationIndex = mActiveBankIndex;
		NavigateBankRelative(1);
		mSwitchDisplay->ForceSwitchDisplay(mDecrementSwitchNumber, true);
		mSwitchDisplay->ForceSwitchDisplay(mIncrementSwitchNumber, true);
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
		mSwitchDisplay->ForceSwitchDisplay(mDecrementSwitchNumber, true);
		mSwitchDisplay->ForceSwitchDisplay(mIncrementSwitchNumber, true);
	}
	else if (switchNumber == GetSwitchNumber(kModeBankDirect))
		ChangeMode(emBankDirect);
	else if (switchNumber == GetSwitchNumber(kModeProgramChangeDirect))
		ChangeMode(emProgramChangeDirect);
	else if (switchNumber == GetSwitchNumber(kModeControlChangeDirect))
		ChangeMode(emControlChangeDirect);
	else if (switchNumber == GetSwitchNumber(kModeExprPedalDisplay))
		ChangeMode(emExprPedalDisplay);
	else if (switchNumber == GetSwitchNumber(kModeToggleLedInversion))
	{
		if (mSwitchDisplay)
			mSwitchDisplay->InvertLeds(!mSwitchDisplay->IsInverted());
		EscapeToDefaultMode();
	}
	else if (switchNumber == GetSwitchNumber(kModeTestLeds))
	{
		if (mSwitchDisplay)
		{
			mSwitchDisplay->EnableDisplayUpdate(true);
			mSwitchDisplay->TestLeds();
		}
		EscapeToDefaultMode();
	}
	else if (switchNumber == GetSwitchNumber(kModeToggleTraceWindow))
	{
		if (mApplication)
			mApplication->ToggleTraceWindow();
	}
	else if (switchNumber == GetSwitchNumber(kModeTime))
		ChangeMode(emTimeDisplay);
	else if (switchNumber == GetSwitchNumber(kModeAdcOverride))
		ChangeMode(emAdcOverride);
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
			// commit
			ChangeMode(emBank);
			mBankNavigationIndex = mActiveBankIndex;
			const int bnkIdx = GetBankIndex(::atoi(mDirectNumber.c_str()));
			if (bnkIdx != -1)
				LoadBank(bnkIdx);
			else if (mMainDisplay)
				mMainDisplay->TextOut("Invalid bank number");
			updateMainDisplay = false;
		}
	}

	if (mSwitchDisplay)
	{
		mSwitchDisplay->EnableDisplayUpdate(false);
		mSwitchDisplay->ForceSwitchDisplay(switchNumber, false);
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
			PatchBank * bnk = GetBank(bnkIdx);
			_ASSERTE(bnk);
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
			// remove last char
			if (mDirectNumber.length())
				mDirectNumber = mDirectNumber.erase(mDirectNumber.length() - 1);
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
		if (bytes.size())
		{
			if (mMainDisplay)
			{
				const std::string byteDump("\r\n" + ::GetAsciiHexStr(bytes, true) + "\r\n");
				mMainDisplay->AppendText(byteDump);
			}
			mMidiOut->MidiOut(bytes);

			if (sJustDidProgramChange && mAxeMgr && mAxeMgr->GetAxeChannel() && mDirectChangeChannel)
				mAxeMgr->SyncFromAxe();
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
		mSwitchDisplay->ForceSwitchDisplay(switchNumber, false);
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
			// remove last char
			if (mDirectNumber.length())
				mDirectNumber = mDirectNumber.erase(mDirectNumber.length() - 1);
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
		if (bytes.size())
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
		mSwitchDisplay->ForceSwitchDisplay(switchNumber, false);
	}
}
