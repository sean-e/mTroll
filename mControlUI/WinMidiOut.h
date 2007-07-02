#pragma once

#include "..\Engine\IMidiOut.h"
#include <Windows.h>
#include <MMSystem.h>

class ITraceDisplay;


class WinMidiOut : public IMidiOut
{
public:
	WinMidiOut(ITraceDisplay * trace);
	virtual ~WinMidiOut();

	// IMidiOut
	virtual unsigned int GetMidiOutDeviceCount() const;
	virtual std::string GetMidiOutDeviceName(unsigned int deviceIdx) const;
	virtual void SetActivityIndicator(ISwitchDisplay * activityIndicator, int activityIndicatorIdx);
	virtual bool OpenMidiOut(unsigned int deviceIdx);
	virtual bool IsMidiOutOpen() const {return mMidiOut != NULL;}
	virtual bool MidiOut(const Bytes & bytes);
	virtual void CloseMidiOut();

private:
	void ReportMidiError(MMRESULT resultCode, unsigned int lineNumber);
	void ReportError(LPCSTR msg);
	void ReportError(LPCSTR msg, int param1);
	void ReportError(LPCSTR msg, int param1, int param2);

	static void CALLBACK MidiOutCallbackProc(HMIDIOUT hmo, UINT wMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);

	class ActivityIndicator
	{
		WinMidiOut		*mThis;

	public:
		ActivityIndicator(WinMidiOut * _this);
		~ActivityIndicator();
	};

	ITraceDisplay				* mTrace;
	ISwitchDisplay				* mActivityIndicator;
	int							mActivityIndicatorIndex;
	HMIDIOUT					mMidiOut;
	enum {MIDIHDR_CNT = 128};
	MIDIHDR						mMidiHdrs[MIDIHDR_CNT];
	int							mCurMidiHdrIdx;
	bool						mMidiOutError;
};
