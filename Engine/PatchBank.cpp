#include <algorithm>
#include <strstream>
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

template<typename T>
struct DeletePtr
{
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
	curPatches.push_back(new PatchMap(patchNumber, patchLoadState, patchUnloadState));
}

void
PatchBank::InitPatches(const MidiControlEngine::Patches & enginePatches)
{
	for (PatchMaps::iterator it = mPatches.begin();
		it != mPatches.end();
		++it)
	{
		PatchVect & patches = (*it).second;
		for (PatchVect::iterator it2 = patches.begin();
			 it2 != patches.end();
			 ++it2)
		{
			PatchMap * curItem = *it2;
			if (curItem)
			{
				MidiControlEngine::Patches::const_iterator patchIt = enginePatches.find(curItem->mPatchNumber);
				Patch * thePatch = (*patchIt).second;
				curItem->mPatch = thePatch;
			}
		}
	}
}

void
PatchBank::Load(IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	for (PatchMaps::iterator it = mPatches.begin();
		it != mPatches.end();
		++it)
	{
		bool once = true;
		PatchVect & patches = (*it).second;
		for (PatchVect::iterator it2 = patches.begin();
			 it2 != patches.end();
			 ++it2)
		{
			PatchMap * curItem = *it2;
			if (!curItem || !curItem->mPatch)
				continue;

			if (stA == curItem->mPatchStateAtLoad)
				curItem->mPatch->SendStringA(midiOut);
			else if (stB == curItem->mPatchStateAtLoad)
				curItem->mPatch->SendStringB(midiOut);

			// only assign a switch to the first patch (if multiple 
			// patches are assigned to the same switch)
			if (once)
			{
				once = false;
				curItem->mPatch->AssignSwitch((*it).first, switchDisplay);
			}
			else
				curItem->mPatch->AssignSwitch(-1, NULL);
		}
	}

	DisplayInfo(mainDisplay, switchDisplay, !switchDisplay->SupportsSwitchText());
}

void
PatchBank::Unload(IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	for (PatchMaps::iterator it = mPatches.begin();
		it != mPatches.end();
		++it)
	{
		PatchVect & patches = (*it).second;
		for (PatchVect::iterator it2 = patches.begin();
			 it2 != patches.end();
			 ++it2)
		{
			PatchMap * curItem = *it2;
			if (!curItem || !curItem->mPatch)
				continue;

			if (stA == curItem->mPatchStateAtUnload)
				curItem->mPatch->SendStringA(midiOut);
			else if (stB == curItem->mPatchStateAtUnload)
				curItem->mPatch->SendStringB(midiOut);

			curItem->mPatch->ClearSwitch(switchDisplay);
		}
	}

	mainDisplay->ClearDisplay();
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
PatchBank::DisplayInfo(IMainDisplay * mainDisplay, 
					   ISwitchDisplay * switchDisplay,
					   bool showPatchInfo)
{
	std::ostrstream info;
	info << "Bank: " << mNumber << " " << mName;

	if (showPatchInfo)
	{
		info << std::endl;
		for (PatchMaps::iterator it = mPatches.begin();
			it != mPatches.end();
			++it)
		{
			bool once = true;
			PatchVect & patches = (*it).second;
			for (PatchVect::iterator it2 = patches.begin();
				it2 != patches.end();
				++it2)
			{
				PatchMap * curItem = *it2;
				if (!curItem || !curItem->mPatch)
					continue;

				if (once)
				{
					once = false;
					curItem->mPatch->AssignSwitch((*it).first, switchDisplay);
					info << "sw " << (*it).first << ":\t" << curItem->mPatch->GetNumber() << " " << curItem->mPatch->GetName() << std::endl;
				}
				else
				{
					info << "\t(" << curItem->mPatch->GetNumber() << " " << curItem->mPatch->GetName() << ")" << std::endl;
				}
			}
		}
	}

	info << std::ends;
	if (mainDisplay)
		mainDisplay->TextOut(info.str());
}

void
PatchBank::DisplayDetailedPatchInfo(int switchNumber, IMainDisplay * mainDisplay)
{
	std::ostrstream info;
	info << "Status for bank " << mNumber << " '" << mName << "' switch " << switchNumber << std::endl;

	for (PatchMaps::iterator it = mPatches.begin();
		it != mPatches.end();
		++it)
	{
		if ((*it).first != switchNumber)
			continue;

		int cnt = 0;
		PatchVect & patches = (*it).second;
		for (PatchVect::iterator it2 = patches.begin();
			it2 != patches.end();
			++it2)
		{
			PatchMap * curItem = *it2;
			if (!curItem || !curItem->mPatch)
				continue;

			if (cnt == 0)
				info << "\tNum\tOn/Off\tName" << std::endl;
			
			if (cnt == 1)
				info << "(Hidden patches)" << std::endl;

			info << "\t" << curItem->mPatch->GetNumber() << "\t" << (curItem->mPatch->IsOn() ? "on" : "off") << "\t" << curItem->mPatch->GetName() << std::endl;
			++cnt;
		}

		break;
	}

	info << std::ends;
	if (mainDisplay)
		mainDisplay->TextOut(info.str());
}
