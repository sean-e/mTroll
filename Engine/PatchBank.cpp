#include <algorithm>
#include <strstream>
#include <iomanip>
#include "PatchBank.h"
#include "Patch.h"

#include "IMainDisplay.h"
#include "ISwitchDisplay.h"
#include "ITraceDisplay.h"


std::list<Patch *>	PatchBank::sActiveNormalPatches;


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
	sActiveNormalPatches.clear();
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
PatchBank::Load(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
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
				curItem->mPatch->SendStringA();
			else if (stB == curItem->mPatchStateAtLoad)
				curItem->mPatch->SendStringB();

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

	DisplayInfo(mainDisplay, switchDisplay, true, false);
}

void
PatchBank::Unload(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
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
				curItem->mPatch->SendStringA();
			else if (stB == curItem->mPatchStateAtUnload)
				curItem->mPatch->SendStringB();

			curItem->mPatch->ClearSwitch(switchDisplay);
		}
	}

//	mainDisplay->ClearDisplay();
}

void
PatchBank::PatchSwitchPressed(int switchNumber, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	PatchVect & curPatches = mPatches[switchNumber];
	PatchVect::iterator it;
	bool curSwitchHasNormalPatch = false;

	// if any patch for the switch is normal, then need to do normal processing
	// on current normal patches.
	for (it = curPatches.begin();
		 it != curPatches.end();
		 ++it)
	{
		PatchMap * curSwitchItem = *it;
		if (!curSwitchItem || !curSwitchItem->mPatch)
			continue;

		if (curSwitchItem->mPatch->GetPatchType() == Patch::ptNormal)
		{
			curSwitchHasNormalPatch = true;
			break;
		}
	}

	if (curSwitchHasNormalPatch)
	{
		// do B processing
		for (std::list<Patch*>::iterator it2 = sActiveNormalPatches.begin();
			it2 != sActiveNormalPatches.end();
			it2 = sActiveNormalPatches.begin())
		{
			Patch * curPatchItem = *it2;
			if (curPatchItem && curPatchItem->IsOn())
			{
				curPatchItem->SendStringB();
				curPatchItem->UpdateDisplays(mainDisplay, switchDisplay);
			}
			sActiveNormalPatches.erase(it2);
		}
	}

	// do standard pressed processing (send A)
	std::strstream msgstr;
	for (it = curPatches.begin();
		 it != curPatches.end();
		 ++it)
	{
		PatchMap * curSwitchItem = *it;
		if (!curSwitchItem || !curSwitchItem->mPatch)
			continue;

		curSwitchItem->mPatch->SwitchPressed(NULL, switchDisplay);
		if (curSwitchItem->mPatch->GetPatchType() == Patch::ptNormal)
		{
			_ASSERTE(std::find(sActiveNormalPatches.begin(), sActiveNormalPatches.end(), curSwitchItem->mPatch) == sActiveNormalPatches.end());
			sActiveNormalPatches.push_back(curSwitchItem->mPatch);
		}
		msgstr << curSwitchItem->mPatch->GetNumber() << " " << curSwitchItem->mPatch->GetName() << std::endl;
	}

	if (mainDisplay)
	{
		msgstr << std::ends;
		mainDisplay->TextOut(msgstr.str());
	}
}

void
PatchBank::PatchSwitchReleased(int switchNumber, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	PatchVect & curPatches = mPatches[switchNumber];
	for (PatchVect::iterator it = curPatches.begin();
		 it != curPatches.end();
		 ++it)
	{
		PatchMap * curSwitchItem = *it;
		if (!curSwitchItem || !curSwitchItem->mPatch)
			continue;

		curSwitchItem->mPatch->SwitchReleased(mainDisplay, switchDisplay);
	}
}

void
PatchBank::DisplayInfo(IMainDisplay * mainDisplay, 
					   ISwitchDisplay * switchDisplay,
					   bool showPatchInfo,
					   bool temporaryDisplay)
{
	std::ostrstream info;
	info << "Bank:   " << std::setw(2) << mNumber << " " << mName;

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
					info << "sw " << std::setw(2) << ((*it).first + 1) << ": " << std::setw(3) << curItem->mPatch->GetNumber() << " " << curItem->mPatch->GetName() << std::endl;
					if (temporaryDisplay)
						curItem->mPatch->AssignSwitch(-1, NULL);
				}
				else
				{
					info << "       " << std::setw(3) << curItem->mPatch->GetNumber() << " " << curItem->mPatch->GetName() << std::endl;
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
	info << "Status for bank " << std::setw(2) << mNumber << " '" << mName << "'" 
		<< std::endl << "switch " << std::setw(2) << (switchNumber + 1) << std::endl;

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
				info << "Num On/Off Type      Name" << std::endl;
			
// 			if (cnt == 1)
// 				info << "(Hidden patches)" << std::endl;

			info << std::setw(3) << curItem->mPatch->GetNumber() << " " 
				<< (curItem->mPatch->IsOn() ? "ON     " : "off    ") 
				<< std::setiosflags(std::ios::left) << std::setw(10) << curItem->mPatch->GetPatchTypeStr() 
				<< curItem->mPatch->GetName() << std::endl;
			++cnt;
		}

		break;
	}

	info << std::ends;
	if (mainDisplay)
		mainDisplay->TextOut(info.str());
}
