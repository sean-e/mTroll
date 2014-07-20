/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2009,2014 Sean Echevarria
 *
 * This file is part of mTroll.
 *
 * mTroll is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mTroll is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Let me know if you modify, extend or use mTroll.
 * Original project site: http://www.creepingfog.com/mTroll/
 * Contact Sean: "fester" at the domain of the original project site
 */

#include "Monome40hFtqt.h"
#include <strstream>
#include <algorithm>
#include "../IMonome40h.h"
#include "../IMonome40hInputSubscriber.h"
#include "../MonomeSerialProtocol.h"
#include "../../Engine/ITraceDisplay.h"
#include "../../Engine/ScopeSet.h"
#ifdef _WINDOWS
#include <windows.h>
	#define SLEEP	Sleep
#else
#include "../notWin32/FTTypes.h"
	#define SLEEP	sleep
#endif // _WINDOWS
#include "../FTD2XX.H"


class MonomeThread : public QThread
{
	Monome40hFtqt * m40h;	// weak ref
public:
	MonomeThread(Monome40hFtqt * pMonome) : m40h(pMonome)
	{
		start(QThread::HighestPriority);
	}
	virtual void run()
	{
		m40h->DeviceServiceThread();
	}
};

Monome40hFtqt::Monome40hFtqt(ITraceDisplay * trace) :
	mTrace(trace),
	mFtDevice(INVALID_HANDLE_VALUE),
	mShouldContinueListening(true),
	mThread(NULL),
	mThreadId(NULL),
	mServicingSubscribers(false),
	mInputSubscriber(NULL),
	mAdcInputSubscriber(NULL),
	mLedBrightness(10)
{
#ifdef _WINDOWS
	// using Win32 delay loading - see if it is found
	HMODULE hMod = ::LoadLibraryW(L"FTD2XX.dll");
	if (!hMod)
		throw std::string("ERROR: Failed to load FTDI library\n");
#endif // _WINDOWS

	for (int portIdx = 0; portIdx < kAdcPortCount; ++portIdx)
	{
		mAdcEnable[portIdx] = false;
		mPrevAdcValsIndex[portIdx] = 0;
		for (int histIdx = 0; histIdx < kAdcValhist; ++histIdx)
			mPrevAdcVals[portIdx][histIdx] = -1;
	}
}

Monome40hFtqt::~Monome40hFtqt()
{
	ReleaseDevice();
}

int
Monome40hFtqt::LocateMonomeDeviceIdx()
{
	int numDevs;
	FT_STATUS ftStatus = ::FT_ListDevices(&numDevs, NULL, FT_LIST_NUMBER_ONLY);
	if (FT_OK != ftStatus)
	{
		if (mTrace)
		{
			std::strstream traceMsg;
			traceMsg << "ERROR: Failed to get FTDI device list: " << ftStatus << std::endl << std::ends;
			mTrace->Trace(traceMsg.str());
		}
		return -1;
	}

	int monomeDevIdx = -1;
	for (int idx = 0; idx < numDevs; ++idx)
	{
		std::string serial(GetDeviceSerialNumber(idx));
		if (mTrace)
		{
			std::strstream traceMsg;
			traceMsg << "FTDI device " << idx << " serial: " << serial << std::endl << std::ends;
			mTrace->Trace(traceMsg.str());
		}

		if (-1 == monomeDevIdx && serial.find("m40h") != -1)
		{
			monomeDevIdx = idx;
			break;
		}
	}

	if (-1 == monomeDevIdx && mTrace)
	{
		std::strstream traceMsg;
		traceMsg << "ERROR: Failed to locate monome device" << std::endl << std::ends;
		mTrace->Trace(traceMsg.str());
	}

	return monomeDevIdx;
}

std::string
Monome40hFtqt::GetDeviceSerialNumber(int devIndex)
{
	std::strstream traceMsg;
	int numDevs;
	FT_STATUS ftStatus = ::FT_ListDevices(&numDevs, NULL, FT_LIST_NUMBER_ONLY);
	if (FT_OK != ftStatus)
	{
		if (mTrace)
		{
			traceMsg << "ERROR: Failed to get FTDI device list: " << ftStatus << std::endl << std::ends;
			mTrace->Trace(traceMsg.str());
		}
		return "";
	}

	if (devIndex >= numDevs)
	{
		if (mTrace)
		{
			traceMsg << "ERROR: Requested FTDI device idx is greater than installed dev count: " << numDevs << std::endl << std::ends;
			mTrace->Trace(traceMsg.str());
		}
		return "";
	}

	char serialNo[64];
	ftStatus = ::FT_ListDevices((PVOID)devIndex, serialNo, FT_LIST_BY_INDEX|FT_OPEN_BY_SERIAL_NUMBER);
	if (FT_OK != ftStatus)
	{
		traceMsg << "ERROR: Failed to get FTDI device " << devIndex << " serial, status: " << ftStatus << std::endl << std::ends;
		mTrace->Trace(traceMsg.str());
		return "";
	}

	return serialNo;
}

