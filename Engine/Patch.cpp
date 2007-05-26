#include "Patch.h"

#include "IMidiOut.h"
#include "IMainDisplay.h"
#include "ISwitchDisplay.h"
#include "ITraceDisplay.h"


Patch * Patch::sCurrentNormalPatch = NULL;

Patch::Patch(int number, 
			 std::string name, 
			 PatchType patchType, 
			 byte * stringA, 
			 byte * stringB) : 
	mNumber(number),
	mName(name),
	mPatchType(patchType),
	mByteStringA(stringA),
	mByteStringB(stringB),
//	mMetaStringA(NULL),
//	mMetaStringB(NULL),
	mSwitchNumber(-1),
	mPatchIsOn(false)
{
}

Patch::~Patch()
{
	delete [] mByteStringA;
	delete [] mByteStringB;
// 	delete [] mMetaStringA;
// 	delete [] mMetaStringB;
}

void
Patch::AssignSwitch(int switchNumber, ISwitchDisplay * switchDisplay)
{
	mSwitchNumber = switchNumber;
	UpdateDisplays(NULL, switchDisplay);
}

void
Patch::ClearSwitch(ISwitchDisplay * switchDisplay)
{
	if (-1 == mSwitchNumber)
		return;

	if (switchDisplay)
		switchDisplay->SetSwitchDisplay(mSwitchNumber, false);

	mSwitchNumber = -1;
}

void
Patch::SwitchPressed(IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	if (mPatchIsOn)
	{
		SendStringB(midiOut);

		if (ptNormal == mPatchType)
			SendStringA(midiOut);

		UpdateDisplays(mainDisplay, switchDisplay);
	}
	else
	{
		if (ptNormal == mPatchType && 
			sCurrentNormalPatch && 
			sCurrentNormalPatch != this &&
			sCurrentNormalPatch->mPatchIsOn)
		{
			sCurrentNormalPatch->SendStringB(midiOut);
			sCurrentNormalPatch->UpdateDisplays(mainDisplay, switchDisplay);
		}

		SendStringA(midiOut);
		if (ptNormal == mPatchType)
			sCurrentNormalPatch = this;
		UpdateDisplays(mainDisplay, switchDisplay);
	}
}

void
Patch::SwitchReleased(IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	if (ptMomentary != mPatchType)
		return;

	SendStringB(midiOut);
	UpdateDisplays(mainDisplay, switchDisplay);
}

void
Patch::UpdateDisplays(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	if (-1 == mSwitchNumber)
		return;

	if (switchDisplay)
		switchDisplay->SetSwitchDisplay(mSwitchNumber, mPatchIsOn);
	if (mainDisplay)
		mainDisplay->TextOut();
}

void
Patch::SendStringA(IMidiOut * midiOut)
{
	midiOut->MidiOut(mByteStringA);
	mPatchIsOn = true;
}

void
Patch::SendStringB(IMidiOut * midiOut)
{
	midiOut->MidiOut(mByteStringB);
	mPatchIsOn = false;
}
