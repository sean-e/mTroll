#include <algorithm>
#include "MidiControlEngine.h"
#include "PatchBank.h"
#include "IMainDisplay.h"
#include "ISwitchDisplay.h"
#include "ITraceDisplay.h"



struct DeletePtr
{
	template<typename T>
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
SortByBankNumber(const PatchBank* & lhs, const PatchBank* & rhs)
{
	return lhs->mNumber < rhs->mNumber;
}


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
	std::for_each(mBanks.begin(), mBanks.end(), DeletePtr<PatchBank>());
	mBanks.clear();
	std::for_each(mPatches.begin(), mPatches.end(), DeletePatch());
	mPatches.clear();
}

PatchBank &
MidiControlEngine::AddBank(int number,
						   std::string name)
{
	PatchBank * pBank = new PatchBank(number, name);
	mBanks.push_back(pBank);
	return * pBank;
}

void
MidiControlEngine::AddPatch(int number,
							std::string name,
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
	std::sort(mBanks.begin, mBanks.end(), SortByBankNumber);

	for (Banks::iterator it = mBanks.begin();
		it != mBanks.end();
		++it)
	{
		PatchBank * curItem = *it;
		curItem->InitPatches(mPatches);
	}

	LoadBank(mPowerUpBank);
	mMode = emDefault;
}

void
MidiControlEngine::SwitchPressed(int switchNumber)
{
	if (emCreated == mMode)
		return;

	switch (switchNumber)
	{
	case mIncrementSwitchNumber:
		if (emDefault == mMode)
			mMode = emBankNav;
		return;
	case mDecrementSwitchNumber:
		if (emDefault == mMode)
			mMode = emBankNav;
		return;
	case mModeSwitchNumber:
		return;
	default:
		if (emDefault == mMode)
		{
			if (mActiveBank)
				mActiveBank->PatchSwitchPressed(switchNumber, mMidiOut, mMainDisplay, mSwitchDisplay);
		}
	    break;
	}
}

void
MidiControlEngine::SwitchReleased(int switchNumber)
{
	if (emCreated == mMode)
		return;

	if (emBankNav == mMode)
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
			// escape bank nav
			mBankNavigationIndex = mActiveBankIndex;
			NavigateBankRelative(0);
			// go to next mode
			NextMode(false);
		}
		else
		{
			// any switch release (except inc/dec/util) after bank inc/dec commits bank
			LoadBank(mBankNavigationIndex);
			// reset to default mode
			mMode = emDefault;
		}
		return;
	}

	switch (switchNumber)
	{
	case mIncrementSwitchNumber:
	case mDecrementSwitchNumber:
		return;
	case mModeSwitchNumber:
		NextMode(true);
		return;
	default:
		if (emDefault == mMode)
		{
			if (mActiveBank)
				mActiveBank->PatchSwitchReleased(switchNumber, mMidiOut, mMainDisplay, mSwitchDisplay);
		}
	    break;
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

	bank->DisplayInfo(mMainDisplay);
	return true;
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

void
MidiControlEngine::NextMode(bool displayMode)
{
	if (emNotValid == ++mMode)
		mMode = emDefault;

	if (!displayMode)
		return;

	std::string msg;
	switch (mMode)
	{
	case emDefault:
		msg = "mode: Default\n";
		break;
	case emBankNav:
		msg = "mode: Bank\n";
		break;
	default:
		msg = "mode: Invalid\n";
	    break;
	}

	mMainDisplay->TextOut(msg);
}
