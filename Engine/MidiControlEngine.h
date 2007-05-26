#pragma once

#include <map>
#include <vector>
#include "IInput.h"


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

	// IInput
	virtual void SwitchPressed(int switchNumber);
	virtual void SwitchReleased(int switchNumber);

private:
	void LoadBankRelative(int relativeBankIndex);

private:
	// runtime state
	IMidiOut *				mMidiOut;
	IMainDisplay *			mMainDisplay;
	ITraceDisplay *			mTrace;
	ISwitchDisplay *		mSwitchDisplay;

	std::map<int, Patch*>	mPatches;		// patchNum is key
	std::vector<PatchBank*> mBanks;			// compressed; bankNum is not index
	PatchBank *				mActiveBank;
	int						mActiveBankIndex;
	bool					mInMeta;

	// retained state
	int						mPowerUpTimeout;
	int						mPowerUpBank;
	int						mPowerUpPatch;
	int						mIncrementSwitchNumber;
	int						mDecrementSwitchNumber;
	int						mUtilSwitchNumber;
	bool					mFilterRedundantProgramChanges;
};
