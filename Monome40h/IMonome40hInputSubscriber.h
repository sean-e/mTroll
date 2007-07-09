#ifndef IInput_h__
#define IInput_h__

typedef unsigned char byte;


// IMonome40hInputSubscriber
// ----------------------------------------------------------------------------
// Implement to get notification of switch press and release, and
// value change of analog port
//
class IMonome40hInputSubscriber
{
public:
	virtual void SwitchPressed(byte row, byte column) = 0;
	virtual void SwitchReleased(byte row, byte column) = 0;
	virtual void AdcValueChanged(int port, int curValue) = 0;
};

#endif // IInput_h__
