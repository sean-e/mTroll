#pragma once

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
		::SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, FALSE, NULL, 0);
	}

	~KeepDisplayOn()
	{
		::SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, mPrevScreenSaver, NULL, 0);
		::SetThreadExecutionState(mPrevExecState);
	}

private:
	UINT				mPrevScreenSaver;
	EXECUTION_STATE		mPrevExecState;
};
