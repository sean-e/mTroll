#ifndef Monome40h_h__
#define Monome40h_h__

#include <Windows.h>
#include <list>
#include "../IMonome40h.h"
#include "../FTD2XX.H"

class ITraceDisplay;
class IMonome40hInputSubscriber;


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

	std::string GetDeviceSerialNumber(int devidx);
	bool AcquireDevice(const std::string & devSerialNum);
	void ReleaseDevice();

private:
	void OnButtonChange(bool pressed, byte row, byte col);
	void OnAdcChange(int port, int value);

	struct MonomeSerialProtocolData
	{
		enum ProtocolCommand
		{
			getPress				= 0,
			getAdcVal				= 1,
			setLed					= 2,
			setLedIntensity			= 3,
			ledTest					= 4,
			enableAdc				= 5,
			shutdown				= 6,
			setLedRow				= 7,
			setledColumn			= 8
		};

		byte mData[2];

		MonomeSerialProtocolData(ProtocolCommand command)
		{
			mData[0] = (command & 0x0f) << 4;
		}

		MonomeSerialProtocolData(ProtocolCommand command, byte data1)
		{
			mData[0] = ((command & 0x0f) << 4) | (data1 & 0x0f);
		}

		MonomeSerialProtocolData(ProtocolCommand command, byte data1, byte data2)
		{
			mData[0] = ((command & 0x0f) << 4) | (data1 & 0x0f);
			mData[1] = (data2 & 0x0f) << 4;
		}

		MonomeSerialProtocolData(ProtocolCommand command, byte data1, byte data2, byte data3)
		{
			mData[0] = ((command & 0x0f) << 4) | (data1 & 0x0f);
			mData[1] = ((data2 & 0x0f) << 4) | (data3 & 0x0f);
		}

		byte & operator[](int idx) {_ASSERTE(0 == idx || 1 == idx); return mData[idx];}
	};

	BOOL Send(const MonomeSerialProtocolData & data);

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
