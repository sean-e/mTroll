#ifndef MidiControlEngine_h__
#define MidiControlEngine_h__

#include <map>
#include <vector>
#include "Patch.h"
#include "ExpressionPedals.h"


class IMainDisplay;
class ISwitchDisplay;
class ITraceDisplay;
class IMidiOut;
class Patch;
class PatchBank;


class MidiControlEngine
{
public:
	MidiControlEngine(IMainDisplay * mainDisplay, 
					  ISwitchDisplay * switchDisplay,
					  ITraceDisplay * traceDisplay,
					  int incrementSwitchNumber,
					  int decrementSwitchNumber,
					  int modeSwitchNumber);
	~MidiControlEngine();

	// initialization
	typedef std::map<int, Patch*> Patches;
	PatchBank &				AddBank(int number, const std::string & name);
	Patch &					AddPatch(int number, const std::string & name, Patch::PatchType patchType, int midiOutPortNumber, IMidiOut * midiOut, const Bytes & midiStringA, const Bytes & midiStringB);
	bool					AddMetaPatch(int number, const std::string & name, const std::string & metaCommand);
	void					SetPowerup(int powerupBank, int powerupPatch, int powerupTimeout);
	void					FilterRedundantProgChg(bool filter) {mFilterRedundantProgramChanges = filter;}
	void					CompleteInit(const PedalCalibration * pedalCalibrationSettings);

	ExpressionPedals &		GetPedals() {return mPedals;}

	void					SwitchPressed(int switchNumber);
	void					SwitchReleased(int switchNumber);
	void					AdcValueChanged(int port, int curValue);
	void					ResetBankPatches();

private:
	void					LoadStartupBank();
	bool					NavigateBankRelative(int relativeBankIndex);
	bool					LoadBank(int bankIndex);
	PatchBank *				GetBank(int bankIndex);
	int						GetBankIndex(int bankNumber);
	void					UpdateBankModeSwitchDisplay();
	void					CalibrateExprSettings(const PedalCalibration * pedalCalibrationSettings);
	enum EngineMode 
	{ 
		emCreated = -1,		// initial state - no data loaded
		emBank,				// select presets in banks
		emModeSelect,		// out of default ready to select new mode
		emBankNav,			// navigate banks
		emBankDesc,			// describe switches in bank
		emBankDirect,		// use buttons to call bank
		emNotValid 
	};
	void					ChangeMode(EngineMode newMode);

private:
	// non-retained runtime state
	IMainDisplay *			mMainDisplay;
	ITraceDisplay *			mTrace;
	ISwitchDisplay *		mSwitchDisplay;

	PatchBank *				mActiveBank;
	int						mActiveBankIndex;
	EngineMode				mMode;
	int						mBankNavigationIndex;
	std::string				mBankDirectNumber;

	// retained in different form
	Patches					mPatches;		// patchNum is key
	typedef std::vector<PatchBank*> Banks;
	Banks					mBanks;			// compressed; bankNum is not index

	// retained state
	int						mPowerUpTimeout;
	int						mPowerUpBank;
	int						mPowerUpPatch;
	int						mIncrementSwitchNumber;
	int						mDecrementSwitchNumber;
	int						mModeSwitchNumber;
	bool					mFilterRedundantProgramChanges;
	int						mModeDefaultSwitchNumber;
	int						mModeBankNavSwitchNumber;
	int						mModeBankDescSwitchNumber;
	ExpressionPedals		mPedals;
};

#endif // MidiControlEngine_h__
