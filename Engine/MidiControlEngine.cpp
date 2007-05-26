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
	mInMeta(false),
	mActiveBank(NULL),
	mActiveBankIndex(0),
	mPowerUpTimeout(0),
	mPowerUpBank(-1),
	mPowerUpPatch(-1),
	mIncrementSwitchNumber(14),
	mDecrementSwitchNumber(15),
	mUtilSwitchNumber(16),
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

void
MidiControlEngine::SwitchPressed(int switchNumber)
{
	if (switchNumber == mIncrementSwitchNumber)
		LoadBankRelative(1);
	else if (switchNumber == mDecrementSwitchNumber)
		LoadBankRelative(-1);
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
	if (switchNumber == mIncrementSwitchNumber)
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

void
MidiControlEngine::LoadBankRelative(int relativeBankIndex)
{
	const int kBankCnt = mBanks.size();
	if (!kBankCnt)
		return;

	int newBankIndex = mActiveBankIndex + relativeBankIndex;
	if (newBankIndex < 0)
		newBankIndex = kBankCnt - 1;
	if (newBankIndex >= kBankCnt)
		newBankIndex = 0;

	PatchBank * bank = mBanks[newBankIndex];
	if (!bank)
		return;

	mActiveBank->Unload(mMidiOut, mMainDisplay, mSwitchDisplay);
	mActiveBank = bank;
	mActiveBankIndex = newBankIndex;
	mActiveBank->Load(mMidiOut, mMainDisplay, mSwitchDisplay);
}
