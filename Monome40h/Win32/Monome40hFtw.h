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

#ifndef Monome40h_h__
#define Monome40h_h__

#include <Windows.h>
#include <list>
#include "../IMonome40h.h"
#include "../FTD2XX.H"

class ITraceDisplay;
class IMonome40hInputSubscriber;
class MonomeSerialProtocolData;


class Monome40hFtw : public IMonome40h
{
public:
	Monome40hFtw(ITraceDisplay * trace);
	virtual ~Monome40hFtw();

public: // IMonome40h
	virtual void EnableLed(byte row, byte col, bool on);
	virtual void SetLedIntensity(byte brightness);
	virtual void TestLed(bool on);
	virtual void EnableAdc(byte port, bool on);
	virtual void Shutdown(bool state);
	virtual void EnableLedRow(byte row, byte columnValues);
	virtual void EnableLedColumn(byte column, byte rowValues);

	virtual bool Subscribe(IMonome40hInputSubscriber * sub);
	virtual bool Unsubscribe(IMonome40hInputSubscriber * sub);

	int LocateMonomeDeviceIdx();
	std::string GetDeviceSerialNumber(int devidx);
	bool AcquireDevice(const std::string & devSerialNum);
	void ReleaseDevice();

private:
	void OnButtonChange(bool pressed, byte row, byte col);
	void OnAdcChange(int port, int value);
	BOOL Send(const MonomeSerialProtocolData & data);
	static unsigned int __stdcall ReadThread(void * _this);
	void ReadThread();
	void ReadInput(byte * readData);

	typedef std::list<IMonome40hInputSubscriber *> InputSubscribers;
	CRITICAL_SECTION				mSubscribersLock;
	InputSubscribers				mInputSubscribers;
	ITraceDisplay					* mTrace;
	FT_HANDLE						mFtDevice;
	HANDLE							mThread;
	bool							mServicingSubscribers;
	bool							mIsListening;
	bool							mShouldContinueListening;
};

#endif // Monome40h_h__