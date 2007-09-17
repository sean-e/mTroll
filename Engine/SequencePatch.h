#ifndef SequencePatch_h__
#define SequencePatch_h__

#include "TwoStatePatch.h"


// SequencePatch
// -----------------------------------------------------------------------------
// responds to SwitchPressed; SwitchReleased does not affect patch state
// does not support expression pedals
//
class SequencePatch : public Patch
{
public:
	SequencePatch(int number, const std::string & name, int midiOutPortNumber, IMidiOut * midiOut) :
		Patch(number, name, midiOutPortNumber, midiOut),
		mCurIndex(0)
	{
	}
	
	virtual std::string GetPatchTypeStr() const { return "sequence"; }

	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
	{
		if (mCurIndex < mMidiByteStrings.size())
		{
			mPatchIsActive = true;
			const Bytes & curBytes = mMidiByteStrings[mCurIndex++];
			if (curBytes.size() && mMidiOut)
				mMidiOut->MidiOut(curBytes);

// should SequencePatches be able to use expr pedals?
// 			if (mMidiByteStrings.size() > 1)
// 				gActivePatchPedals = &mPedals;
		}

		if (mCurIndex >= mMidiByteStrings.size())
		{
			mPatchIsActive = false;
			mCurIndex = 0;
			if (gActivePatchPedals == &mPedals)
				gActivePatchPedals = NULL;
		}

		UpdateDisplays(mainDisplay, switchDisplay);
	}

	virtual void SwitchReleased(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay)
	{
		return;
	}

	virtual void BankTransitionActivation()
	{
		mCurIndex = 0;
		SwitchPressed(NULL, NULL);
	}

	virtual void BankTransitionDeactivation()
	{
		mPatchIsActive = false;
		mCurIndex = 0;
	}

			void AddString(const Bytes & midiString) { mMidiByteStrings.push_back(midiString); }

private:
	size_t					mCurIndex;
	std::vector<Bytes>		mMidiByteStrings;
};

#endif // SequencePatch_h__
