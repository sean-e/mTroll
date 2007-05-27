#pragma once

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
					  ITraceDisplay * traceDisplay);
	~MidiControlEngine();

	// initialization
	typedef std::map<int, Patch*> Patches;
	PatchBank & AddBank(int number, const std::string & name);
	void AddPatch(int number, const std::string & name, Patch::PatchType patchType, const Bytes & stringA, const Bytes & stringB);
	void InitComplete();

	// IInput
	virtual void SwitchPressed(int switchNumber);
	virtual void SwitchReleased(int switchNumber);

private:
	void LoadBankRelative(int relativeBankIndex);

private:
	// non-retained runtime state
	IMidiOut *				mMidiOut;
	IMainDisplay *			mMainDisplay;
	ITraceDisplay *			mTrace;
	ISwitchDisplay *		mSwitchDisplay;

	PatchBank *				mActiveBank;
	int						mActiveBankIndex;
	bool					mInMeta;

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
	int						mUtilSwitchNumber;
	bool					mFilterRedundantProgramChanges;
};
