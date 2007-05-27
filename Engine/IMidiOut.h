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
	virtual bool MidiOut(const Bytes & bytes) = 0;
};
