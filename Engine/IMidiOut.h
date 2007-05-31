#ifndef IMidiOut_h__
#define IMidiOut_h__

#include <string>
#include <vector>

typedef unsigned char byte;
typedef std::vector<byte> Bytes;


// IMidiOut
// ----------------------------------------------------------------------------
// use to send MIDI
//
class IMidiOut
{
public:
	virtual int GetDeviceCount() = 0;
	virtual std::string GetDeviceName(int deviceIdx) = 0;
	virtual bool OpenMidiOut(int deviceIdx) = 0;
	virtual bool MidiOut(const Bytes & bytes) = 0;
	virtual void CloseMidiOut() = 0;
};

#endif // IMidiOut_h__
