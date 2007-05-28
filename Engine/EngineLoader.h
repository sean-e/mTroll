#ifndef EngineLoader_h__
#define EngineLoader_h__

#include <string>

class MidiControlEngine;
class IMainDisplay;
class ISwitchDisplay;
class ITraceDisplay;
class IMidiOut;
class TiXmlElement;


class EngineLoader
{
public:
	EngineLoader(IMidiOut * midiOut, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay, ITraceDisplay * traceDisplay);

	MidiControlEngine *		CreateEngine(const std::string & engineSettingsFile);

private:
	bool					LoadSystemConfig(TiXmlElement * pElem);
	void					LoadPatches(TiXmlElement * pElem);
	void					LoadBanks(TiXmlElement * pElem);

	MidiControlEngine *		mEngine;
	IMainDisplay *			mMainDisplay;
	IMidiOut *				mMidiOut;
	ISwitchDisplay *		mSwitchDisplay;
	ITraceDisplay *			mTraceDisplay;
};

#endif // EngineLoader_h__
