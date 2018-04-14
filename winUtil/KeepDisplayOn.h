/*
Original code copyright (c) 2007-2008,2018 Sean Echevarria ( http://www.creepingfog.com/sean/ )

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

#ifndef KeepDisplayOn_h__
#define KeepDisplayOn_h__

#include <Windows.h>

// KeepDisplayOn 
// -----------------------------------------------------------------------------
// This class should be used sparingly.  It violates the policies set in place
// by the user via the Control Panel.  This application sets aside the 
// policies because:
//	1) it is used as a non-interactive status monitor of external hardware
//	2) external events that arrive via the USB port are not considered by the OS
//		when resetting the screensaver and sleep mode system timers.
//
// An alternative to using this class would be to create and use alternative 
// policies; but for me personally, use of this class is preferable.
//
// see the following references:
// http://msdn2.microsoft.com/en-us/library/aa373208.aspx
// http://support.microsoft.com/kb/q126627/
// http://softwarecommunity.intel.com/articles/eng/2667.htm
//

class KeepDisplayOn
{
public:
	KeepDisplayOn()
	{
		mPrevExecState = ::SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED | ES_CONTINUOUS);
		::SystemParametersInfo(SPI_GETSCREENSAVETIMEOUT, 0, &mPrevScreenSaver, 0);
		::SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, FALSE, nullptr, 0);
	}

	~KeepDisplayOn()
	{
		::SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, mPrevScreenSaver, nullptr, 0);
		::SetThreadExecutionState(mPrevExecState);
	}

private:
	UINT				mPrevScreenSaver;
	EXECUTION_STATE		mPrevExecState;
};

#endif // KeepDisplayOn_h__
