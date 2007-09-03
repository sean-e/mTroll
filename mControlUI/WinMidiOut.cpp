#include "WinMidiOut.h"
#include "..\Engine\ITraceDisplay.h"
#include "..\Engine\ISwitchDisplay.h"
#include <atlstr.h>

#pragma comment(lib, "winmm.lib")

static std::string GetMidiErrorText(MMRESULT resultCode);
WinMidiOut * sOutOnTimer;


WinMidiOut::WinMidiOut(ITraceDisplay * trace) : 
	mTrace(trace), 
	mMidiOut(NULL), 
	mMidiOutError(false),
	mCurMidiHdrIdx(0),
	mActivityIndicator(NULL),
	mActivityIndicatorIndex(0),
	mEnableActivityIndicator(false)
{
	for (int idx = 0; idx < MIDIHDR_CNT; ++idx)
		ZeroMemory(&mMidiHdrs[idx], sizeof(MIDIHDR));
}

WinMidiOut::~WinMidiOut()
{
	if (sOutOnTimer == this)
		TimerProc(NULL, WM_TIMER, (UINT_PTR)this, 0);

	CloseMidiOut();
}

// IMidiOut
unsigned int
WinMidiOut::GetMidiOutDeviceCount() const
{
	return ::midiOutGetNumDevs();
}

std::string
WinMidiOut::GetMidiOutDeviceName(unsigned int deviceIdx) const
{
	std::string devName;
	MIDIOUTCAPS outCaps;
	MMRESULT res = ::midiOutGetDevCaps(deviceIdx, &outCaps, sizeof(MIDIOUTCAPS));
	if (res == MMSYSERR_NOERROR)
		devName = outCaps.szPname;
	else
	{
		std::string errMsg(::GetMidiErrorText(res));
		CString msg;
		msg.Format(_T("Error getting name of out device %d: %s"), deviceIdx, errMsg.c_str());
		devName = msg;
	}

	return devName;
}

void
WinMidiOut::SetActivityIndicator(ISwitchDisplay * activityIndicator, 
								 int activityIndicatorIdx)
{
	mActivityIndicator = activityIndicator;
	mActivityIndicatorIndex = activityIndicatorIdx;
	mEnableActivityIndicator = mActivityIndicator > 0 && mActivityIndicator != NULL;
}

void
WinMidiOut::EnableActivityIndicator(bool enable)
{
	if (enable)
		mEnableActivityIndicator = mActivityIndicator > 0 && mActivityIndicator != NULL;
	else
		mEnableActivityIndicator = false;
}

bool
WinMidiOut::OpenMidiOut(unsigned int deviceIdx)
{
	_ASSERTE(!mMidiOut);
	MMRESULT res = ::midiOutOpen(&mMidiOut, deviceIdx, (DWORD_PTR)MidiOutCallbackProc, (DWORD_PTR)this, CALLBACK_FUNCTION);
	if (MMSYSERR_NOERROR != res)
		ReportMidiError(res, __LINE__);
	return res == MMSYSERR_NOERROR;
}

#define NOTEOFF			0x80
#define	NOTEON			0x90
#define SYSEX			0xF0
#define EOX				0xF7
#define	MIDI_CLOCK		0xF8
#define	MIDI_START		0xFA
#define	MIDI_CONTINUE	0xFB
#define	MIDI_STOP		0xFC

