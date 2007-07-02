#ifndef IMidiOut_h__
#define IMidiOut_h__

#include <string>
#include <vector>

typedef unsigned char byte;
typedef std::vector<byte> Bytes;
class ISwitchDisplay;


// IMidiOut
// ----------------------------------------------------------------------------
// use to send MIDI
//
class IMidiOut
{
public:
	virtual unsigned int GetMidiOutDeviceCount() const = 0;
	virtual std::string GetMidiOutDeviceName(unsigned int deviceIdx) const = 0;
	virtual void SetActivityIndicator(ISwitchDisplay * activityIndicator, int activityIndicatorIdx) = 0;
	virtual bool OpenMidiOut(unsigned int deviceIdx) = 0;
	virtual bool IsMidiOutOpen() const = 0;
	virtual bool MidiOut(const Bytes & bytes) = 0;
	virtual void CloseMidiOut() = 0;
};

#endif // IMidiOut_h__
