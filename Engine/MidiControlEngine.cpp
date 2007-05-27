#include <algorithm>
#include "MidiControlEngine.h"
#include "PatchBank.h"
#include "IMainDisplay.h"
#include "ISwitchDisplay.h"
#include "ITraceDisplay.h"



MidiControlEngine::MidiControlEngine(IMidiOut * midiOut, 
									 IMainDisplay * mainDisplay, 
									 ISwitchDisplay * switchDisplay, 
									 ITraceDisplay * traceDisplay) :
	mMidiOut(midiOut),
	mMainDisplay(mainDisplay),
	mSwitchDisplay(switchDisplay),
	mTrace(traceDisplay),
	mProcessSwitches(false),
	mInMeta(false),
	mInBankNavigation(false),
	mActiveBank(NULL),
	mActiveBankIndex(0),
	mBankNavigationIndex(0),
	mPowerUpTimeout(0),
	mPowerUpBank(0),
	mPowerUpPatch(-1),
	mIncrementSwitchNumber(13),
	mDecrementSwitchNumber(14),
	mUtilSwitchNumber(15),
	mFilterRedundantProgramChanges(false)
{
	mBanks.reserve(999);
}

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

static bool
SortByBankNumber(const PatchBank* & lhs, const PatchBank* & rhs)
{
	return lhs->mNumber < rhs->mNumber;
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
	mProcessSwitches = true;
}

void
MidiControlEngine::SwitchPressed(int switchNumber)
{
	if (!mProcessSwitches)
		return;

	if (switchNumber == mIncrementSwitchNumber)
	{
		if (mInMeta)
			;
		else
		{
			// bank inc/dec does not commit bank
			NavigateBankRelative(1);
		}
	}
	else if (switchNumber == mDecrementSwitchNumber)
	{
		if (mInMeta)
			;
		else
		{
			// bank inc/dec does not commit bank
			NavigateBankRelative(-1);
		}
	}
	else if (mInBankNavigation)
		; // check before util switch so that meta state is not entered
	else if (switchNumber == mUtilSwitchNumber)
	{
		if (mInMeta)
			mInMeta = false;
		else
			mInMeta = true;
	}
	else if (mInMeta)
		;
	else if (mActiveBank)
		mActiveBank->PatchSwitchPressed(switchNumber, mMidiOut, mMainDisplay, mSwitchDisplay);
	else if (mTrace)
		mTrace->TextOut("SwitchPressed: no bank loaded");
}

void
MidiControlEngine::SwitchReleased(int switchNumber)
{
	if (!mProcessSwitches)
		return;

	if (mInBankNavigation)
	{
		if (switchNumber == mIncrementSwitchNumber)
			; // handled in SwitchPressed
		else if (switchNumber == mDecrementSwitchNumber)
			; // handled in SwitchPressed
		else if (switchNumber == mUtilSwitchNumber)
		{
			// escape bank nav
			mBankNavigationIndex = mActiveBankIndex;
			NavigateBankRelative(0);
			mInBankNavigation = false;
		}
		else
		{
			// any switch release (except inc/dec/util) after bank inc/dec commits bank
			LoadBank(mBankNavigationIndex);
			mInBankNavigation = false;
		}
	}
	else if (switchNumber == mIncrementSwitchNumber)
		;
	else if (switchNumber == mDecrementSwitchNumber)
		;
	else if (switchNumber == mUtilSwitchNumber)
		;
	else if (mInMeta)
		;
	else if (mActiveBank)
		mActiveBank->PatchSwitchReleased(switchNumber, mMidiOut, mMainDisplay, mSwitchDisplay);
	else if (mTrace)
		mTrace->TextOut("SwitchReleased: no bank loaded");
}

bool
MidiControlEngine::NavigateBankRelative(int relativeBankIndex)
{
	// bank inc/dec does not commit bank
	const int kBankCnt = mBanks.size();
	if (!kBankCnt || kBankCnt == 1)
		return false;

	mInBankNavigation = true;
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
