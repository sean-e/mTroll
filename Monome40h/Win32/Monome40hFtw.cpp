#include "Monome40hFtw.h"
#include <strstream>
#include <algorithm>
#include "../IMonome40h.h"
#include "../IMonome40hInputSubscriber.h"
#include "../../Engine/ITraceDisplay.h"


Monome40hFtw::Monome40hFtw(ITraceDisplay * trace) :
	mTrace(trace),
	mFtDevice(INVALID_HANDLE_VALUE),
	mServicingSubscribers(false)
{
}

Monome40hFtw::~Monome40hFtw()
{
	Release();
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
			traceMsg << "ERROR: Failed to get FT device list: " << ftStatus << std::endl << std::ends;
			mTrace->Trace(traceMsg.str());
		}
		return "";
	}

	if (devIndex >= numDevs)
	{
		if (mTrace)
		{
			traceMsg << "ERROR: Requested FT device idx is greater than installed dev count: " << numDevs << std::endl << std::ends;
			mTrace->Trace(traceMsg.str());
		}
		return "";
	}

	char serialNo[64];
	ftStatus = ::FT_ListDevices((PVOID)devIndex, serialNo, FT_LIST_BY_INDEX|FT_OPEN_BY_SERIAL_NUMBER);
	if (FT_OK != ftStatus)
	{
		traceMsg << "ERROR: Failed to get FT device " << devIndex << " serial, status: " << ftStatus << std::endl << std::ends;
		mTrace->Trace(traceMsg.str());
		return "";
	}

	return serialNo;
}

bool
Monome40hFtw::Acquire(const std::string & devSerialNum)
{
	std::strstream traceMsg;
	mFtDevice = ::FT_W32_CreateFile(devSerialNum.c_str(), 
		GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 
		FILE_ATTRIBUTE_NORMAL|FT_OPEN_BY_SERIAL_NUMBER, NULL);
	if (INVALID_HANDLE_VALUE == mFtDevice)
	{
		if (mTrace)
		{
			traceMsg << "ERROR: Failed to open FT device " << devSerialNum << std::endl << std::ends;
			mTrace->Trace(traceMsg.str());
		}
		return false;
	}

	if (mTrace)
	{
		traceMsg << "Opened FT device " << devSerialNum << std::endl << std::ends;
		mTrace->Trace(traceMsg.str());
	}

	return true;
}

void
Monome40hFtw::Release()
{
	Shutdown(true);
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

	::FT_W32_CloseHandle(mFtDevice);
	mFtDevice = INVALID_HANDLE_VALUE;
}

bool
Monome40hFtw::Subscribe(IMonome40hInputSubscriber * sub)
{
	_ASSERTE(!mServicingSubscribers);
	InputSubscribers::iterator it = std::find(mInputSubscribers.begin(), mInputSubscribers.end(), sub);
	if (it == mInputSubscribers.end())
	{
		mInputSubscribers.push_back(sub);
		return true;
	}
	return false;
}

bool
Monome40hFtw::Unsubscribe(IMonome40hInputSubscriber * sub)
{
	_ASSERTE(!mServicingSubscribers);
	InputSubscribers::iterator it = std::find(mInputSubscribers.begin(), mInputSubscribers.end(), sub);
	if (it != mInputSubscribers.end())
	{
		mInputSubscribers.erase(it);
		return true;
	}
	return false;
}

// output address 2
void
Monome40hFtw::EnableLed(int row, 
						int col, 
						bool on)
{
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

}

// output address 3
void
Monome40hFtw::SetLedIntensity(int row, 
							  int col, 
							  int brightness)
{
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

}

// output address 4
void
Monome40hFtw::TestLed(bool on)
{
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

}

// output address 5
void
Monome40hFtw::EnableAdc(int port, 
						bool on)
{
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

}

// output address 6
void
Monome40hFtw::Shutdown(bool state)
{
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

}

// output address 7
void
Monome40hFtw::EnableLedRow(int row, 
						   int columnValues)
{
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

}

// output address 8
void
Monome40hFtw::EnableLedColumn(int column, 
							  int rowValues)
{
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

}
