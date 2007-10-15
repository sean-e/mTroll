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

#include "Monome40hFtw.h"
#include <strstream>
#include <algorithm>
#include <process.h>
#include "../IMonome40h.h"
#include "../IMonome40hInputSubscriber.h"
#include "../MonomeSerialProtocol.h"
#include "../../Engine/ITraceDisplay.h"
#include "../../Engine/ScopeSet.h"
#include "../../mControlUI/AutoLockCs.h"


Monome40hFtw::Monome40hFtw(ITraceDisplay * trace) :
	mTrace(trace),
	mFtDevice(INVALID_HANDLE_VALUE),
	mShouldContinueListening(true),
	mThread(NULL),
	mThreadId(0),
	mServicingSubscribers(false),
	mInputSubscriber(NULL),
	mAdcInputSubscriber(NULL),
	mLedBrightness(10)
{
	HMODULE hMod = ::LoadLibrary("FTD2XX.dll");
	if (!hMod)
		throw std::string("ERROR: Failed to load FTDI library\n");

	::InitializeCriticalSection(&mOutputCommandsLock);

	for (int portIdx = 0; portIdx < kAdcPortCount; ++portIdx)
		for (int histIdx = 0; histIdx < kAdcValhist; ++histIdx)
			mPrevAdcVals[portIdx][histIdx] = -1;
}

Monome40hFtw::~Monome40hFtw()
{
	ReleaseDevice();
	::DeleteCriticalSection(&mOutputCommandsLock);
}

int
Monome40hFtw::LocateMonomeDeviceIdx()
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
		return -1;
	}

	int monomeDevIdx = -1;
	for (int idx = 0; idx < numDevs; ++idx)
	{
		std::string serial(GetDeviceSerialNumber(idx));
		if (mTrace)
		{
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
		traceMsg << "ERROR: Failed to locate monome device" << std::endl << std::ends;
		mTrace->Trace(traceMsg.str());
	}

	return monomeDevIdx;
}

std::string
Monome40hFtw::GetDeviceSerialNumber(int devIndex)
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
Monome40hFtw::AcquireDevice(const std::string & devSerialNum)
{
	std::strstream traceMsg;
	mDevSerialNumber = devSerialNum;
	mFtDevice = ::FT_W32_CreateFile(devSerialNum.c_str(), 
		GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 
		FILE_ATTRIBUTE_NORMAL|FT_OPEN_BY_SERIAL_NUMBER, NULL);
	if (INVALID_HANDLE_VALUE == mFtDevice)
	{
		if (mTrace)
		{
			traceMsg << "ERROR: Failed to open FTDI device " << devSerialNum << std::endl << std::ends;
			mTrace->Trace(traceMsg.str());
		}
		return false;
	}

	if (mTrace)
	{
		traceMsg << "Opened FTDI device " << devSerialNum << std::endl << std::ends;
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
			traceMsg << "ERROR: Failed to set FTDI device params " << std::endl << std::ends;
			mTrace->Trace(traceMsg.str());
		}
		return false;
	}

	// startup listener
	mShouldContinueListening = true;
	mThread = (HANDLE)_beginthreadex(NULL, 0, DeviceServiceThread, this, 0, (unsigned int*)&mThreadId);
	if (mThread && mThread != INVALID_HANDLE_VALUE)
	{
		::SetThreadPriority(mThread, /*HIGH_PRIORITY_CLASS*/  REALTIME_PRIORITY_CLASS );
		SetLedIntensity(mLedBrightness);
		return true;
	}

	return false;
}

