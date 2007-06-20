#include "Patch.h"
#include <strstream>
#include "IMidiOut.h"
#include "IMainDisplay.h"
#include "ISwitchDisplay.h"
#include "ITraceDisplay.h"


Patch::Patch(int number, 
			 const std::string & name, 
			 PatchType patchType, 
			 const std::vector<byte> & stringA, 
			 const std::vector<byte> & stringB) : 
	mNumber(number),
	mName(name),
	mPatchType(patchType),
	mByteStringA(stringA),
	mByteStringB(stringB),
	mSwitchNumber(-1),
	mPatchIsOn(false)
{
}

Patch::~Patch()
{
}

void
Patch::AssignSwitch(int switchNumber, ISwitchDisplay * switchDisplay)
{
	mSwitchNumber = switchNumber;
	if (switchDisplay)
		switchDisplay->SetSwitchText(mSwitchNumber, mName);
	UpdateDisplays(NULL, switchDisplay);
}

void
Patch::ClearSwitch(ISwitchDisplay * switchDisplay)
{
	if (-1 == mSwitchNumber)
		return;

	if (switchDisplay)
	{
		switchDisplay->SetSwitchDisplay(mSwitchNumber, false);
		switchDisplay->ClearSwitchText(mSwitchNumber);
	}

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
	}
	else
	{
		SendStringA(midiOut);
	}

	UpdateDisplays(mainDisplay, switchDisplay);
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
	{
		std::strstream msgstr;
		msgstr << mNumber << " " << mName << std::endl << std::ends;
		mainDisplay->TextOut(msgstr.str());
	}
}

void
Patch::SendStringA(IMidiOut * midiOut)
{
	if (mByteStringA.size())
		midiOut->MidiOut(mByteStringA);
	mPatchIsOn = true;
}

void
Patch::SendStringB(IMidiOut * midiOut)
{
	if (mByteStringB.size())
		midiOut->MidiOut(mByteStringB);
	mPatchIsOn = false;
}

std::string
Patch::GetPatchTypeStr() const
{
	std::string retval;
	switch (mPatchType)
	{
	case ptNormal:
		retval = "normal";
		break;
	case ptToggle:
		retval = "toggle";
		break;
	case ptMomentary:
		retval = "momentary";
		break;
	default:
		retval = "unknown";
	}

	return retval;
}
