#include "WinMidiOut.h"
#include "..\Engine\ITraceDisplay.h"
#include <atlstr.h>

#pragma comment(lib, "winmm.lib")


WinMidiOut::~WinMidiOut()
{
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
		ReportMidiError(res, __LINE__);

	return devName;
}

bool
WinMidiOut::OpenMidiOut(unsigned int deviceIdx)
{
	_ASSERTE(!mMidiOut);
	MMRESULT res = ::midiOutOpen(&mMidiOut, deviceIdx, NULL, this, CALLBACK_NULL);
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
	_ASSERTE(mMidiOut);
	if (!mMidiOut)
		return false;

	// number of data bytes for each status
	static const int kStatusByteLenArrayLen = 23;
	static const int kMsglens[23] = { 2, 2, 2, 2, 1, 1, 2, 0, 1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	const size_t kDataSize = bytes.size();
	const byte * dataPtr = &bytes[0];
	size_t idx = 0;
	int sysexDepth = 0;
	MMRESULT res;
	mMidiOutError = false;

	const int kBufferSize = 256;
	const int kDelayTime = 60;

	for ( ; idx < kDataSize && !mMidiOutError; )
	{
		// pack the data into a midievent
		if (sysexDepth || 
			SYSEX == dataPtr[idx] || 
			EOX == dataPtr[idx])
		{
			if (EOX == dataPtr[idx])
				sysexDepth--;
			else if (SYSEX == dataPtr[idx])
				sysexDepth++;	// nested sysex

			++idx;
			// send sysex
// 			res = ::MidiOut(mMidiOut, (LPMIDIEVENT) &outEvent);
			res = MMSYSERR_NOTSUPPORTED;
		}
		else
		{
			int statusByteIdx = dataPtr[idx];

			if ((statusByteIdx & 0xF0) < SYSEX)		// is it a channel message?
				statusByteIdx = ((statusByteIdx & 0xF0) >> 4) - 8;
			else		// or system message
				statusByteIdx = (statusByteIdx & 0x0F) + 7;

			if (statusByteIdx < 0 || statusByteIdx >= kStatusByteLenArrayLen)
			{
				ReportError("Status byte handling error at byte %d (%x).\n", idx+1, dataPtr[idx]);
				break;
			}

			const int kCurMsgLen = kMsglens[statusByteIdx];
			if ((idx + kCurMsgLen) > kDataSize)
			{
				ReportError("Data string consistency error at byte %d.  Missing data byte.\n", idx+1);
				break;
			}

			DWORD shortMsg;
			switch (kCurMsgLen)
			{
			case 0:
				// no data bytes
				shortMsg = dataPtr[idx];
				break;
			case 1:
				// one data byte
				shortMsg = dataPtr[idx] | (dataPtr[idx+1] << 8);
				break;
			case 2:
				// two data bytes
				shortMsg = dataPtr[idx] | (dataPtr[idx+1] << 8) | (dataPtr[idx+2] << 16);
				break;
			default:
				_ASSERTE(!"impossible");
				shortMsg = 0;
			}

			if (mMidiOutError)
				break;

			res = ::midiOutShortMsg(mMidiOut, shortMsg);
			idx += kCurMsgLen + 1;
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

		if (!(idx % kBufferSize))
			Sleep(kDelayTime);
	}

	if (mTrace && !mMidiOutError)
		mTrace->Trace("Transmission complete.\n");

	return true;
}

void
WinMidiOut::CloseMidiOut()
{
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
							unsigned int lineNumber) const
{
	TCHAR	errMsg[MAXERRORLENGTH];
	CString msg;

	mMidiOutError = true;
	::midiOutGetErrorText(resultCode, errMsg, sizeof(TCHAR)*MAXERRORLENGTH);
	msg.Format(_T("Error: %s\nError location: %s (%d)\n"), errMsg, __FILE__, lineNumber);
	if (mTrace)
		mTrace->Trace(std::string(msg));
}

void
WinMidiOut::ReportError(LPCSTR msg) const
{
	mMidiOutError = true;
	if (mTrace)
		mTrace->Trace(msg);
}

void
WinMidiOut::ReportError(LPCSTR msg, 
						int param1) const
{
	CString errMsg;
	errMsg.Format(msg, param1);
	ReportError(errMsg);
}

void
WinMidiOut::ReportError(LPCSTR msg, 
						int param1, 
						int param2) const
{
	CString errMsg;
	errMsg.Format(msg, param1, param2);
	ReportError(errMsg);
}
