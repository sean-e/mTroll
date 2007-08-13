#include "Patch.h"
#include <strstream>
#include "IMidiOut.h"
#include "IMainDisplay.h"
#include "ISwitchDisplay.h"
#include "ITraceDisplay.h"


ExpressionPedals * gActivePatchPedals = NULL;

Patch::Patch(int number, 
			 const std::string & name, 
			 PatchType patchType, 
			 int midiOutPortNumber,
			 IMidiOut * midiOut,
			 const std::vector<byte> & midiStringA, 
			 const std::vector<byte> & midiStringB) : 
	mNumber(number),
	mName(name),
	mPatchType(patchType),
	mMidiByteStringA(midiStringA),
	mMidiByteStringB(midiStringB),
	mPatchIsOn(false),
	mMidiOutPort(midiOutPortNumber),
	mMidiOut(midiOut),
	mPedals(midiOut)
{
	_ASSERTE(midiOut);
}

Patch::~Patch()
{
}

void
Patch::AssignSwitch(int switchNumber, ISwitchDisplay * switchDisplay)
{
	_ASSERTE(switchNumber != -1 && switchNumber >= 0);
	mSwitchNumbers.push_back(switchNumber);
	if (switchDisplay)
		switchDisplay->SetSwitchText(switchNumber, mName);
	UpdateDisplays(NULL, switchDisplay);
}

void
Patch::ClearSwitch(ISwitchDisplay * switchDisplay)
{
	if (mSwitchNumbers.empty())
		return;

	if (switchDisplay)
	{
		for (std::vector<int>::iterator it = mSwitchNumbers.begin(); 
			it != mSwitchNumbers.end();
			++it)
		{
			switchDisplay->SetSwitchDisplay(*it, false);
			switchDisplay->ClearSwitchText(*it);
		}
	}

	mSwitchNumbers.clear();
}

void
Patch::SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	if (mPatchIsOn)
	{
		SendStringB();

		if (ptNormal == mPatchType)
			SendStringA();
	}
	else
	{
		SendStringA();
	}

	UpdateDisplays(mainDisplay, switchDisplay);
}

void
Patch::SwitchReleased(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	if (ptMomentary != mPatchType)
		return;

	SendStringB();
	UpdateDisplays(mainDisplay, switchDisplay);
}

void
Patch::UpdateDisplays(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
{
	if (mSwitchNumbers.empty())
		return;

	if (switchDisplay)
	{
		for (std::vector<int>::iterator it = mSwitchNumbers.begin(); 
			it != mSwitchNumbers.end();
			++it)
		{
			switchDisplay->SetSwitchDisplay(*it, mPatchIsOn);
		}
	}

	if (mainDisplay)
	{
		std::strstream msgstr;
		msgstr << mNumber << " " << mName << std::endl << std::ends;
		mainDisplay->TextOut(msgstr.str());
	}
}

void
Patch::SendStringA()
{
	if (mMidiByteStringA.size())
		mMidiOut->MidiOut(mMidiByteStringA);
	mPatchIsOn = true;

	if (ptNormal == mPatchType)
	{
		// do this here rather than SwitchPressed to that pedals can be
		// set on bank load rather than only patch load
		gActivePatchPedals = &mPedals;
	}
}

void
Patch::SendStringB()
{
	if (mMidiByteStringB.size())
		mMidiOut->MidiOut(mMidiByteStringB);
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
