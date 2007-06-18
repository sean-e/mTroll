#ifndef ISwitchDisplay_h__
#define ISwitchDisplay_h__

#include <string>


// ISwitchDisplay
// ----------------------------------------------------------------------------
// use to set / clear LEDs associated with the switches.
//
class ISwitchDisplay
{
public:
	virtual void SetSwitchDisplay(int switchNumber, bool isOn) = 0;
	virtual void SetSwitchText(int switchNumber, const std::string & txt) = 0;
	virtual void ClearSwitchText(int switchNumber) = 0;
};

#endif // ISwitchDisplay_h__
