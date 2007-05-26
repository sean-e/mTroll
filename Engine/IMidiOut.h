#pragma once

// IMidiOut
// ----------------------------------------------------------------------------
// use to send MIDI
//
class IMidiOut
{
public:
	virtual bool MidiOut(byte * bytes) = 0;
};
