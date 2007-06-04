#ifndef MidiControlEngine_h__
#define MidiControlEngine_h__

#include <map>
#include <vector>
#include "IInput.h"
#include "Patch.h"


class IMainDisplay;
class ISwitchDisplay;
class ITraceDisplay;
class IMidiOut;
class Patch;
class PatchBank;


class MidiControlEngine : public IInput
{
public:
	MidiControlEngine(IMidiOut * midiOut, 
					  IMainDisplay * mainDisplay, 
					  ISwitchDisplay * switchDisplay,
					  ITraceDisplay * traceDisplay,
					  int incrementSwitchNumber,
					  int decrementSwitchNumber,
					  int modeSwitchNumber);
	~MidiControlEngine();

	// initialization
	typedef std::map<int, Patch*> Patches;
	PatchBank &				AddBank(int number, const std::string & name);
	void					AddPatch(int number, const std::string & name, Patch::PatchType patchType, const Bytes & stringA, const Bytes & stringB);
	void					SetPowerup(int powerupBank, int powerupPatch, int powerupTimeout);
	void					FilterRedundantProgChg(bool filter) {mFilterRedundantProgramChanges = filter;}
	void					CompleteInit();

	// IInput
	virtual void			SwitchPressed(int switchNumber);
	virtual void			SwitchReleased(int switchNumber);

private:
	bool					NavigateBankRelative(int relativeBankIndex);
	bool					LoadBank(int bankIndex);
	PatchBank *				GetBank(int bankIndex);
	void					NextMode();

private:
	// non-retained runtime state
	IMidiOut *				mMidiOut;
	IMainDisplay *			mMainDisplay;
	ITraceDisplay *			mTrace;
	ISwitchDisplay *		mSwitchDisplay;

	PatchBank *				mActiveBank;
	int						mActiveBankIndex;
	enum EngineMode 
	{ 
		emCreated = -1,		// initial state - no data loaded
		emDefault,			// select presets in banks
		emBankNav,			// navigate banks
		emBankDesc,			// describe switches in bank
		emNotValid 
	};
	EngineMode				mMode;
	int						mBankNavigationIndex;

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
};

#endif // MidiControlEngine_h__
