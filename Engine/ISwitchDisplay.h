#pragma once

// ISwitchDisplay
// ----------------------------------------------------------------------------
// use to set / clear LEDs associated with the switches.
//
class ISwitchDisplay
{
public:
	virtual void SetSwitchDisplay(int switchNumber, bool isOn) = 0;
};
