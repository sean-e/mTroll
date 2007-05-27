#ifndef IInput_h__
#define IInput_h__

// IInput
// ----------------------------------------------------------------------------
// Implement to get notification of pedal press and switch
//
class IInput
{
public:
	virtual void SwitchPressed(int switchNumber) = 0;
	virtual void SwitchReleased(int switchNumber) = 0;
};

#endif // IInput_h__
