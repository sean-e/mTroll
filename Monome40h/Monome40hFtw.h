#ifndef Monome40h_h__
#define Monome40h_h__

#include "IMonome40h.h"
#include <Windows.h>
#include "FTD2XX.H"
#include <list>

class ITraceDisplay;
class IMonome40hInputSubscriber;


class Monome40h : public IMonome40h
{
public:
	Monome40h(ITraceDisplay * trace);
	virtual ~Monome40h();

public: // IMonome40h
	// output address 2
	virtual void EnableLed(int led, bool on);
	virtual void EnableLed(int row, int col, bool on);
	// output address 3
	virtual void SetLedIntensity(int led, int brightness);
	virtual void SetLedIntensity(int row, int col, int brightness);
	// output address 7
	virtual void EnableLedRow(int row, int columnValues);
	// output address 8
	virtual void EnableLedColumn(int column, int rowValues);
	// output address 4
	virtual void TestLed(bool on);
	// output address 5
	virtual void EnableAdc(int port, bool on);
	// output address 6
	virtual void Shutdown(bool state);

	virtual bool Subscribe(IMonome40hInputSubscriber * sub);
	virtual bool Unsubscribe(IMonome40hInputSubscriber * sub);

private:
	typedef std::list<IMonome40hInputSubscriber *> InputSubscribers;
	InputSubscribers				mInputSubscribers;
	ITraceDisplay					* mTrace;
	FT_HANDLE						mFtDevice;
	bool							mServicingSubscribers;
};

#endif // Monome40h_h__
