#include <algorithm>
#include <strstream>
#include "MidiControlEngine.h"
#include "PatchBank.h"
#include "IMainDisplay.h"
#include "ISwitchDisplay.h"
#include "ITraceDisplay.h"



template<typename T>
struct DeletePtr
{
	void operator()(const T * ptr)
	{
		delete ptr;
	}
};

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

const int kModeDefaultSwitchNumber = 0;
const int kModeBankNavSwitchNumber = 1;
const int kModeBankDescSwitchNumber = 2;
const int kModeBankDirect = 3;

MidiControlEngine::MidiControlEngine(IMidiOut * midiOut, 
									 IMainDisplay * mainDisplay, 
									 ISwitchDisplay * switchDisplay, 
									 ITraceDisplay * traceDisplay,
									 int incrementSwitchNumber,
									 int decrementSwitchNumber,
									 int modeSwitchNumber) :
	mMidiOut(midiOut),
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
	mFilterRedundantProgramChanges(false)
{
	mBanks.reserve(999);
}

MidiControlEngine::~MidiControlEngine()
{
	if (mMidiOut)
		mMidiOut->CloseMidiOut();
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
MidiControlEngine::AddPatch(int number,
							const std::string & name,
							Patch::PatchType patchType,
							const Bytes & stringA,
							const Bytes & stringB)
{
	mPatches[number] = new Patch(number, name, patchType, stringA, stringB);
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

// this would not need to exist if we could ensure that banks 
// were only added after all patches had been added (AddBank 
// would then need to maintain sort)
void
MidiControlEngine::CompleteInit()
{
	std::sort(mBanks.begin(), mBanks.end(), SortByBankNumber);

	int powerUpBankIndex = -1;

	int itIdx = 0;
	for (Banks::iterator it = mBanks.begin();
		it != mBanks.end();
		++it, ++itIdx)
	{
		PatchBank * curItem = *it;
		curItem->InitPatches(mPatches);

		if (curItem->GetBankNumber() == mPowerUpBank)
			powerUpBankIndex = itIdx;
	}

	ChangeMode(emBank);
	if (mMidiOut)
		mMidiOut->OpenMidiOut(0);
	LoadBank(powerUpBankIndex);

	if (mTrace)
	{
		std::strstream msg;
		msg << "Load complete: bank cnt " << mBanks.size() << ", patch cnt " << mPatches.size() << std::endl << std::ends;
		mTrace->Trace(std::string(msg.str()));
	}

}

void
MidiControlEngine::SwitchPressed(int switchNumber)
{
	if (mTrace)
	{
		std::strstream msg;
		msg << "SwitchPressed: " << switchNumber << std::endl << std::ends;
		mTrace->Trace(std::string(msg.str()));
	}

	if (emCreated == mMode ||
		emBankDesc == mMode ||
		emBankNav == mMode ||
		emModeSelect == mMode ||
		emBankDirect == mMode)
		return;

	if (emBank == mMode)
	{
		if (switchNumber == mIncrementSwitchNumber ||
			switchNumber == mDecrementSwitchNumber)
			ChangeMode(emBankNav);
		else if (switchNumber == mModeSwitchNumber)
			;
		else if (mActiveBank)
			mActiveBank->PatchSwitchPressed(switchNumber, mMidiOut, mMainDisplay, mSwitchDisplay);
		return;
	}
}

void
MidiControlEngine::SwitchReleased(int switchNumber)
{
	if (mTrace)
	{
		std::strstream msg;
		msg << "SwitchReleased: " << switchNumber << std::endl << std::ends;
		mTrace->Trace(msg.str());
	}

	if (emCreated == mMode)
		return;

	if (emModeSelect == mMode)
	{
		if (switchNumber == mIncrementSwitchNumber ||
			switchNumber == mDecrementSwitchNumber)
		{
			return;
		}
		else if (switchNumber == mModeSwitchNumber ||
			switchNumber == kModeDefaultSwitchNumber)
		{
			// escape
			ChangeMode(emBank);
			mBankNavigationIndex = mActiveBankIndex;
			NavigateBankRelative(0);
		}
		else if (switchNumber == kModeBankDescSwitchNumber)
		{
			ChangeMode(emBankDesc);
			mBankNavigationIndex = mActiveBankIndex;
			NavigateBankRelative(0);
		}
		else if (switchNumber == kModeBankNavSwitchNumber)
		{
			ChangeMode(emBankNav);
			mBankNavigationIndex = mActiveBankIndex;
			NavigateBankRelative(0);
		}
		else if (switchNumber == kModeBankDirect)
		{
			ChangeMode(emBankDirect);
		}

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
			// escape
			ChangeMode(emBank);
			mBankNavigationIndex = mActiveBankIndex;
			NavigateBankRelative(0);
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
				bank->DisplayInfo(mMainDisplay, mSwitchDisplay, true);
				bank->DisplayDetailedPatchInfo(switchNumber, mMainDisplay);
			}
		}

		return;
	}

	if (emBankDirect == mMode)
	{
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
			// escape
			ChangeMode(emBank);
			mBankNavigationIndex = mActiveBankIndex;
			NavigateBankRelative(0);
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
		}
		else if (mMainDisplay)
		{
			const int bnkIdx = GetBankIndex(::atoi(mBankDirectNumber.c_str()));
			if (bnkIdx == -1)
				mMainDisplay->TextOut(mBankDirectNumber);
			else
			{
				PatchBank * bnk = GetBank(bnkIdx);
				_ASSERTE(bnk);
				mMainDisplay->TextOut(mBankDirectNumber + " " + bnk->GetBankName());
			}
		}

		return;
	}

	if (emBank == mMode)
	{
		if (switchNumber == mIncrementSwitchNumber ||
			switchNumber == mDecrementSwitchNumber)
		{
			return;
		}

		if (switchNumber == mModeSwitchNumber)
		{
			ChangeMode(emModeSelect);
			return;
		}

		if (mActiveBank)
			mActiveBank->PatchSwitchReleased(switchNumber, mMidiOut, mMainDisplay, mSwitchDisplay);

		return;
	}
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

	// display bank info
	PatchBank * bank = GetBank(mBankNavigationIndex);
	if (!bank)
		return false;

	bank->DisplayInfo(mMainDisplay, mSwitchDisplay, true);
	return true;
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
		mActiveBank->Unload(mMidiOut, mMainDisplay, mSwitchDisplay);

	mActiveBank = bank;
	mBankNavigationIndex = mActiveBankIndex = bankIndex;
	mActiveBank->Load(mMidiOut, mMainDisplay, mSwitchDisplay);
	return true;
}

