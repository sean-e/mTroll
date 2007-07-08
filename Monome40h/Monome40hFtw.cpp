#include "Monome40hFtw.h"
#include <algorithm>
#include "IMonome40h.h"
#include "IMonome40hInputSubscriber.h"
#include "..\Engine\ITraceDisplay.h"


Monome40hFtw::Monome40hFtw(ITraceDisplay * trace) :
	mTrace(trace),
	mFtDevice(NULL),
	mServicingSubscribers(false)
{
}

Monome40hFtw::~Monome40hFtw()
{
	Shutdown(true);
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

void
Monome40hFtw::EnableLed(int row, 
						int col, 
						bool on)
{

}

void
Monome40hFtw::SetLedIntensity(int row, 
							  int col, 
							  int brightness)
{

}

void
Monome40hFtw::EnableLedRow(int row, 
						   int columnValues)
{

}

void
Monome40hFtw::EnableLedColumn(int column, 
							  int rowValues)
{

}

void
Monome40hFtw::TestLed(bool on)
{

}

void
Monome40hFtw::EnableAdc(int port, 
						bool on)
{

}

void
Monome40hFtw::Shutdown(bool state)
{

}
