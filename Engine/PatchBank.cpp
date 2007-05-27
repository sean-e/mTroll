#include <algorithm>
#include "PatchBank.h"
#include "Patch.h"

#include "IMainDisplay.h"
#include "ISwitchDisplay.h"
#include "ITraceDisplay.h"


PatchBank::PatchBank(int number, 
					 const std::string & name) :
	mNumber(number),
	mName(name)
{
}

PatchBank::~PatchBank()
{
	std::for_each(mPatches.begin(), mPatches.end(), DeletePatchMaps());
	mPatches.clear();
}

struct DeletePtr
{
	template<typename T>
	void operator()(const T * ptr)
	{
		delete ptr;
	}
};

void PatchBank::DeletePatchMaps::operator()(const std::pair<int, PatchVect> & pr)
{
	std::for_each(pr.second.begin(), pr.second.end(), DeletePtr<PatchMap>());
}

void
PatchBank::AddPatchMapping(int switchNumber, int patchNumber, PatchState patchLoadState, PatchState patchUnloadState)
{
	PatchVect & curPatches = mPatches[switchNumber];
	curPatches.push_back(new PatchMap(patchNumber, patchLoadState, patchUnloadState, NULL));
}

void
PatchBank::InitPatches(const MidiControlEngine::Patches & patches)
{
	for (PatchMaps::iterator it = mPatches.begin();
		it != mPatches.end();
		++it)
	{
		PatchVect & patches = (*it).second();
		for (PatchVect::iterator it2 = patches.begin();
			 it2 != patches.end();
			 ++it2)
		{
			PatchMap * curItem = *it2;
			if (curItem)
			{
				Patch * thePatch = patches[curItem->mPatchNumber];
				curItem->mPatch = thePatch;
			}
		}
	}
}

bool
PatchBank::Load(IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	for (PatchMaps::iterator it = mPatches.begin();
		it != mPatches.end();
		++it)
	{
		bool once = true;
		PatchVect & patches = (*it).second();
		for (PatchVect::iterator it2 = patches.begin();
			 it2 != patches.end();
			 ++it2)
		{
			PatchMap * curItem = *it2;
			if (!curItem || !curItem->mPatch)
				continue;

			if (PatchState::stA == curItem->mPatchStateAtLoad)
				curItem->mPatch->SendStringA(midiOut);
			else if (PatchState::stB == curItem->mPatchStateAtLoad)
				curItem->mPatch->SendStringB(midiOut);

			// only assign a switch to the first patch (if multiple 
			// patches are assigned to the same switch)
			if (once)
			{
				curItem->mPatch->AssignSwitch((*it).first(), switchDisplay);
				once = false;
			}
		}
	}

	DisplayInfo(mainDisplay);
}

void
PatchBank::Unload(IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	for (PatchMaps::iterator it = mPatches.begin();
		it != mPatches.end();
		++it)
	{
		PatchVect & patches = (*it).second();
		for (PatchVect::iterator it2 = patches.begin();
			 it2 != patches.end();
			 ++it2)
		{
			PatchMap * curItem = *it2;
			if (!curItem || !curItem->mPatch)
				continue;

			if (PatchState::stA == curItem->mPatchStateAtUnload)
				curItem->mPatch->SendStringA(midiOut);
			else if (PatchState::stB == curItem->mPatchStateAtUnload)
				curItem->mPatch->SendStringB(midiOut);

			curItem->mPatch->ClearSwitch(switchDisplay);
		}
	}

	mainDisplay->Clear();
}

void
PatchBank::PatchSwitchPressed(int switchNumber, IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	PatchSwitchAction(true, switchNumber, midiOut, mainDisplay, switchDisplay);
}

void
PatchBank::PatchSwitchReleased(int switchNumber, IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	PatchSwitchAction(false, switchNumber, midiOut, mainDisplay, switchDisplay);
}

void
PatchBank::PatchSwitchAction(bool pressed, int switchNumber, IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	PatchVect & curPatches = mPatches[switchNumber];
	for (PatchVect::iterator it = curPatches.begin();
		 it != curPatches.end();
		 ++it)
	{
		PatchMap * curSwitchItem = *it;
		if (!curSwitchItem || !curSwitchItem->mPatch)
			continue;

		if (pressed)
			curSwitchItem->mPatch->SwitchPressed(midiOut, mainDisplay, switchDisplay);
		else
			curSwitchItem->mPatch->SwitchReleased(midiOut, mainDisplay, switchDisplay);
	}
}

void
PatchBank::DisplayInfo(IMainDisplay * mainDisplay)
{
	mainDisplay->TextOut("Bank: " + mNumber + " " + mName);
}
