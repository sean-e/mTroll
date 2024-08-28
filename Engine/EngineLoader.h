/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2010,2018,2020,2021,2023-2024 Sean Echevarria
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
#include <memory>
#include <array>
#include <set>
#include <vector>
#include "ExpressionPedals.h"
#include "IAxeFx.h"

class MidiControlEngine;
class ITrollApplication;
class IMainDisplay;
class ISwitchDisplay;
class ITraceDisplay;
class IMidiOutGenerator;
class IMidiInGenerator;
class TiXmlElement;
class IMonome40h;
class AxeFxManager;
class AxeFx3Manager;
class EdpManager;

using MidiControlEnginePtr = std::shared_ptr<MidiControlEngine>;
using AxeFxManagerPtr = std::shared_ptr<AxeFxManager>;
using AxeFx3ManagerPtr = std::shared_ptr<AxeFx3Manager>;
using EdpManagerPtr = std::shared_ptr<EdpManager>;
using MidiPortToDeviceIdxMap = std::map<int, unsigned int>;

constexpr unsigned int kFirstColorPreset = 0x80000000;
constexpr unsigned int kPresetColorMarkerBit = 0x80000000;


class EngineLoader
{
public:
	EngineLoader(ITrollApplication * app, IMidiOutGenerator * midiOutGenerator, IMidiInGenerator * midiInGenerator, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay, ITraceDisplay * traceDisplay);
	~EngineLoader() = default;

	MidiControlEnginePtr	CreateEngine(const std::string & engineSettingsFile);
	void					InitMonome(IMonome40h * monome, 
										const bool adcOverrides[ExpressionPedals::PedalCount],
										bool userAdcSettings[ExpressionPedals::PedalCount]);

private:
	bool					LoadSystemConfig(TiXmlElement * pElem);
	void					LoadDeviceChannelMap(TiXmlElement * pElem);
	void					InitDefaultLedPresetColors();
	void					LoadLedPresetColors(TiXmlElement * pElem);
	void					LoadLedDefaultColors(TiXmlElement * pElem);
	void					LoadExpressionPedalSettings(TiXmlElement * pElem, ExpressionPedals &pedals, int defaultChannel);
	void					GenerateDefaultNotePatches();
	void					GeneratePatchNumbers(TiXmlElement* pElem);
	void					LoadPatches(TiXmlElement * pElem);
	void					LoadElementColorAttributes(TiXmlElement * pElem, unsigned int &ledActiveColor, unsigned int &ledInactiveColor);

	IAxeFxPtr				GetAxeMgr(TiXmlElement * pElem);
	void					LoadBanks(TiXmlElement * pElem);
	void					LoadSetOrder(TiXmlElement * pElem, std::vector<std::string> &setorder);
	unsigned int			LookUpColor(std::string device, std::string patchType, int activeState, unsigned int defaultColor = kFirstColorPreset);
	int						GetPatchNumber(const std::string& name) const;
	bool					IsPatchNumberUsed(int num) const;
	unsigned int			GetMidiOutDeviceIndex(std::string outDevice);
	unsigned int			GetMidiInDeviceIndex(std::string inDevice);

	MidiPortToDeviceIdxMap	mMidiOutPortToDeviceIdxMap;
	MidiPortToDeviceIdxMap	mMidiInPortToDeviceIdxMap;
	enum AdcEnableState {adc_default, adc_used, adc_forceOn, adc_forceOff};
	AdcEnableState			mAdcEnables[ExpressionPedals::PedalCount];
	PedalCalibration		mAdcCalibration[ExpressionPedals::PedalCount];

	MidiControlEnginePtr	mEngine;
	ITrollApplication *		mApp;
	IMainDisplay *			mMainDisplay;
	IMidiOutGenerator *		mMidiOutGenerator;
	IMidiInGenerator *		mMidiInGenerator;
	ISwitchDisplay *		mSwitchDisplay;
	ITraceDisplay *			mTraceDisplay;
	AxeFxManagerPtr			mAxeFxManager;
	AxeFx3ManagerPtr		mAxeFx3Manager;
	EdpManagerPtr			mEdpManager;
	std::map<std::string, std::string> mDeviceChannels; // outboard device channels
	std::map<std::string, int> mDevicePorts; // computer midi ports used to address outboard devices
	std::string				mAxeDeviceName;
	std::string				mAxe3DeviceName;
	int						mAxeSyncPort = -1;
	int						mAxe3SyncPort = -1;
	int						mEdpPort = -1;
	std::string				mPowerupBankName;
	std::array<unsigned int, 32> mLedPresetColors;
	// device/patchType/state --> rgb color (or preset slot 0-31 if hi-bit set)
	std::map<std::tuple<std::string, std::string, int>, unsigned int> mLedDefaultColors;

	using Patches = std::map<const std::string, int>;
	Patches					mPatchMap;
	std::set<int>			mPatchNumbersUsed;
};

#endif // EngineLoader_h__
