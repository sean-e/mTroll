/*
Original code copyright (c) 2007,2014-2015,2020,2026 Sean Echevarria ( http://www.creepingfog.com/sean/ )

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
		// 4 bit command identifiers
		getPress				= 0,
		getAdcVal				= 1,
		setLed					= 2,
		setPixelIndex			= 3,
		ledTest					= 4,
		enableAdc				= 5,
		shutdown				= 6,
		setLedRow				= 7,
		setledColumn			= 8,
		setLedRgbOn,			// enable LED using specified RGB value (5 byte message)
		updatePresetGroup1,		// set preset color slot to specified RGB value (for slots 0 - 15) (4 byte message)
		setLedOnPresetGroup1,	// enable LED using preset color slot 0-15 (2 byte message)
		updatePresetGroup2,		// set preset color slot to specified RGB value (slots 16 - 31 specified as 0 - 15) (4 byte message)
		setLedOnPresetGroup2,	// enable LED using preset color slot 16-31 (specified as 0 - 15) (2 byte message)
		invalidateAllPixels		// in lieu of using setPixelIndex with a value that can't be sent
		// Reserve value 15 to use as marker for 8 bit identifiers -- requires update of firmware to support 8 bit identifiers
	};

protected:
	byte mData[5];
	const int mLen = 2;

	MonomeSerialProtocolData(ProtocolCommand command)
	{
		mData[0] = (command & 0x0f) << 4;
		mData[1] = 0;
	}

	MonomeSerialProtocolData(ProtocolCommand command, byte data1)
	{
		mData[0] = ((command & 0x0f) << 4) | (data1 & 0x0f);
		mData[1] = 0;
	}

	MonomeSerialProtocolData(ProtocolCommand command, byte data1, byte data2)
	{
		mData[0] = ((command & 0x0f) << 4) | (data1 & 0x0f);
		mData[1] = (data2 & 0x0f) << 4;
	}

	MonomeSerialProtocolData(ProtocolCommand command, byte data1, byte data2, byte data3)
	{
		if (setPixelIndex == command)
		{
			// 	kMessageTypePixelIndexToXY message bits
			// 		4 message type
			// 		6 pixel index 0-63
			// 		3 row 0-7
			// 		3 col 0-7
			// 
			// 	4 bits message type : 6 bits pixel ID (0-63) : 3 bits X (0-7) : 3 bits Y (0-7)
			// 	the 6 bits pixel ID are 4 bits lsb in data0 and 2 bits msb in data1
			// 	
			// 	Read in monome firmware as:
			//  i1 = (serial_in.data0 & 0xF) | ((serial_in.data1 >> 2) & 0x30); // index
			// 	i2 = (serial_in.data1 >> 3) & 0x7; // X, col, data3
			// 	i3 = serial_in.data1 & 0x7; // Y, row, data2
			// 	kLedMatrix[i3][i2] = i1;
			mData[0] = ((command & 0x0f) << 4) | (data1 & 0x0f);
			mData[1] = ((data1 & 0x30) << 2) | ((data3 & 0x07) << 3) | (data2 & 0x07);
		}
		else
		{
			mData[0] = ((command & 0x0f) << 4) | (data1 & 0x0f);
			mData[1] = ((data2 & 0x0f) << 4) | (data3 & 0x0f);
		}
	}

	// updatePresetGroup1 and updatePresetGroup2
	explicit MonomeSerialProtocolData(ProtocolCommand command, byte data1, unsigned int data2) : mLen(4)
	{
		mData[0] = ((command & 0x0f) << 4) | (data1 & 0x0f); // data1 is the preset slot number
		mData[2] = (data2 >> 16) & 0xff; // R
		mData[1] = (data2 >> 8) & 0xff; // G
		mData[3] = data2 & 0xff; // B
	}

	// setLedRgbOn
	explicit MonomeSerialProtocolData(ProtocolCommand command, byte data1, byte data2, unsigned int data3) : mLen(5)
	{
		mData[0] = ((command & 0x0f) << 4);
		mData[1] = ((data1 & 0x0f) << 4) | (data2 & 0x0f);
		mData[3] = (data3 >> 16) & 0xff; // R
		mData[2] = (data3 >> 8) & 0xff; // G
		mData[4] = data3 & 0xff; // B
	}

public:
	const byte * Data() const {return mData;}
	int DataLen() const { return mLen; }

private:
	MonomeSerialProtocolData() = delete;
};

class MonomeSetLed : public MonomeSerialProtocolData
{
public:
	MonomeSetLed(bool enable, byte row, byte col) : 
		MonomeSerialProtocolData(MonomeSerialProtocolData::setLed, enable ? 1 : 0, col, row) { }
};

class MonomeLedOnRgb : public MonomeSerialProtocolData
{
public:
	MonomeLedOnRgb(byte row, byte col, unsigned int color) :
		MonomeSerialProtocolData(MonomeSerialProtocolData::setLedRgbOn, col, row, color) { }
};

class MonomeLedOnPresetGroup1 : public MonomeSerialProtocolData
{
public:
	MonomeLedOnPresetGroup1(byte preset, byte row, byte col) :
		MonomeSerialProtocolData(MonomeSerialProtocolData::setLedOnPresetGroup1, preset, col, row) { }
};

class MonomeLedOnPresetGroup2 : public MonomeSerialProtocolData
{
public:
	MonomeLedOnPresetGroup2(byte preset, byte row, byte col) :
		MonomeSerialProtocolData(MonomeSerialProtocolData::setLedOnPresetGroup2, preset, col, row) { }
};

class MonomeSetPixelRowCol : public MonomeSerialProtocolData
{
public:
	MonomeSetPixelRowCol(byte pixel, byte row, byte col) :
		MonomeSerialProtocolData(MonomeSerialProtocolData::setPixelIndex, pixel, row, col) { }
};

class MonomeInvalidateAllPixels : public MonomeSerialProtocolData
{
public:
	MonomeInvalidateAllPixels() :
		MonomeSerialProtocolData(MonomeSerialProtocolData::invalidateAllPixels) { }
};

class MonomeTestLed : public MonomeSerialProtocolData
{
public:
	MonomeTestLed(int pattern) :
		MonomeSerialProtocolData(MonomeSerialProtocolData::ledTest, 0, 0, (byte)(pattern)) { }
};

class MonomeEnableAdc : public MonomeSerialProtocolData
{
public:
	MonomeEnableAdc(int port, bool enable) :
		MonomeSerialProtocolData(MonomeSerialProtocolData::enableAdc, 0, port, (byte)(enable ? 1 : 0)) { }
};

class MonomeShutdown : public MonomeSerialProtocolData
{
public:
	MonomeShutdown(bool state) : 
		MonomeSerialProtocolData(MonomeSerialProtocolData::shutdown, 0, 0, (byte)(state ? 1 : 0)) { }
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

class MonomeUpdatePresetGroup1 : public MonomeSerialProtocolData
{
public:
	MonomeUpdatePresetGroup1(byte preset, unsigned int color) :
		MonomeSerialProtocolData(MonomeSerialProtocolData::updatePresetGroup1, preset, color) { }
};

class MonomeUpdatePresetGroup2 : public MonomeSerialProtocolData
{
public:
	MonomeUpdatePresetGroup2(byte preset, unsigned int color) :
		MonomeSerialProtocolData(MonomeSerialProtocolData::updatePresetGroup2, preset, color) { }
};

#endif // MonomeSerialProtocol_h__
