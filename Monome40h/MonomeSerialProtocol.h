#ifndef MonomeSerialProtocol_h__
#define MonomeSerialProtocol_h__


class MonomeSerialProtocolData
{
protected:
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

public:
	const byte * Data() const {return mData;}

private:
	MonomeSerialProtocolData();
};

class MonomeSetLed : public MonomeSerialProtocolData
{
public:
	MonomeSetLed(bool enable, byte row, byte col) : 
		MonomeSerialProtocolData(MonomeSerialProtocolData::setLed, enable ? 1 : 0, row, col) { }
};

class MonomeSetLedIntensity : public MonomeSerialProtocolData 
{
public:
	MonomeSetLedIntensity(byte intensity) :
		MonomeSerialProtocolData(MonomeSerialProtocolData::setLedIntensity, 0, 0, intensity) { }
};

class MonomeTestLed : public MonomeSerialProtocolData
{
public:
	MonomeTestLed(bool enable) :
		MonomeSerialProtocolData(MonomeSerialProtocolData::ledTest, 0, 0, enable ? 1 : 0) { }
};

class MonomeEnableAdc : public MonomeSerialProtocolData
{
public:
	MonomeEnableAdc(int port, bool enable) :
		MonomeSerialProtocolData(MonomeSerialProtocolData::enableAdc, 0, port, enable ? 1 : 0) { }
};

class MonomeShutdown : public MonomeSerialProtocolData
{
public:
	MonomeShutdown(bool state) : 
		MonomeSerialProtocolData(MonomeSerialProtocolData::shutdown, 0, 0, state ? 1 : 0) { }
};

class MonomeSetLedRow : public MonomeSerialProtocolData
{
public:
	MonomeSetLedRow(byte row, byte columnValues) :
		MonomeSerialProtocolData(MonomeSerialProtocolData::setLedRow, row) 
	{ 
		mData[1] = columnValues;
	}
};

class MonomeSetLedColumn : public MonomeSerialProtocolData
{
public:
	MonomeSetLedColumn(byte column, byte rowValues) :
		MonomeSerialProtocolData(MonomeSerialProtocolData::setledColumn, column) 
	{ 
		mData[1] = rowValues;
	}
};

#endif // MonomeSerialProtocol_h__
