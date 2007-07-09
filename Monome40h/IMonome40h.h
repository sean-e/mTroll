#ifndef IMonome40h_h__
#define IMonome40h_h__

class IMonome40hInputSubscriber;
typedef unsigned char byte;


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
	virtual bool Subscribe(IMonome40hInputSubscriber * sub) = 0;
	virtual bool Unsubscribe(IMonome40hInputSubscriber * sub) = 0;

protected:
	IMonome40h() {}
	IMonome40h(const IMonome40h & rhs);
};

inline byte RowFromOrdinal(int ord) {return ord / 8;}
inline byte ColumnFromOrdinal(int ord) {return ord % 8;}
inline void RowColFromOrdinal(int ord, byte & row, byte & col) {row = RowFromOrdinal(ord); col = ColumnFromOrdinal(ord);}
inline int OrdinalFromRowCol(byte row, byte col) {return (row * 8) + col;}

#endif // IMonome40h_h__
