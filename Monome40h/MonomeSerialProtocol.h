/*
Original code copyright (c) 2007,2014-2015 Sean Echevarria ( http://www.creepingfog.com/sean/ )

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

#ifndef MonomeSerialProtocol_h__
#define MonomeSerialProtocol_h__


class MonomeSerialProtocolData
{
public:
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

protected:
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

public:
	const byte * Data() const {return mData;}

private:
	MonomeSerialProtocolData();
};

class MonomeSetLed : public MonomeSerialProtocolData
{
public:
#ifdef PER_LED_INTENSITY
	MonomeSetLed(byte intensity, byte row, byte col) : 
		MonomeSerialProtocolData(MonomeSerialProtocolData::setLed, intensity, col, row) { }
#else
	MonomeSetLed(bool enable, byte row, byte col) : 
		MonomeSerialProtocolData(MonomeSerialProtocolData::setLed, enable ? 1 : 0, col, row) { }
#endif // PER_LED_INTENSITY
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