bool
Monome40hFtqt::AcquireDevice(const std::string & devSerialNum)
{
	mDevSerialNumber = devSerialNum;
	if (!AcquireDevice())
		return false;

	SetLedIntensity(mLedBrightness);

	// startup listener
	mShouldContinueListening = true;
	mThread = new MonomeThread(this);
	return true;
}

bool
Monome40hFtqt::AcquireDevice()
{
	FT_HANDLE prevDev = mFtDevice;
	if (INVALID_HANDLE_VALUE != prevDev)
	{
		mFtDevice = INVALID_HANDLE_VALUE;
		::FT_W32_CloseHandle(prevDev);
	}

	mFtDevice = ::FT_W32_CreateFile((LPCSTR)mDevSerialNumber.c_str(), 
		GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 
		FILE_ATTRIBUTE_NORMAL|FT_OPEN_BY_SERIAL_NUMBER, NULL);
	if (INVALID_HANDLE_VALUE == mFtDevice)
	{
		if (mTrace)
		{
			std::strstream traceMsg;
			traceMsg << "ERROR: Failed to open FTDI device " << mDevSerialNumber << std::endl << std::ends;
			mTrace->Trace(traceMsg.str());
		}
		return false;
	}

	if (mTrace)
	{
		std::strstream traceMsg;
		traceMsg << "Opened FTDI device " << mDevSerialNumber << std::endl << std::ends;
		mTrace->Trace(traceMsg.str());
	}

	FTTIMEOUTS ftTS;
	ftTS.ReadIntervalTimeout = 0;
	ftTS.ReadTotalTimeoutMultiplier = 0;
	ftTS.ReadTotalTimeoutConstant = 15;
	ftTS.WriteTotalTimeoutMultiplier = 0;
	ftTS.WriteTotalTimeoutConstant = 15;
	if (!::FT_W32_SetCommTimeouts(mFtDevice, &ftTS))
	{
		if (mTrace)
		{
			std::strstream traceMsg;
			traceMsg << "ERROR: Failed to set FTDI device params " << std::endl << std::ends;
			mTrace->Trace(traceMsg.str());
		}
		return false;
	}

	return true;
}

void
Monome40hFtqt::ReleaseDevice()
{
	mShouldContinueListening = false;

	if (mThread)
	{
		mThread->wait(5000);
		delete mThread;
		mThread = NULL;
	}

	// free queued commands - there shouldn't be any...
	if (mTrace && mOutputCommandQueue.size())
	{
		std::strstream traceMsg;
		traceMsg << "WARNING: unsent monome commands still queued " << mOutputCommandQueue.size() << std::endl << std::ends;
		mTrace->Trace(traceMsg.str());
	}

	for (OutputCommandQueue::iterator it = mOutputCommandQueue.begin();
		it != mOutputCommandQueue.end();
		++it)
	{
		delete *it;
	}

	mOutputCommandQueue.clear();
}

bool
Monome40hFtqt::Subscribe(IMonome40hSwitchSubscriber * sub)
{
	_ASSERTE(!mServicingSubscribers);
	mInputSubscriber = sub;
	return true;
}

bool
Monome40hFtqt::Unsubscribe(IMonome40hSwitchSubscriber * sub)
{
	_ASSERTE(!mServicingSubscribers);

	if (mInputSubscriber == sub)
	{
		mInputSubscriber = NULL;
		return true;
	}

	return false;
}

bool
Monome40hFtqt::Subscribe(IMonome40hAdcSubscriber * sub)
{
	_ASSERTE(!mServicingSubscribers);
	mAdcInputSubscriber = sub;
	return true;
}

bool
Monome40hFtqt::Unsubscribe(IMonome40hAdcSubscriber * sub)
{
	_ASSERTE(!mServicingSubscribers);

	if (mAdcInputSubscriber == sub)
	{
		mAdcInputSubscriber = NULL;
		return true;
	}

	return false;
}

int
Monome40hFtqt::Send(const MonomeSerialProtocolData & data)
{
	_ASSERTE(mFtDevice && INVALID_HANDLE_VALUE != mFtDevice);
	DWORD bytesWritten = 0;
	int retval = ::FT_W32_WriteFile(mFtDevice, (void*)data.Data(), 2, &bytesWritten, NULL);
	if (retval && 2 > bytesWritten)
	{
		// timeout
		if (1 == bytesWritten)
		{
			retval = ::FT_W32_WriteFile(mFtDevice, (void*)(data.Data()+1), 1, &bytesWritten, NULL);
			_ASSERTE(retval && 1 == bytesWritten);
		}
		else
		{
			retval = ::FT_W32_WriteFile(mFtDevice, (void*)data.Data(), 2, &bytesWritten, NULL);
			_ASSERTE(retval && 2 == bytesWritten);
		}
	}
	_ASSERTE(retval);
	return retval;
}

