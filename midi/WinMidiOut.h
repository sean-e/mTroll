#ifndef WinMidiOut_h__
#define WinMidiOut_h__

#include "..\Engine\IMidiOut.h"
#include <Windows.h>
#include <MMSystem.h>
#include <tchar.h>

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
	virtual void EnableActivityIndicator(bool enable);
	virtual bool OpenMidiOut(unsigned int deviceIdx);
	virtual bool IsMidiOutOpen() const {return mMidiOut != NULL;}
	virtual bool MidiOut(const Bytes & bytes);
	virtual void MidiOut(byte singleByte, bool useIndicator = true);
	virtual void MidiOut(byte byte1, byte byte2, bool useIndicator = true);
	virtual void MidiOut(byte byte1, byte byte2, byte byte3, bool useIndicator = true);
	virtual void CloseMidiOut();

private:
	void ReportMidiError(MMRESULT resultCode, unsigned int lineNumber);
	void ReportError(LPCTSTR msg);
	void ReportError(LPCTSTR msg, int param1);
	void ReportError(LPCTSTR msg, int param1, int param2);

	void IndicateActivity();
	void TurnOffIndicator();
	static void CALLBACK TimerProc(HWND, UINT, UINT_PTR id, DWORD);
	static void CALLBACK MidiOutCallbackProc(HMIDIOUT hmo, UINT wMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);

	ITraceDisplay				* mTrace;
	ISwitchDisplay				* mActivityIndicator;
	volatile bool				mEnableActivityIndicator;
	int							mActivityIndicatorIndex;
	HMIDIOUT					mMidiOut;
	enum {MIDIHDR_CNT = 128};
	MIDIHDR						mMidiHdrs[MIDIHDR_CNT];
	int							mCurMidiHdrIdx;
	bool						mMidiOutError;
	UINT_PTR					mTimerId;
	LONG						mTimerEventCount;
};

#endif // WinMidiOut_h__
