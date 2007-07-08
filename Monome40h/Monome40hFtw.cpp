#include "Monome40h.h"
#include <algorithm>
#include "IMonome40h.h"
#include "IMonome40hInputSubscriber.h"
#include "..\Engine\ITraceDisplay.h"


Monome40h::Monome40h(ITraceDisplay * trace) :
	mTrace(trace),
	mFtDevice(NULL),
	mServicingSubscribers(false)
{
}

Monome40h::~Monome40h()
{
	Shutdown(true);
}

bool
Monome40h::Subscribe(IMonome40hInputSubscriber * sub)
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
Monome40h::Unsubscribe(IMonome40hInputSubscriber * sub)
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

void
Monome40h::EnableLed(int led, 
					 bool on)
{

}

void
Monome40h::EnableLed(int row, 
					 int col, 
					 bool on)
{

}

void
Monome40h::SetLedIntensity(int led, 
						   int brightness)
{

}

void
Monome40h::SetLedIntensity(int row, 
						   int col, 
						   int brightness)
{

}

void
Monome40h::EnableLedRow(int row, 
						int columnValues)
{

}

void
Monome40h::EnableLedColumn(int column, 
						   int rowValues)
{

}

void
Monome40h::TestLed(bool on)
{

}

void
Monome40h::EnableAdc(int port, 
					 bool on)
{

}

void
Monome40h::Shutdown(bool state)
{

}
