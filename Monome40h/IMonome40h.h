/*
Original code copyright (c) 2007-2008,2014,2018,2020 Sean Echevarria ( http://www.creepingfog.com/sean/ )

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

#ifndef IMonome40h_h__
#define IMonome40h_h__

class IMonome40hSwitchSubscriber;
class IMonome40hAdcSubscriber;
using byte = unsigned char;


// IMonome40h
// ----------------------------------------------------------------------------
// Implemented by a monome 40h provider.
//
class IMonome40h
{
public:
	virtual ~IMonome40h() {}

	// monome 40h controls
	virtual void EnableLed(byte row, byte col, bool on) = 0;
	virtual void SetLedIntensity(byte brightness) = 0;
	virtual void EnableLedRow(byte row, byte columnValues) = 0;
	virtual void EnableLedColumn(byte column, byte rowValues) = 0;
	virtual void TestLed(bool on) = 0;
	virtual void EnableAdc(byte port, bool on) = 0;
	virtual void Shutdown(bool state) = 0;

	// input notification
	virtual bool Subscribe(IMonome40hSwitchSubscriber * sub) = 0;
	virtual bool Unsubscribe(IMonome40hSwitchSubscriber * sub) = 0;
	virtual bool Subscribe(IMonome40hAdcSubscriber * sub) = 0;
	virtual bool Unsubscribe(IMonome40hAdcSubscriber * sub) = 0;

	// device identification and acquisition
	virtual int LocateMonomeDeviceIdx() = 0;
	virtual std::string GetDeviceSerialNumber(int devidx) = 0;
	virtual bool AcquireDevice(const std::string & devSerialNum) = 0;

	virtual bool IsAdcEnabled(int portIdx) const = 0;

protected:
	IMonome40h() {}
	IMonome40h(const IMonome40h & rhs);
};

// row = y, col = x
// inline byte RowFromOrdinal(int ord) {return ord / 8;}
// inline byte ColumnFromOrdinal(int ord) {return ord % 8;}
// inline void RowColFromOrdinal(int ord, byte & row, byte & col) {row = RowFromOrdinal(ord); col = ColumnFromOrdinal(ord);}
// inline int OrdinalFromRowCol(byte row, byte col) {return (row * 8) + col;}

#endif // IMonome40h_h__
