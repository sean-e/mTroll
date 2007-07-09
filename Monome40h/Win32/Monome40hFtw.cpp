#include "Monome40hFtw.h"
#include <strstream>
#include <algorithm>
#include "../IMonome40h.h"
#include "../IMonome40hInputSubscriber.h"
#include "../../Engine/ITraceDisplay.h"
#include "../../Engine/ScopeSet.h"
#include "AutoLockCs.h"


Monome40hFtw::Monome40hFtw(ITraceDisplay * trace) :
	mTrace(trace),
	mFtDevice(INVALID_HANDLE_VALUE),
	mIsListening(false),
	mShouldContinueListening(true),
	mThread(NULL),
	mServicingSubscribers(false)
{
	::InitializeCriticalSection(&mSubscribersLock);
}

Monome40hFtw::~Monome40hFtw()
{
	Release();
	::DeleteCriticalSection(&mSubscribersLock);
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

	// startup listener
	mShouldContinueListening = true;

	return true;
}

void
Monome40hFtw::Release()
{
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

	mShouldContinueListening = false;
	Shutdown(true);

	::WaitForSingleObjectEx(mThread, 250, FALSE);
	_ASSERTE(!mIsListening);
	mIsListening = false;

	::FT_W32_CloseHandle(mFtDevice);
	mFtDevice = INVALID_HANDLE_VALUE;
}

bool
Monome40hFtw::Subscribe(IMonome40hInputSubscriber * sub)
{
	_ASSERTE(!mServicingSubscribers);
	AutoLockCs lock(mSubscribersLock);
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
	AutoLockCs lock(mSubscribersLock);
	InputSubscribers::iterator it = std::find(mInputSubscribers.begin(), mInputSubscribers.end(), sub);
	if (it != mInputSubscribers.end())
	{
		mInputSubscribers.erase(it);
		return true;
	}
	return false;
}

BOOL
Monome40hFtw::Send(const SerialProtocolData & data)
{
	_ASSERTE(mFtDevice && INVALID_HANDLE_VALUE != mFtDevice);
	DWORD bytesWritten = 0;
	BOOL retval = ::FT_W32_WriteFile(mFtDevice, (void*)data.mData, 2, &bytesWritten, NULL);
	_ASSERTE(retval && 2 == bytesWritten);
	return retval;
}

void
Monome40hFtw::EnableLed(byte row, 
						byte col, 
						bool on)
{
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

	SerialProtocolData data(SerialProtocolData::setLed, on ? 1 : 0, row, col);
	Send(data);
}

void
Monome40hFtw::SetLedIntensity(byte brightness)
{
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

	SerialProtocolData data(SerialProtocolData::setLedIntensity, 0, 0, brightness);
	Send(data);
}

void
Monome40hFtw::TestLed(bool on)
{
	_ASSERTE(mFtDevice && INVALID_HANDLE_VALUE != mFtDevice);
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

	SerialProtocolData data(SerialProtocolData::ledTest, 0, 0, on ? 1 : 0);
	Send(data);
}

void
Monome40hFtw::EnableAdc(byte port, 
						bool on)
{
	_ASSERTE(mFtDevice && INVALID_HANDLE_VALUE != mFtDevice);
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

	SerialProtocolData data(SerialProtocolData::enableAdc, 0, port, on ? 1 : 0);
	Send(data);
}

void
Monome40hFtw::Shutdown(bool state)
{
	_ASSERTE(mFtDevice && INVALID_HANDLE_VALUE != mFtDevice);
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

	SerialProtocolData data(SerialProtocolData::shutdown, 0, 0, state ? 1 : 0);
	Send(data);
}

void
Monome40hFtw::EnableLedRow(byte row, 
						   byte columnValues)
{
	_ASSERTE(mFtDevice && INVALID_HANDLE_VALUE != mFtDevice);
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

	SerialProtocolData data(SerialProtocolData::setLedRow, row);
	data[1] = columnValues;
	Send(data);
}

void
Monome40hFtw::EnableLedColumn(byte column, 
							  byte rowValues)
{
	_ASSERTE(mFtDevice && INVALID_HANDLE_VALUE != mFtDevice);
	if (INVALID_HANDLE_VALUE == mFtDevice)
		return;

	SerialProtocolData data(SerialProtocolData::setledColumn, column);
	data[1] = rowValues;
	Send(data);
}

void
Monome40hFtw::OnButtonChange(bool pressed, byte row, byte col)
{
	AutoLockCs lock(mSubscribersLock);
	ScopeSet<bool> active(&mServicingSubscribers, true);
	for (InputSubscribers::iterator it = mInputSubscribers.begin();
		it != mInputSubscribers.end();
		++it)
	{
		if (pressed)
			(*it)->SwitchPressed(row, col);
		else
			(*it)->SwitchReleased(row, col);
	}
}

void
Monome40hFtw::OnAdcChange(int port, int value)
{
	AutoLockCs lock(mSubscribersLock);
	ScopeSet<bool> active(&mServicingSubscribers, true);
	for (InputSubscribers::iterator it = mInputSubscribers.begin();
		it != mInputSubscribers.end();
		++it)
	{
		(*it)->AdcValueChanged(port, value);
	}
}
