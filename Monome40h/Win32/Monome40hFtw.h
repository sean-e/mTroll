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