bool
WinMidiOut::MidiOut(const Bytes & bytes)
{
	if (!mMidiOut)
	{
		if (0 && mTrace)
			mTrace->Trace("midiout is not open.\n");
		return false;
	}

	const size_t kDataSize = bytes.size();
	if (!kDataSize)
		return false;

	IndicateActivity();

	// number of data bytes for each status
	static const int kMsgDataBytesLen = 23;
	static const int kMsgDataBytes[23] = { 2, 2, 2, 2, 1, 1, 2, 0, 1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	const byte * dataPtr = &bytes[0];
	size_t idx = 0;
	MMRESULT res;
	mMidiOutError = false;
	int sysexDepth = 0;

	const int kBufferSize = 256;
	const int kDelayTime = 60;

	for ( ; idx < kDataSize && !mMidiOutError; )
	{
		int curMsgLen;
		int statusByteIdx = dataPtr[idx];

		if (sysexDepth || SYSEX == statusByteIdx)
		{
			curMsgLen = 0;

			do
			{
				if (SYSEX == statusByteIdx)
					sysexDepth++;	// nested sysex
				else if (EOX == statusByteIdx)
					sysexDepth--;

				if (!((idx + ++curMsgLen) % kBufferSize))
					break;
			}
			while (sysexDepth && (idx + curMsgLen) < kDataSize);

			LPMIDIHDR curHdr = &mMidiHdrs[mCurMidiHdrIdx++];
			if (mCurMidiHdrIdx == MIDIHDR_CNT)
				mCurMidiHdrIdx = 0;

			curHdr->dwBufferLength = curMsgLen;
			curHdr->lpData = (LPSTR)(byte*)&bytes[idx];

			res = ::midiOutPrepareHeader(mMidiOut, curHdr, sizeof(MIDIHDR));
			if (MMSYSERR_NOERROR == res)
				res = ::midiOutLongMsg(mMidiOut, curHdr, sizeof(MIDIHDR));
		}
		else
		{
			if ((statusByteIdx & 0xF0) < SYSEX)		// is it a channel message?
				statusByteIdx = ((statusByteIdx & 0xF0) >> 4) - 8;
			else		// or system message
				statusByteIdx = (statusByteIdx & 0x0F) + 7;

			if (statusByteIdx < 0)
			{
				ReportError("Status byte handling error at byte %d (%x).\n", idx+1, dataPtr[idx]);
				break;
			}

			curMsgLen = statusByteIdx >= kMsgDataBytesLen ? 1 : kMsgDataBytes[statusByteIdx] + 1;
			if ((idx + curMsgLen) > kDataSize)
			{
				ReportError("Data string consistency error at byte %d.  Missing data byte.\n", idx+1);
				break;
			}

			DWORD shortMsg;
			switch (curMsgLen)
			{
			case 1:			// no data bytes
				shortMsg = dataPtr[idx];
				break;
			case 2:			// one data byte
				shortMsg = dataPtr[idx] | (dataPtr[idx+1] << 8);
				break;
			case 3:			// two data bytes
				shortMsg = dataPtr[idx] | (dataPtr[idx+1] << 8) | (dataPtr[idx+2] << 16);
				break;
			default:
				_ASSERTE(!"impossible");
				shortMsg = 0;
			}

			res = ::midiOutShortMsg(mMidiOut, shortMsg);
		}

		if (MMSYSERR_NOERROR != res)
		{
			if (MIDIERR_NOTREADY == res) 
			{
				Sleep(50);
				continue;
			}
			else
			{
				ReportMidiError(res, __LINE__);
				break;
			}
		}

		idx += curMsgLen;
		if (!(idx % kBufferSize))
			::Sleep(kDelayTime);
	}

	if (0 && mTrace && !mMidiOutError)
		mTrace->Trace("Transmission complete.\n");

	return true;
}

void
WinMidiOut::MidiOut(byte singleByte,
					bool useIndicator /*= true*/)
{
	if (!mMidiOut)
		return;

	if (useIndicator)
		IndicateActivity();
	const MMRESULT res = ::midiOutShortMsg(mMidiOut, singleByte);
	if (MMSYSERR_NOERROR != res)
		ReportMidiError(res, __LINE__);
}

void
WinMidiOut::MidiOut(byte byte1, 
					byte byte2,
					bool useIndicator /*= true*/)
{
	if (!mMidiOut)
		return;

	if (useIndicator)
		IndicateActivity();
	const DWORD shortMsg = byte1 | (byte2 << 8);
	const MMRESULT res = ::midiOutShortMsg(mMidiOut, shortMsg);
	if (MMSYSERR_NOERROR != res)
		ReportMidiError(res, __LINE__);
}

void
WinMidiOut::MidiOut(byte byte1, 
					byte byte2, 
					byte byte3,
					bool useIndicator /*= true*/)
{
	if (!mMidiOut)
		return;

	if (useIndicator)
		IndicateActivity();
	const DWORD shortMsg = byte1 | (byte2 << 8) | (byte3 << 16);
	const MMRESULT res = ::midiOutShortMsg(mMidiOut, shortMsg);
	if (MMSYSERR_NOERROR != res)
		ReportMidiError(res, __LINE__);
}

void CALLBACK 
WinMidiOut::MidiOutCallbackProc(HMIDIOUT hmo, 
								UINT wMsg, 
								DWORD dwInstance, 
								DWORD dwParam1, 
								DWORD dwParam2)
{
	if (MOM_DONE == wMsg)
	{
		WinMidiOut * _this = (WinMidiOut *) dwInstance;
		LPMIDIHDR hdr = (LPMIDIHDR) dwParam1;
		MMRESULT res = ::midiOutUnprepareHeader(_this->mMidiOut, hdr, sizeof(MIDIHDR));
		hdr->dwFlags = 0;
		_ASSERTE(MMSYSERR_NOERROR == res);
	}
}

void
WinMidiOut::IndicateActivity()
{
	if (!mEnableActivityIndicator)
	{
		if (sOutOnTimer == this)
			TimerProc(NULL, WM_TIMER, (UINT_PTR)this, 0);

		return;
	}

	mActivityIndicator->SetSwitchDisplay(mActivityIndicatorIndex, true);
	sOutOnTimer = this;
	::SetTimer(NULL, (UINT_PTR)this, 150, TimerProc);
}

void CALLBACK
WinMidiOut::TimerProc(HWND, 
					  UINT, 
					  UINT_PTR id, 
					  DWORD)
{
//	WinMidiOut * whyDoesntThisWork = reinterpret_cast<WinMidiOut *>(id);
	WinMidiOut * _this = sOutOnTimer;
	sOutOnTimer = NULL;
	if (_this)
		_this->mActivityIndicator->SetSwitchDisplay(_this->mActivityIndicatorIndex, false);
}

void
WinMidiOut::CloseMidiOut()
{
	mEnableActivityIndicator = false;
	mActivityIndicator = NULL;
	if (mMidiOut)
	{
		MMRESULT res = ::midiOutClose(mMidiOut);
		if (res == MMSYSERR_NOERROR)
			mMidiOut = NULL;
		else
			ReportMidiError(res, __LINE__);
	}
}

void
WinMidiOut::ReportMidiError(MMRESULT resultCode, 
							unsigned int lineNumber)
{
	std::string errMsg(::GetMidiErrorText(resultCode));
	CString msg;

	mMidiOutError = true;
	msg.Format(_T("Error: %s\nError location: %s (%d)\n"), errMsg.c_str(), __FILE__, lineNumber);
	if (mTrace)
		mTrace->Trace(std::string(msg));
}

void
WinMidiOut::ReportError(LPCSTR msg)
{
	mMidiOutError = true;
	if (mTrace)
		mTrace->Trace(msg);
}

void
WinMidiOut::ReportError(LPCSTR msg, 
						int param1)
{
	CString errMsg;
	errMsg.Format(msg, param1);
	ReportError(errMsg);
}

void
WinMidiOut::ReportError(LPCSTR msg, 
						int param1, 
						int param2)
{
	CString errMsg;
	errMsg.Format(msg, param1, param2);
	ReportError(errMsg);
}

std::string
GetMidiErrorText(MMRESULT resultCode)
{
	TCHAR	errMsg[MAXERRORLENGTH];
	::midiOutGetErrorText(resultCode, errMsg, sizeof(TCHAR)*MAXERRORLENGTH);
	return errMsg;
}