void
Monome40hFtqt::EnableLed(byte row, 
						 byte col, 
						 bool enable)
{
#ifdef PER_LED_INTENSITY
	DispatchCommand(new MonomeSetLed(enable ? mLedBrightness : 0, row, col));
#else
	DispatchCommand(new MonomeSetLed(enable, row, col));
#endif // PER_LED_INTENSITY
}

#ifdef PER_LED_INTENSITY
void
Monome40hFtqt::EnableLed(byte row, 
						 byte col, 
						 byte intensity)
{
	if (0 > intensity)
		intensity = 0;
	else if (15 < intensity)
		intensity = 15;

	DispatchCommand(new MonomeSetLed(intensity, row, col));
}
#endif // PER_LED_INTENSITY

void
Monome40hFtqt::SetLedIntensity(byte brightness)
{
	if (0 > brightness)
		mLedBrightness = 0;
	else if (15 < brightness)
		mLedBrightness = 15;
	else
		mLedBrightness = brightness;

	DispatchCommand(new MonomeSetLedIntensity(mLedBrightness));
}

void
Monome40hFtqt::TestLed(bool enable)
{
	DispatchCommand(new MonomeTestLed(enable));
}

void
Monome40hFtqt::EnableAdc(byte port, 
						 bool enable)
{
	mAdcEnable[port] = enable;
	DispatchCommand(new MonomeEnableAdc(port, enable));
}

void
Monome40hFtqt::Shutdown(bool state)
{
	DispatchCommand(new MonomeShutdown(state));
}

void
Monome40hFtqt::EnableLedRow(byte row, 
							byte columnValues)
{
	DispatchCommand(new MonomeSetLedRow(row, columnValues));
}

void
Monome40hFtqt::EnableLedColumn(byte column, 
							   byte rowValues)
{
	DispatchCommand(new MonomeSetLedColumn(column, rowValues));
}

bool
Monome40hFtqt::ReadInput(byte * readData)
{
	const byte cmd = readData[0] >> 4;
	switch (cmd) 
	{
	case MonomeSerialProtocolData::getPress:
		if (mInputSubscriber)
		{
			byte state = readData[0] & 0x0f;
			byte col = readData[1] >> 4;
			byte row = readData[1] & 0x0f;

			ScopeSet<volatile bool> active(&mServicingSubscribers, true);
			if (state)
				mInputSubscriber->SwitchPressed(row, col);
			else
				mInputSubscriber->SwitchReleased(row, col);
		}
		return true;

	case MonomeSerialProtocolData::getAdcVal:
		if (mAdcInputSubscriber)
		{
			const byte port = (readData[0] & 0x0c) >> 2;
			if (port >= kAdcPortCount)
				return true;
			int adcValue = readData[1];
			adcValue |= ((readData[0] & 3) << 8);

			// standard monome firmware does filtering and smoothing using 16 buckets (via averaging)
			// modified firmware uses 4 buckets - basically smoothing
			// if using modified firmware, do filtering here (using hard compares instead of averaging)
			// otherwise set kDoFiltering to false
			const bool kDoFiltering = true;
			if (kDoFiltering)
			{
				int * prevValsForCurPort = mPrevAdcVals[port];
				for (int idx = 0; idx < kAdcValhist; ++idx)
				{
					if (prevValsForCurPort[idx] == adcValue)
					{
						if (0 && mTrace)
						{
							std::strstream displayMsg;
							displayMsg << "adc val repeat: " << adcValue << std::endl << std::ends;
							mTrace->Trace(displayMsg.str());
						}
						return true;
					}
				}

				prevValsForCurPort[mPrevAdcValsIndex[port]++] = adcValue;
				if (mPrevAdcValsIndex[port] >= kAdcValhist)
					mPrevAdcValsIndex[port] = 0;
			}

			mAdcInputSubscriber->AdcValueChanged(port, adcValue);
		}
		return true;

	default:
		if (mTrace)
		{
			const byte n2 = readData[0] & 0x0f;
			const byte n3 = readData[1] >> 4;
			const byte n4 = readData[1] & 0x0f;

			std::strstream traceMsg;
			traceMsg << "monome IO error: unknown command " << (int) cmd << " " << (short)n2 << " " << (short)n3 << " " << (short)n4 << std::endl << std::ends;
			mTrace->Trace(traceMsg.str());
		}
		return false;
	}
}

