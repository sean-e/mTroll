/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2013,2015,2018,2020-2022,2024-2025 Sean Echevarria
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

#ifndef MidiControlEngine_h__
#define MidiControlEngine_h__

#include <map>
#include <vector>
#include <stack>
#include <memory>
#include <list>

#include "Patch.h"
#include "ExpressionPedals.h"
#include "../Monome40h/IMonome40hInputSubscriber.h"
#include "IAxeFx.h"
#include "EngineLoader.h"
#include "EdpManager.h"


class ITrollApplication;
class IMainDisplay;
class IMidiOutGenerator;
class ISwitchDisplay;
class ITraceDisplay;
class IMidiOut;
class Patch;
class PatchBank;
class ControllerInputMonitor;
class TwoStatePatch;

using PatchBankPtr = std::shared_ptr<PatchBank>;
using IMidiOutPtr = std::shared_ptr<IMidiOut>;
using ControllerInputMonitorPtr = std::shared_ptr<ControllerInputMonitor>;


class MidiControlEngine : 
	public IMonome40hAdcSubscriber,
	public std::enable_shared_from_this<MidiControlEngine>
{
public:
	MidiControlEngine(ITrollApplication * app,
					  IMainDisplay * mainDisplay, 
					  ISwitchDisplay * switchDisplay,
					  ITraceDisplay * traceDisplay,
					  IMidiOutGenerator * midiOutGenerator,
					  IMidiOutPtr midiOut,
					  IAxeFxPtr axMgr,
					  IAxeFxPtr ax3Mgr,
					  EdpManagerPtr edpMgr,
					  int incrementSwitchNumber,
					  int decrementSwitchNumber,
					  int modeSwitchNumber);
	~MidiControlEngine();

	// These are the switch numbers used in "Menu" mode (default values 
	// that can be overridden in the config xml file <switches> section)
	enum EngineModeSwitch
	{
		kUnassignedSwitchNumber = -1,
		kModeRecall = 0,
		kModeBack,
		kModeForward,
		kModeTime,
		kModeProgramChangeDirect,
		kModeControlChangeDirect,
		kModeBankDirect,
		kModeExprPedalDisplay,
		kModeAdcOverride,
		kModeTestLeds,
		kModeToggleTraceWindow,
		kModeBankDesc,
		kModeClockSetup,
		kModeMidiOutSelect
	};

	// initialization
	using Patches = std::map<int, PatchPtr>;
	PatchBankPtr			AddBank(int number, const std::string & name, const std::string & notes);
	void					AddPatch(PatchPtr patch);
	void					SetPowerup(const std::string &powerupBank);
	void					AssignCustomBankLoad(int switchNumber, const std::string &bankName);
	void					AssignModeSwitchNumber(EngineModeSwitch mode, int switchNumber);
	const std::string		GetBankNameByNum(int bankNumberNotIndex);
	int						GetBankNumber(const std::string& name) const;
	void					AddToPatchGroup(const std::string &groupId, TwoStatePatch* patch);
	void					CompleteInit(const PedalCalibration * pedalCalibrationSettings, unsigned int ledColor, std::vector<std::string> &setorder);
	void					Shutdown();

	ExpressionPedals &		GetPedals() {return mGlobalPedals;} // only used during init/load
	PatchPtr				GetPatch(int number);
	int						GetPatchNumber(const std::string & name) const;
	ISwitchDisplay *		GetSwitchDisplay() const { return mSwitchDisplay; }

	void					SwitchPressed(int switchNumber);
	void					SwitchReleased(int switchNumber);
	void					VolatilePatchIsActive(PatchPtr patch);
	void					DeactivateVolatilePatches(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void					DeactivateRestOfPatchGroup(const std::string &groupId, TwoStatePatch * activePatch, IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay);
	void					ReleaseProgramChangePatchForChannel(int channel, ISwitchDisplay * switchDisplay, IMainDisplay * mainDisplay);
	void					CheckForDeviceProgramChange(PatchPtr patch, ISwitchDisplay * switchDisplay, IMainDisplay * mainDisplay);
	virtual void			AdcValueChanged(int port, int curValue) override;
	void					RefirePedal(int pedal);
	void					ResetBankPatches();
	void					ResetExclusiveGroup(int switchNumberToActivate);
	void					LoadBankByNumber(int bankNumber);
	void					LoadBankRelative(int relativeBankIndex);

	void					EnterNavMode() { ChangeMode(emBankNav); }
	void					HistoryBackward();
	void					HistoryForward();
	void					HistoryRecall();

	ControllerInputMonitorPtr GetControllerInputMonitor(int inputDevicePort);
	void AddControllerInputMonitor(int inputDevicePort, ControllerInputMonitorPtr mon);

	void					SetTempo(int val);
	int						GetTempo() const noexcept { return mTempo; }
	void					EnableMidiClock(bool enable);
	bool					IsMidiClockEnabled() const;

private:
	void					SetBankNavOrder(std::vector<std::string> &setorder);
	void					LoadStartupBank();
	bool					NavigateBankRelative(int relativeBankIndex);
	bool					LoadBank(int bankIndex);
	PatchBankPtr			GetBank(int bankIndex);
	int						GetBankIndex(int bankNumber);
	void					UpdateBankModeSwitchDisplay();
	void					CalibrateExprSettings(const PedalCalibration * pedalCalibrationSettings);
	void					EscapeToDefaultMode();
	int						GetSwitchNumber(EngineModeSwitch m) const;
	enum EngineMode 
	{ 
		emCreated = -1,		// initial state - no data loaded
		emBank,				// select presets in banks
		emModeSelect,		// out of default ready to select new mode
		emBankNav,			// navigate banks
		emBankDesc,			// describe switches in bank
		emBankDirect,		// use buttons to call bank
		emExprPedalDisplay,	// display actual adc values
		emAdcOverride,		// set ADC overrides
		emTimeDisplay,		// displays time
		emProgramChangeDirect, // manual send of program changes
		emControlChangeDirect, // manual send of control changes
		emLedTests,			// LED display tests
		emClockSetup,		// setup MIDI beat clock
		emMidiOutSelect,	// select midi out port to use for emProgramChangeDirect/emControlChangeDirect/emClockSetup
		emNotValid 
	};
	void					ChangeMode(EngineMode newMode);
	EngineMode				CurrentMode() const { return mMode.top(); }
	void					SetupModeSelectSwitch(EngineModeSwitch m);
	void					SwitchReleased_AdcOverrideMode(int switchNumber);
	void					SwitchReleased_PedalDisplayMode(int switchNumber);
	void					SwitchReleased_NavAndDescMode(int switchNumber);
	void					SwitchReleased_ModeSelect(int switchNumber);
	void					SwitchReleased_BankDirect(int switchNumber);
	void					SwitchPressed_ProgramChangeDirect(int switchNumber);
	void					SwitchReleased_ProgramChangeDirect(int switchNumber);
	void					SwitchPressed_ControlChangeDirect(int switchNumber);
	void					SwitchReleased_ControlChangeDirect(int switchNumber);
	void					SwitchPressed_ClockSetup(int switchNumber);
	void					SwitchReleased_ClockSetup(int switchNumber);
	void					SwitchReleased_TimeDisplay(int switchNumber);
	void					SwitchReleased_LedTests(int switchNumber);
	void					SwitchReleased_MidiOutSelect(int switchNumber);

private:
	// non-retained runtime state
	ITrollApplication *		mApplication = nullptr;
	IMainDisplay *			mMainDisplay = nullptr;
	ITraceDisplay *			mTrace = nullptr;
	ISwitchDisplay *		mSwitchDisplay = nullptr;
	IMidiOutGenerator *		mMidiOutGenerator = nullptr;
	IMidiOutPtr				mMidiOut; // only used for emProgramChangeDirect / emControlChangeDirect / emClockSetup
	std::vector<IAxeFxPtr>	mAxeMgrs;
	EdpManagerPtr			mEdpMgr;
	using ListenerMap = std::map<int, ControllerInputMonitorPtr>;
	ListenerMap				mInputMonitors;
	std::list<PatchPtr>		mActiveVolatilePatches;
	PatchPtr				mActiveProgramChangePatches[16];

	PatchBankPtr			mActiveBank;
	int						mActiveBankIndex;
	std::stack<EngineMode>	mMode;
	int						mBankNavigationIndex;
	std::string				mDirectNumber; // used by emBankDirect / emProgramChangeDirect / emControlChangeDirect / emClockSetup
	int						mDirectChangeChannel; // used by emProgramChangeDirect / emControlChangeDirect
	int						mDirectValueLastSent; // used by emProgramChangeDirect / emControlChangeDirect / emClockSetup
	int						mDirectValue1LastSent; // used by emControlChangeDirect
	std::stack<int>			mBackHistory;
	std::stack<int>			mForwardHistory;
	enum HistoryNavMode		{ hmNone, hmBack, hmForward, hmWentBack, hmWentForward};
	HistoryNavMode			mHistoryNavMode;
	unsigned int			mSwitchPressedEventTime;
	int						mTempo = 120;

	// retained in different form
	Patches					mPatches;		// patchNum is key
	using Banks = std::vector<PatchBankPtr>;
	Banks					mBanks;			// compressed; bankNum is not index; used during init and as backing store
	Banks					mBanksInNavOrder; // used at runtime -- could be identical to mBanks
	std::map<std::string, std::vector<TwoStatePatch*>> mPatchGroups;

	// retained state
	std::string				mPowerUpBankName;
	ExpressionPedals		mGlobalPedals;
	int						mPedalModePort;
	unsigned int			mEngineLedColor = kFirstColorPreset;

	// mode switch numbers
	int						mModeSwitchNumber;
	int						mIncrementSwitchNumber;
	int						mDecrementSwitchNumber;
	std::map<EngineModeSwitch, int>		mOtherModeSwitchNumbers;
	std::map<int, int>		mBankLoadSwitchNumbers;
	std::map<int, std::string>	mBankLoadSwitchBankNamesForInit;
};

using MidiControlEnginePtr = std::shared_ptr<MidiControlEngine>;

// reserved patch numbers
// As 2024-08-27, this isn't really necessary since user-defined patch numbers are no longer supported.
// patch number reservations
// -1 to -1000 are reserved for engine
// -1001 to -2000 are user-defined patch numbers of auto-generated patches defined via bank switch (not required)
// -2000 to -XXX are auto-generated patches (defined via bank switch)
// 
enum /*class*/ ReservedPatchNumbers
{
	// engine commands
	kBankNavNext							= -2,
	kBankNavPrev							= -3,
	kLoadNextBank							= -4,
	kLoadPrevBank							= -5,
	kBankHistoryBackward					= -6,
	kBankHistoryForward						= -7,
	kBankHistoryRecall						= -8,
	kResetBankPatches						= -9,
	kSyncAxeFx								= -10,
	kAxeFx3NextPreset						= -11,
	kAxeFx3PrevPreset						= -12,
	kAxeFx3NextScene						= -13,
	kAxeFx3PrevScene						= -14,
	kAxeFx3ReloadCurrentPreset				= -15,

	// auto-gen'd patches
	kAutoGenPatchNumberStart				= -2000
};

#endif // MidiControlEngine_h__
