#pragma once

#include <vector>

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
	virtual bool Open(int deviceIdx) = 0;
	virtual bool MidiOut(const Bytes & bytes) = 0;
	virtual void Close() = 0;
};