void
Monome40hFtqt::DeviceServiceThread()
{
	const int kDataLen = 2;
	int consecutiveReadErrors = 0;
	byte readData[kDataLen];
	DWORD bytesRead;

	mThreadId = QThread::currentThreadId();
	while (mShouldContinueListening)
	{
		if (mOutputCommandQueue.size())
			ServiceCommands();

		bytesRead = 0;
		int readRetVal = ::FT_W32_ReadFile(mFtDevice, readData, kDataLen, &bytesRead, NULL);
		if (readRetVal)
		{
			consecutiveReadErrors = 0;
			if (FT_IO_ERROR == readRetVal)
			{
				if (mTrace)
				{
					std::strstream traceMsg;
					traceMsg << "monome IO error: disconnected?" << std::endl << std::ends;
					mTrace->Trace(traceMsg.str());
				}
			}
			else if (bytesRead)
			{
				if (1 == bytesRead)
				{
					// timeout interrupted read, get the next byte
					for (int cnt = 0; cnt < 2; ++cnt)
					{
						bytesRead = 0;
						::FT_W32_ReadFile(mFtDevice, &readData[1], 1, &bytesRead, NULL);
						if (bytesRead)
						{
							if (1 != bytesRead && mTrace)
							{
								std::strstream traceMsg;
								traceMsg << "monome read interrupted: bytes read on retry " << (int) bytesRead << std::endl << std::ends;
								mTrace->Trace(traceMsg.str());
							}
							++bytesRead;
							break;
						}
						else if (cnt && mTrace)
						{
							const byte n1 = readData[0] >> 4;
							const byte n2 = readData[0] & 0x0f;

							std::strstream traceMsg;
							traceMsg << "monome read interrupted: retry " << cnt << " failed cmd(" << (short)n1 << " " << (short)n2 << ")" << std::endl << std::ends;
							mTrace->Trace(traceMsg.str());
						}
						SLEEP(10);
					}
				}

				if (kDataLen == bytesRead)
				{
					ReadInput(readData);
				}
				else if (mTrace)
				{
					// if bytes is 0, then we entered the timeout interrupt block 
					// and reset it to 0 but then failed to read anything on the retry
					std::strstream traceMsg;
					traceMsg << "monome read error: bytes read " << (int) bytesRead << " command " << (readData[0] >> 4) << " " << (readData[0] & 0x0f) << std::endl << std::ends;
					mTrace->Trace(traceMsg.str());
				}
			}
			else
			{
				// timeout
			}
		}
		else
		{
			if (mTrace && !consecutiveReadErrors)
			{
				std::strstream traceMsg;
				traceMsg << "monome read error" << std::endl << std::ends;
				mTrace->Trace(traceMsg.str());
			}

			if (++consecutiveReadErrors > 100)
			{
				consecutiveReadErrors = 0;
				if (mTrace)
				{
					std::strstream traceMsg;
					traceMsg << "reconnecting to monome due to read errors" << std::endl << std::ends;
					mTrace->Trace(traceMsg.str());
				}

				if (!AcquireDevice())
				{
					mShouldContinueListening = false;
					return;
				}
			}
		}
	}

	ServiceCommands();

	::FT_W32_CloseHandle(mFtDevice);
	mFtDevice = INVALID_HANDLE_VALUE;
}

void
Monome40hFtqt::ServiceCommands()
{
	QMutexLocker lock(&mOutputCommandsLock);
	if (!mOutputCommandQueue.size())
		return;

	for (OutputCommandQueue::iterator it = mOutputCommandQueue.begin();
		it != mOutputCommandQueue.end();
		++it)
	{
		const MonomeSerialProtocolData * curCmd = *it;
		Send(*curCmd);
		delete curCmd;
	}

	mOutputCommandQueue.clear();
}

void
Monome40hFtqt::DispatchCommand(const MonomeSerialProtocolData * data)
{
	if (INVALID_HANDLE_VALUE == mFtDevice)
	{
		delete data;
		return;
	}

	if (mServicingSubscribers)
	{
		Qt::HANDLE curThreadId = QThread::currentThreadId();
		if (curThreadId == mThreadId)
		{
			// handle synchronously
			Send(*data);
			delete data;
			return;
		}
	}

	// queue to be serviced asynchronously
	QMutexLocker lock(&mOutputCommandsLock);
	mOutputCommandQueue.push_back(data);
}

bool
Monome40hFtqt::IsAdcEnabled(int portIdx) const
{
	if (portIdx < 0 || portIdx >= kAdcPortCount)
		return false;

	return mAdcEnable[portIdx];
}
