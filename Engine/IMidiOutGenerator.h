#ifndef IMidiOutGenerator_h__
#define IMidiOutGenerator_h__


class IMidiOut;

// IMidiOutGenerator
// ----------------------------------------------------------------------------
// use to create IMidiOut
//
class IMidiOutGenerator
{
public:
	virtual IMidiOut *	GetMidiOut(unsigned int deviceIdx) = 0;
	virtual void		OpenMidiOuts() = 0;
	virtual void		CloseMidiOuts() = 0;
};

#endif // IMidiOutGenerator_h__
