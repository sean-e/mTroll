#ifndef IInput_h__
#define IInput_h__

// IMonome40hInputSubscriber
// ----------------------------------------------------------------------------
// Implement to get notification of switch press and release, and
// value change of analog port
//
class IMonome40hInputSubscriber
{
public:
	// input address 0 is button press/release
	virtual void SwitchPressed(int switchNumber) = 0;
	virtual void SwitchReleased(int switchNumber) = 0;
	// input address 1 is adc
	virtual void AdcValueChanged(int port, int curValue) = 0;
};

#endif // IInput_h__
