/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2009 Sean Echevarria
 *
 * This file is part of mTroll.
 *
 * mTroll is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mTroll is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Let me know if you modify, extend or use mTroll.
 * Original project site: http://www.creepingfog.com/mTroll/
 * Contact Sean: "fester" at the domain of the original project site
 */

#ifndef EngineLoader_h__
#define EngineLoader_h__

#include <string>
#include <map>
#include "ExpressionPedals.h"

class MidiControlEngine;
class IMainDisplay;
class ISwitchDisplay;
class ITraceDisplay;
class IMidiOutGenerator;
class TiXmlElement;
class IMonome40h;


class EngineLoader
{
public:
	EngineLoader(IMidiOutGenerator * midiOutGenerator, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay, ITraceDisplay * traceDisplay);

	MidiControlEngine *		CreateEngine(const std::string & engineSettingsFile);
	void					InitMonome(IMonome40h * monome, 
										const bool adcOverrides[ExpressionPedals::PedalCount],
										bool userAdcSettings[ExpressionPedals::PedalCount]);

private:
	bool					LoadSystemConfig(TiXmlElement * pElem);
	void					LoadDeviceChannelMap(TiXmlElement * pElem);
	void					LoadExpressionPedalSettings(TiXmlElement * pElem, ExpressionPedals &pedals);
	void					LoadPatches(TiXmlElement * pElem);
	void					LoadBanks(TiXmlElement * pElem);

	typedef std::map<int, unsigned int> MidiOutPortToDeviceIdxMap;
	MidiOutPortToDeviceIdxMap	mMidiOutPortToDeviceIdxMap;
	enum AdcEnableState {adc_default, adc_used, adc_forceOn, adc_forceOff};
	AdcEnableState			mAdcEnables[ExpressionPedals::PedalCount];
	PedalCalibration		mAdcCalibration[ExpressionPedals::PedalCount];

	MidiControlEngine *		mEngine;
	IMainDisplay *			mMainDisplay;
	IMidiOutGenerator *		mMidiOutGenerator;
	ISwitchDisplay *		mSwitchDisplay;
	ITraceDisplay *			mTraceDisplay;
	std::map<std::string, std::string> mDevices;
};

#endif // EngineLoader_h__
