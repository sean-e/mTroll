#ifndef ExpressionPedals_h__
#define ExpressionPedals_h__

typedef unsigned char byte;
class IMidiOut;
class IMainDisplay;


struct PedalCalibration
{
	PedalCalibration() : 
		mMinAdcVal(0),
		mMaxAdcVal(10000),
		mAdcRange(10000)
	{ }

	void Init(int minVal, int maxVal)
	{
		mMinAdcVal = minVal;
		mMaxAdcVal = maxVal;
		mAdcRange = maxVal - minVal;
	}

	int		mMinAdcVal;
	int		mMaxAdcVal;
	int		mAdcRange;
};


class ExpressionControl
{
public:
	ExpressionControl() : mEnabled(false) { }

	void Init(bool enable, 
			  bool invert, 
			  byte channel, 
			  byte controlNumber, 
			  byte minVal, 
			  byte maxVal)
	{
		mEnabled = enable;
		mInverted = invert;
		mChannel = channel;
		mControlNumber = controlNumber;
		mMinCcVal = minVal;
		mMaxCcVal = maxVal;
		mCurCcVal = mPrevCcVal = 255;
	}

	void Calibrate(PedalCalibration & calibrationSetting)
	{
		if (!mEnabled)
			return;

		// do stuff
	}

	void AdcValueChange(IMainDisplay * mainDisplay, IMidiOut * midiOut, int newVal)
	{
		if (!mEnabled)
			return;

		mPrevCcVal = mCurCcVal;

		byte newCcVal;
		// do stuff
		newCcVal = 0;
		mCurCcVal = newCcVal;
	}

private:
	bool				mEnabled;
	bool				mInverted;
	byte				mChannel;
	byte				mControlNumber;
	byte				mMinCcVal;
	byte				mMaxCcVal;
	byte				mCurCcVal;
	byte				mPrevCcVal;
};


class ExpressionPedal
{
	enum {ccsPerPedals = 2};

public:
	ExpressionPedal() { }

	void Init(int idx, 
			  bool enable, 
			  bool invert, 
			  byte channel, 
			  byte controlNumber, 
			  byte minVal, 
			  byte maxVal)
	{
		_ASSERTE(idx < ccsPerPedals);
		mPedalControlData[idx].Init(enable, invert, channel, controlNumber, minVal, maxVal);
	}

	void Calibrate(PedalCalibration & calibrationSetting)
	{
		mPedalControlData[0].Calibrate(calibrationSetting);
		mPedalControlData[1].Calibrate(calibrationSetting);
	}

	void AdcValueChange(IMainDisplay * mainDisplay, IMidiOut * midiOut, int newVal)
	{
		mPedalControlData[0].AdcValueChange(mainDisplay, midiOut, newVal);
		mPedalControlData[1].AdcValueChange(mainDisplay, midiOut, newVal);
	}

private:
	ExpressionControl	mPedalControlData[ccsPerPedals];
};


class ExpressionPedals
{
public:
	enum {PedalCount = 4};

	ExpressionPedals() 
	{
		int idx;
		for (idx = 0; idx < PedalCount; ++idx)
			mGlobalEnables[idx] = true;
		for (idx = 0; idx < PedalCount; ++idx)
			mPedalEnables[idx] = false;
	}

	void EnableGlobal(int pedal, bool enable)
	{
		_ASSERTE(pedal < PedalCount);
		mGlobalEnables[pedal] = enable;
	}

	void Init(int pedal, 
			  int idx, 
			  bool enable, 
			  bool invert, 
			  byte channel, 
			  byte controlNumber, 
			  byte minVal, 
			  byte maxVal)
	{
		_ASSERTE(pedal < PedalCount);
		mPedals[pedal].Init(idx, enable, invert, channel, controlNumber, minVal, maxVal);

		if (enable)
			mPedalEnables[pedal] = true;
	}

	void Calibrate(PedalCalibration * calibrationSetting)
	{
		for (int idx = 0; idx < PedalCount; ++idx)
			mPedals[idx].Calibrate(calibrationSetting[idx]);
	}

	bool AdcValueChange(IMainDisplay * mainDisplay, 
						IMidiOut * midiOut, 
						int pedal, 
						int newVal)
	{
		_ASSERTE(pedal < PedalCount);
		if (mPedalEnables[pedal])
			mPedals[pedal].AdcValueChange(mainDisplay, midiOut, newVal);
		return mGlobalEnables[pedal];
	}

private:
	bool					mGlobalEnables[PedalCount];
	bool					mPedalEnables[PedalCount];	// true if either cc is enabled for a pedal
	ExpressionPedal			mPedals[PedalCount];
};

#endif // ExpressionPedals_h__
