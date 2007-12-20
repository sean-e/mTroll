#include "Patch.h"
#include <strstream>
#include "IMidiOut.h"
#include "IMainDisplay.h"
#include "ISwitchDisplay.h"
#include "ITraceDisplay.h"


ExpressionPedals * gActivePatchPedals = NULL;

Patch::Patch(int number, 
			 const std::string & name,
			 int midiOutPortNumber /*= -1*/, 
			 IMidiOut * midiOut /*= NULL*/) :
	mNumber(number),
	mName(name),
	mPatchIsActive(false),
	mMidiOutPort(midiOutPortNumber),
	mMidiOut(midiOut),
	mPedals(midiOut)
{
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
			switchDisplay->SetSwitchDisplay(*it, mPatchIsActive);
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
Patch::Activate(IMainDisplay * mainDisplay, 
				ISwitchDisplay * switchDisplay)
{
	if (IsActive())
		return;

	SwitchPressed(mainDisplay, switchDisplay);
}

void
Patch::Deactivate(IMainDisplay * mainDisplay, 
				  ISwitchDisplay * switchDisplay)
{
	if (!IsActive())
		return;

	SwitchPressed(mainDisplay, switchDisplay);
}