void
Monome40hFtw::ReleaseDevice()
{
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

	mShouldContinueListening = false;

	::WaitForSingleObjectEx(mThread, 3000, FALSE);
	CloseHandle(mThread);
	mThread = NULL;

	::FT_W32_CloseHandle(mFtDevice);
	mFtDevice = INVALID_HANDLE_VALUE;

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
Monome40hFtw::Subscribe(IMonome40hSwitchSubscriber * sub)
{
	_ASSERTE(!mServicingSubscribers);
	mInputSubscriber = sub;
	return true;
}

bool
Monome40hFtw::Unsubscribe(IMonome40hSwitchSubscriber * sub)
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
Monome40hFtw::Subscribe(IMonome40hAdcSubscriber * sub)
{
	_ASSERTE(!mServicingSubscribers);
	mAdcInputSubscriber = sub;
	return true;
}

bool
Monome40hFtw::Unsubscribe(IMonome40hAdcSubscriber * sub)
{
	_ASSERTE(!mServicingSubscribers);

	if (mAdcInputSubscriber == sub)
	{
		mAdcInputSubscriber = NULL;
		return true;
	}

	return false;
}

BOOL
Monome40hFtw::Send(const MonomeSerialProtocolData & data)
{
	_ASSERTE(mFtDevice && INVALID_HANDLE_VALUE != mFtDevice);
	DWORD bytesWritten = 0;
	BOOL retval = ::FT_W32_WriteFile(mFtDevice, (void*)data.Data(), 2, &bytesWritten, NULL);
	_ASSERTE(retval && 2 == bytesWritten);
	return retval;
}

void
Monome40hFtw::EnableLed(byte row, 
						byte col, 
						bool enable)
{
	_ASSERTE(mFtDevice && INVALID_HANDLE_VALUE != mFtDevice);
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

	DispatchCommand(new MonomeSetLed(enable, row, col));
}

void
Monome40hFtw::SetLedIntensity(byte brightness)
{
	_ASSERTE(mFtDevice && INVALID_HANDLE_VALUE != mFtDevice);
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

	if (0 > brightness)
		mLedBrightness = 0;
	else if (10 < brightness)
		mLedBrightness = 10;
	else
		mLedBrightness = brightness;

	DispatchCommand(new MonomeSetLedIntensity(mLedBrightness));
}

void
Monome40hFtw::TestLed(bool enable)
{
	_ASSERTE(mFtDevice && INVALID_HANDLE_VALUE != mFtDevice);
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

	DispatchCommand(new MonomeTestLed(enable));
}

void
Monome40hFtw::EnableAdc(byte port, 
						bool enable)
{
	_ASSERTE(mFtDevice && INVALID_HANDLE_VALUE != mFtDevice);
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

	DispatchCommand(new MonomeEnableAdc(port, enable));
}

void
Monome40hFtw::Shutdown(bool state)
{
	_ASSERTE(mFtDevice && INVALID_HANDLE_VALUE != mFtDevice);
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

	DispatchCommand(new MonomeShutdown(state));
}

void
Monome40hFtw::EnableLedRow(byte row, 
						   byte columnValues)
{
	_ASSERTE(mFtDevice && INVALID_HANDLE_VALUE != mFtDevice);
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

	DispatchCommand(new MonomeSetLedRow(row, columnValues));
}

void
Monome40hFtw::EnableLedColumn(byte column, 
							  byte rowValues)
{
	_ASSERTE(mFtDevice && INVALID_HANDLE_VALUE != mFtDevice);
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

	DispatchCommand(new MonomeSetLedColumn(column, rowValues));
}

void
Monome40hFtw::ReadInput(byte * readData)
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
		break;
	case MonomeSerialProtocolData::getAdcVal:
		if (mAdcInputSubscriber)
		{
			const byte port = (readData[0] & 0x0c) >> 2;
			if (port >= kAdcPortCount)
				break;
			int adcValue = readData[1];
			adcValue |= ((readData[0] & 3) << 8);

			if (mPrevAdcVals[port][0] == adcValue)
				break;
			if (mPrevAdcVals[port][1] == adcValue)
			{
				if (0 && mTrace)
				{
					std::strstream displayMsg;
					displayMsg << "adc val repeat: " << adcValue << std::endl << std::ends;
					mTrace->Trace(displayMsg.str());
				}
				break;
			}

			mAdcInputSubscriber->AdcValueChanged(port, adcValue);
			mPrevAdcVals[port][1] = mPrevAdcVals[port][0];
			mPrevAdcVals[port][0] = adcValue;
		}
		break;
	default:
		if (mTrace)
		{
			std::strstream traceMsg;
			traceMsg << "monome IO error: unknown command " << (int) cmd << std::endl << std::ends;
			mTrace->Trace(traceMsg.str());
		}
	}
}

void
Monome40hFtw::DeviceServiceThread()
{
	bool aborted = false;
	const int kDataLen = 2;
	int consecutiveReadErrors = 0;
	byte readData[kDataLen];
	DWORD bytesRead;

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
					bytesRead = 0;
					::FT_W32_ReadFile(mFtDevice, &readData[1], 1, &bytesRead, NULL);
					if (bytesRead)
						++bytesRead;
				}

				if (kDataLen == bytesRead)
				{
					ReadInput(readData);
				}
				else if (mTrace)
				{
					std::strstream traceMsg;
					traceMsg << "monome read error: bytes read " << (int) bytesRead << std::endl << std::ends;
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
				aborted = true;
				if (mTrace)
				{
					std::strstream traceMsg;
					traceMsg << "aborting monome thread due to read errors" << std::endl << std::ends;
					mTrace->Trace(traceMsg.str());
				}

				ReleaseDevice();

				// attempt to reacquire it
				Sleep(5000);
				AcquireDevice(mDevSerialNumber);
				break;
			}
		}
	}

	if (!aborted)
		ServiceCommands();
}

unsigned int __stdcall
Monome40hFtw::DeviceServiceThread(void * _thisParam)
{
	Monome40hFtw * _this = static_cast<Monome40hFtw *>(_thisParam);
	_this->DeviceServiceThread();
	_endthreadex(0);
	return 0;
}

void
Monome40hFtw::ServiceCommands()
{
	AutoLockCs lock(mOutputCommandsLock);
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
Monome40hFtw::DispatchCommand(const MonomeSerialProtocolData * data)
{
	if (mServicingSubscribers)
	{
		DWORD curThreadId = ::GetCurrentThreadId();
		if (curThreadId == mThreadId)
		{
			// handle synchronously
			Send(*data);
			delete data;
			return;
		}
	}

	// queue to be serviced asynchronously
	AutoLockCs lock(mOutputCommandsLock);
	mOutputCommandQueue.push_back(data);
}