// possible mode transitions:
// emCreated -> emDefault
// emDefault -> emBankNav
// emDefault -> emModeSelect
// emModeSelect -> emDefault
// emModeSelect -> emBankNav
// emModeSelect -> emBankDesc
// emModeSelect -> emBankDirect
// emBankNav -> emDefault
// emBankDesc -> emDefault
// emBankDirect -> emDefault
void
MidiControlEngine::ChangeMode(EngineMode newMode)
{
	mMode = newMode;

	if (mSwitchDisplay)
	{
		for (int idx = 0; idx < 32; idx++)
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
		if (mSwitchDisplay)
		{
			mSwitchDisplay->SetSwitchText(mIncrementSwitchNumber, "Next Bank");
			mSwitchDisplay->SetSwitchText(mDecrementSwitchNumber, "Prev Bank");
		}

		if (mActiveBank)
		{
			mActiveBank->DisplayInfo(mMainDisplay, mSwitchDisplay, true);
			showModeInMainDisplay = false;
		}
		break;
	case emBankNav:
		msg = "Bank Navigation";
		if (mSwitchDisplay)
		{
			mSwitchDisplay->SetSwitchText(mIncrementSwitchNumber, "Next Bank");
			mSwitchDisplay->SetSwitchText(mDecrementSwitchNumber, "Prev Bank");
		}
		break;
	case emBankDesc:
		msg = "Bank and Switch Description";
		if (mSwitchDisplay)
		{
			mSwitchDisplay->SetSwitchText(mIncrementSwitchNumber, "Next Bank");
			mSwitchDisplay->SetSwitchText(mDecrementSwitchNumber, "Prev Bank");
		}
		break;
	case emModeSelect:
		msg = "Mode Select";
		if (mSwitchDisplay)
		{
			mSwitchDisplay->SetSwitchText(kModeDefaultSwitchNumber, "Bank");
			mSwitchDisplay->SetSwitchText(kModeBankNavSwitchNumber, "Bank Nav");
			mSwitchDisplay->SetSwitchText(kModeBankDescSwitchNumber, "Bank Desc");
			mSwitchDisplay->SetSwitchText(kModeBankDirect, "Bank Direct");
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
		}
		break;
	default:
		msg = "Invalid";
	    break;
	}

	if (showModeInMainDisplay && mMainDisplay)
		mMainDisplay->TextOut("mode: " +  msg);

	if (mSwitchDisplay)
	{
		mSwitchDisplay->SetSwitchText(mModeSwitchNumber, msg);
		mSwitchDisplay->SetSwitchDisplay(mModeSwitchNumber, mMode == emBank ? true : false);
	}
}
