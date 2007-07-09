#ifndef Monome40h_h__
#define Monome40h_h__

#include "IMonome40h.h"
#include <Windows.h>
#include "FTD2XX.H"
#include <list>

class ITraceDisplay;
class IMonome40hInputSubscriber;


class Monome40hFtw : public IMonome40h
{
public:
	Monome40hFtw(ITraceDisplay * trace);
	virtual ~Monome40hFtw();

public: // IMonome40h
	virtual void EnableLed(int row, int col, bool on);
	virtual void SetLedIntensity(int row, int col, int brightness);
	virtual void TestLed(bool on);
	virtual void EnableAdc(int port, bool on);
	virtual void Shutdown(bool state);
	virtual void EnableLedRow(int row, int columnValues);
	virtual void EnableLedColumn(int column, int rowValues);

	virtual bool Subscribe(IMonome40hInputSubscriber * sub);
	virtual bool Unsubscribe(IMonome40hInputSubscriber * sub);

	std::string GetDeviceSerialNumber(int devidx);
	bool Acquire(const std::string & devSerialNum);
	void Release();

private:
	typedef std::list<IMonome40hInputSubscriber *> InputSubscribers;
	InputSubscribers				mInputSubscribers;
	ITraceDisplay					* mTrace;
	FT_HANDLE						mFtDevice;
	bool							mServicingSubscribers;
};

#endif // Monome40h_h__
