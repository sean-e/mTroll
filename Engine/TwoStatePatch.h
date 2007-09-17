#ifndef TwoStatePatch_h__
#define TwoStatePatch_h__

#include "Patch.h"


// TwoStatePatch
// -----------------------------------------------------------------------------
// Is it two states or two strings?  both.
// Base class for classic pmc10 patch types.
//
class TwoStatePatch : public Patch
{
protected:
	enum PedalSupport {psDisallow, psAllow, psAllowOnlyActive};

	TwoStatePatch(int number, 
				  const std::string & name, 
				  int midiOutPortNumber, 
				  IMidiOut * midiOut, 
				  const Bytes & midiStringA, 
				  const Bytes & midiStringB, 
				  PedalSupport pedalSupport) :
		Patch(number, name, midiOutPortNumber, midiOut),
		mMidiByteStringA(midiStringA),
		mMidiByteStringB(midiStringB),
		mPedalSupport(pedalSupport)
	{
	}

	virtual void BankTransitionActivation() { SendStringA(); }
	virtual void BankTransitionDeactivation() { SendStringB(); }

	void SendStringA()
	{
		if (mMidiByteStringA.size())
			mMidiOut->MidiOut(mMidiByteStringA);
		mPatchIsActive = true;

		if (psDisallow != mPedalSupport)
		{
			if (mPedals.HasAnySettings())
			{
				// do this here rather than SwitchPressed to that pedals can be
				// set on bank load rather than only during patch load
				gActivePatchPedals = &mPedals;
			}
			else
				gActivePatchPedals = NULL;
		}
	}

	void SendStringB()
	{
		if (mMidiByteStringB.size())
			mMidiOut->MidiOut(mMidiByteStringB);
		mPatchIsActive = false;

		if (psAllowOnlyActive == mPedalSupport && gActivePatchPedals == &mPedals)
			gActivePatchPedals = NULL;
	}

private:
	TwoStatePatch();
	TwoStatePatch(const TwoStatePatch &);

protected:
	const Bytes			mMidiByteStringA;
	const Bytes			mMidiByteStringB;
	const PedalSupport	mPedalSupport;
};

#endif // TwoStatePatch_h__
