#ifndef IMonome40h_h__
#define IMonome40h_h__

class IMonome40hInputSubscriber;

// IMonome40h
// ----------------------------------------------------------------------------
// Implemented by a monome 40h provider.
//
class IMonome40h
{
	// monome 40h controls
	virtual void EnableLed(int row, int col, bool on) = 0;
	virtual void SetLedIntensity(int row, int col, int brightness) = 0;
	virtual void EnableLedRow(int row, int columnValues) = 0;
	virtual void EnableLedColumn(int column, int rowValues) = 0;
	virtual void TestLed(bool on) = 0;
	virtual void EnableAdc(int port, bool on) = 0;
	virtual void Shutdown(bool state) = 0;

	// input notification
	virtual bool Subscribe(IMonome40hInputSubscriber * sub) = 0;
	virtual bool Unsubscribe(IMonome40hInputSubscriber * sub) = 0;
};

inline int RowFromOrdinal(int ord) {return ord / 8;}
inline int ColumnFromOrdinal(int ord) {return ord % 8;}
inline void RowColFromOrdinal(int ord, int & row, int & col) {row = RowFromOrdinal(ord); col = ColumnFromOrdinal(ord);}
inline int OrdinalFromRowCol(int row, int col) {return (row * 8) + col;}

#endif // IMonome40h_h__
