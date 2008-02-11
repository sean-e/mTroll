#pragma once

// see the following references:
// http://support.microsoft.com/kb/q126627/
// http://msdn2.microsoft.com/en-us/library/aa373208.aspx
// http://softwarecommunity.intel.com/articles/eng/2667.htm


class KeepDisplayOn
{
public:
	KeepDisplayOn()
	{
		mPrevExecState = ::SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_CONTINUOUS);

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
