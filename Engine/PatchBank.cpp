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
	std::for_each(mPatches.begin(), mPatches.end(), DeletePatchMap());
	mPatches.clear();
}

void
PatchBank::AddPatchMapping(int switchNumber, int patchNumber, PatchState patchLoadState, PatchState patchUnloadState)
{
	mPatches[switchNumber] = new PatchMap(patchNumber, patchLoadState, patchUnloadState, NULL);
}

void
PatchBank::InitPatches(const MidiControlEngine::Patches & patches)
{
	for (PatchMaps::iterator it = mPatches.begin();
		it != mPatches.end();
		++it)
	{
		PatchMap * curItem = (*it).second();
		if (curItem)
		{
			Patch * thePatch = patches[curItem->mPatchNumber];
			curItem->mPatch = thePatch;
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
		PatchMap * curItem = (*it).second();
		if (!curItem || !curItem->mPatch)
			continue;

		if (PatchState::stA == curItem->mPatchStateAtLoad)
			curItem->mPatch->SendStringA(midiOut);
		else if (PatchState::stB == curItem->mPatchStateAtLoad)
			curItem->mPatch->SendStringB(midiOut);

		curItem->mPatch->AssignSwitch((*it).first(), switchDisplay);
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
		PatchMap * curItem = (*it).second();
		if (!curItem || !curItem->mPatch)
			continue;

		if (PatchState::stA == curItem->mPatchStateAtUnload)
			curItem->mPatch->SendStringA(midiOut);
		else if (PatchState::stB == curItem->mPatchStateAtUnload)
			curItem->mPatch->SendStringB(midiOut);

		curItem->mPatch->ClearSwitch(switchDisplay);
	}

	mainDisplay->Clear();
}

void
PatchBank::PatchSwitchPressed(int switchNumber, IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	if (switchNumber >= mPatches.size())
		return;

	PatchMap * switchItem = mPatches[switchNumber];
	if (!switchItem || !switchItem->mPatch)
		return;

	switchItem->mPatch->SwitchPressed(midiOut, mainDisplay, switchDisplay);
}

void
PatchBank::PatchSwitchReleased(int switchNumber, IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	if (switchNumber >= mPatches.size())
		return;

	PatchMap * switchItem = mPatches[switchNumber];
	if (!switchItem || !switchItem->mPatch)
		return;

	switchItem->mPatch->SwitchReleased(midiOut, mainDisplay, switchDisplay);
}

void
PatchBank::DisplayInfo(IMainDisplay * mainDisplay)
{
	mainDisplay->TextOut("Bank: " + mNumber + " " + mName);
}
