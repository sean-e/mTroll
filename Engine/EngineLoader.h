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
class ExpressionPedals;
class IMonome40h;


class EngineLoader
{
public:
	EngineLoader(IMidiOutGenerator * midiOutGenerator, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay, ITraceDisplay * traceDisplay);

	MidiControlEngine *		CreateEngine(const std::string & engineSettingsFile);
	void					InitMonome(IMonome40h * monome);

private:
	bool					LoadSystemConfig(TiXmlElement * pElem);
	void					LoadExpressionPedalSettings(TiXmlElement * pElem, ExpressionPedals &pedals);
	void					LoadPatches(TiXmlElement * pElem);
	void					LoadBanks(TiXmlElement * pElem);

	typedef std::map<int, unsigned int> MidiOutPortToDeviceIdxMap;
	MidiOutPortToDeviceIdxMap	mMidiOutPortToDeviceIdxMap;
	enum AdcEnableState {adc_default, adc_used, adc_forceOn, adc_forceOff};
	AdcEnableState			mAdcEnables[4];

	MidiControlEngine *		mEngine;
	IMainDisplay *			mMainDisplay;
	IMidiOutGenerator *		mMidiOutGenerator;
	ISwitchDisplay *		mSwitchDisplay;
	ITraceDisplay *			mTraceDisplay;
};

#endif // EngineLoader_h__
