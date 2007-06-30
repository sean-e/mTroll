#ifndef EngineLoader_h__
#define EngineLoader_h__

#include <string>
#include <map>

class MidiControlEngine;
class IMainDisplay;
class ISwitchDisplay;
class ITraceDisplay;
class IMidiOutGenerator;
class TiXmlElement;


class EngineLoader
{
public:
	EngineLoader(IMidiOutGenerator * midiOutGenerator, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay, ITraceDisplay * traceDisplay);

	MidiControlEngine *		CreateEngine(const std::string & engineSettingsFile);

private:
	bool					LoadSystemConfig(TiXmlElement * pElem);
	void					LoadPatches(TiXmlElement * pElem);
	void					LoadBanks(TiXmlElement * pElem);

	typedef std::map<int, unsigned int> MidiOutPortToDeviceIdxMap;
	MidiOutPortToDeviceIdxMap	mMidiOutPortToDeviceIdxMap;

	MidiControlEngine *		mEngine;
	IMainDisplay *			mMainDisplay;
	IMidiOutGenerator *		mMidiOutGenerator;
	ISwitchDisplay *		mSwitchDisplay;
	ITraceDisplay *			mTraceDisplay;
};

#endif // EngineLoader_h__
