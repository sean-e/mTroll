/*
Original code copyright (c) 2007 Sean Echevarria ( http://www.creepingfog.com/sean/ )

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/

#ifndef IMonomeInput_h__
#define IMonomeInput_h__

typedef unsigned char byte;


// IMonome40hSwitchSubscriber
// ----------------------------------------------------------------------------
// Implement to get notification of switch press and release
//
class IMonome40hSwitchSubscriber
{
public:
	virtual void SwitchPressed(byte row, byte column) = 0;
	virtual void SwitchReleased(byte row, byte column) = 0;
};

// IMonome40hAdcSubscriber
// ----------------------------------------------------------------------------
// Implement to get value changes of analog ports
//
class IMonome40hAdcSubscriber
{
public:
	virtual void AdcValueChanged(int port, int curValue) = 0;
};

#endif // IMonomeInput_h__
