/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2010 Sean Echevarria
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

#include <string>
#include <strstream>
#include "ExpressionPedals.h"
#include "IMainDisplay.h"
#include "IMidiOut.h"
#include "Patch.h"
#include "MidiControlEngine.h"
#include "ITraceDisplay.h"


bool
PedalToggle::Activate()
{
	if (mPatch->IsActive())
		return false;

	mPatch->SwitchPressed(NULL, mSwitchDisplay);
	return true;
}

bool
PedalToggle::Deactivate()
{
	if (!mPatch->IsActive())
		return false;

	mPatch->SwitchPressed(NULL, mSwitchDisplay);
	return true;
}

void
ExpressionControl::Init(bool invert, 
						byte channel, 
						byte controlNumber, 
						int minVal, 
						int maxVal,
						bool doubleByte,
						int bottomTogglePatchNumber,
						int topTogglePatchNumber)
{
	mEnabled = true;
	mInverted = invert;
	mChannel = channel;
	mControlNumber = controlNumber;
	mMinCcVal = minVal < maxVal ? minVal : maxVal;
	if (mMinCcVal < 0)
		mMinCcVal = 0;
	mMaxCcVal = maxVal > minVal ? maxVal : minVal;

	// http://www.midi.org/techspecs/midimessages.php
	if (doubleByte && controlNumber >= 0 && controlNumber < 32)
	{
		mIsDoubleByte = true;
		if (mMaxCcVal > 16383)
			mMaxCcVal = 16383;
	}
	else
		mIsDoubleByte = false;
	
	if (!mIsDoubleByte && mMaxCcVal > 127)
		mMaxCcVal = 127;

	mCcValRange = mMaxCcVal - mMinCcVal;

	mMidiData[0] = (0xb0 | mChannel);
	mMidiData[1] = mControlNumber;
	// init to invalid value so that jitter control does not filter initial 0s
	// these values need to be specifically checked for in Refire
	mMidiData[2] = 0xff;
	mMidiData[3] = 0xff;
	mMidiData[4] = 0;

	mBottomToggle.mTogglePatchNumber = bottomTogglePatchNumber;
	mTopToggle.mTogglePatchNumber = topTogglePatchNumber;
}

void
ExpressionControl::Calibrate(const PedalCalibration & calibrationSetting, 
							 MidiControlEngine * eng, 
							 ITraceDisplay * traceDisp)
{
	mMinAdcVal = calibrationSetting.mMinAdcVal;
	mMaxAdcVal = calibrationSetting.mMaxAdcVal;
	mAdcValRange = mMaxAdcVal - mMinAdcVal;

	if (mBottomToggle.mTogglePatchNumber != -1)
	{
		if (-1 == calibrationSetting.mBottomToggleZoneSize || -1 == calibrationSetting.mBottomToggleDeadzoneSize)
		{
			if (traceDisp)
			{
				std::strstream traceMsg;
				traceMsg << "Error setting up expression pedal bottom toggle - bottomToggleZoneSize and/or bottomToggleDeadzoneSize are not set" << std::endl << std::ends;
				traceDisp->Trace(std::string(traceMsg.str()));
			}
		}
		else
		{
			mBottomToggle.mMinDeactivateAdcVal = mMinAdcVal;
			mBottomToggle.mMaxDeactivateAdcVal = mBottomToggle.mMinDeactivateAdcVal + calibrationSetting.mBottomToggleZoneSize;
			mBottomToggle.mMinActivateAdcVal = mBottomToggle.mMaxDeactivateAdcVal + calibrationSetting.mBottomToggleDeadzoneSize;
			mBottomToggle.mMaxActivateAdcVal = mMaxAdcVal;
			mAdcValRange = mMaxAdcVal - mBottomToggle.mMinActivateAdcVal;

			if (eng)
			{
				mBottomToggle.mPatch = eng->GetPatch(mBottomToggle.mTogglePatchNumber);
				mBottomToggle.mSwitchDisplay = eng->GetSwitchDisplay();
				if (mBottomToggle.mPatch)
					mBottomToggle.mToggleIsEnabled = true;
			}
		}
	}

	if (mTopToggle.mTogglePatchNumber != -1)
	{
		if (-1 == calibrationSetting.mTopToggleZoneSize || -1 == calibrationSetting.mTopToggleDeadzoneSize)
		{
			if (traceDisp)
			{
				std::strstream traceMsg;
				traceMsg << "Error setting up expression pedal top toggle - topToggleZoneSize and/or topToggleDeadzoneSize are not set" << std::endl << std::ends;
				traceDisp->Trace(std::string(traceMsg.str()));
			}
		}
		else
		{
			mTopToggle.mMaxDeactivateAdcVal = mMaxAdcVal;
			mTopToggle.mMinDeactivateAdcVal = mTopToggle.mMaxDeactivateAdcVal - calibrationSetting.mTopToggleZoneSize;
			mTopToggle.mMaxActivateAdcVal = mTopToggle.mMinDeactivateAdcVal - calibrationSetting.mTopToggleDeadzoneSize;
			if (mBottomToggle.mToggleIsEnabled)
			{
				mTopToggle.mMinActivateAdcVal = mBottomToggle.mMinActivateAdcVal;
				mBottomToggle.mMaxActivateAdcVal = mTopToggle.mMaxActivateAdcVal;
				mAdcValRange = mTopToggle.mMaxActivateAdcVal - mBottomToggle.mMinActivateAdcVal;
			}
			else
			{
				mTopToggle.mMinActivateAdcVal = mMinAdcVal;
				mAdcValRange = mTopToggle.mMaxActivateAdcVal - mMinAdcVal;
			}

			if (eng)
			{
				mTopToggle.mPatch = eng->GetPatch(mTopToggle.mTogglePatchNumber);
				mTopToggle.mSwitchDisplay = eng->GetSwitchDisplay();
				if (mTopToggle.mPatch)
					mTopToggle.mToggleIsEnabled = true;
			}
		}
	}

	mActiveAdcRangeStart = mBottomToggle.mToggleIsEnabled ? mBottomToggle.mMinActivateAdcVal : mMinAdcVal;
	mActiveAdcRangeEnd = mTopToggle.mToggleIsEnabled ? mTopToggle.mMaxActivateAdcVal : mMaxAdcVal;

	if (0 >= mAdcValRange)
		mAdcValRange = 1;

	if (!mBottomToggle.mToggleIsEnabled && !mTopToggle.mToggleIsEnabled)
	{
		_ASSERTE(mMinAdcVal == mActiveAdcRangeStart);
	}
	_ASSERTE(mAdcValRange == mActiveAdcRangeEnd - mActiveAdcRangeStart);
}

void
ExpressionControl::AdcValueChange(IMainDisplay * mainDisplay, 
								  IMidiOut * midiOut, 
								  int newAdcVal)
{
	if (!mEnabled)
		return;

	int newCcVal;
	bool showStatus = false;
	bool doCcSend;
	bool bottomActivated = false;
	bool bottomDeactivated = false;
	bool topActivated = false;
	bool topDeactivated = false;
	bool deadZone = false;

	// unaffected by toggle zones
	const int cappedAdcVal = newAdcVal < mMinAdcVal ? 
			mMinAdcVal : 
			(newAdcVal > mMaxAdcVal) ? mMaxAdcVal : newAdcVal;

	if (mBottomToggle.mToggleIsEnabled || mTopToggle.mToggleIsEnabled)
	{
		if (cappedAdcVal >= mActiveAdcRangeStart && cappedAdcVal <= mActiveAdcRangeEnd)
		{
			// normal 127 range is 1023
			newCcVal = ((cappedAdcVal - mActiveAdcRangeStart) * mCcValRange) / mAdcValRange;
			doCcSend = true;
		}
		else
		{
			doCcSend = false;
			newCcVal = -1;
		}

		if (mBottomToggle.mToggleIsEnabled)
		{
			if (mBottomToggle.IsInActivationZone(cappedAdcVal))
			{
				_ASSERTE(doCcSend);
				if (mBottomToggle.Activate())
					showStatus = bottomActivated = true;
			}
			else if (mBottomToggle.IsInDeactivationZone(cappedAdcVal))
			{
				_ASSERTE(!doCcSend);
				if (mBottomToggle.Deactivate())
					showStatus = bottomDeactivated = true;
			}
			else if (mBottomToggle.IsInDeadzone(cappedAdcVal))
			{
				_ASSERTE(!doCcSend);
				showStatus = deadZone = true;
			}
			else
			{
				_ASSERTE(mTopToggle.mToggleIsEnabled && (mTopToggle.IsInDeactivationZone(cappedAdcVal) || mTopToggle.IsInDeadzone(cappedAdcVal)));
			}
		}

		if (!deadZone && !bottomDeactivated && mTopToggle.mToggleIsEnabled)
		{
			if (mTopToggle.IsInActivationZone(cappedAdcVal))
			{
				_ASSERTE(doCcSend);
				if (mTopToggle.Activate())
					showStatus = topActivated = true;
			}
			else if (mTopToggle.IsInDeactivationZone(cappedAdcVal))
			{
				_ASSERTE(!doCcSend);
				if (mTopToggle.Deactivate())
					showStatus = topDeactivated = true;
			}
			else if (mTopToggle.IsInDeadzone(cappedAdcVal))
			{
				_ASSERTE(!doCcSend);
				showStatus = deadZone = true;
			}
			else
			{
				_ASSERTE(mBottomToggle.mToggleIsEnabled && (mBottomToggle.IsInDeactivationZone(cappedAdcVal) || mBottomToggle.IsInDeadzone(cappedAdcVal)));
			}
		}
	}
	else
	{
		// no toggle
		// normal 127 range is 1023
		newCcVal = ((cappedAdcVal - mMinAdcVal) * mCcValRange) / mAdcValRange;
		doCcSend = true;
	}

	if (doCcSend)
	{
		if (mMinCcVal)
			newCcVal += mMinCcVal;

		if (newCcVal > mMaxCcVal)
			newCcVal = mMaxCcVal;
		else if (newCcVal < mMinCcVal)
			newCcVal = mMinCcVal;

		// only fire midi indicator at top and bottom of range -
		// easier to see that top and bottom hit on controller than on pc
		if (!showStatus)
			showStatus = newCcVal == mMinCcVal || newCcVal == mMaxCcVal;

		if (mIsDoubleByte)
		{
			if (mInverted)
				newCcVal = 16383 - newCcVal;

			byte newFineCcVal = newCcVal & 0x7F; // LSB
			byte newCoarseCcVal = (newCcVal >> 7) & 0x7f; // MSB

			// jitter control
			if (mMidiData[2] == newCoarseCcVal && (mMidiData[3] == newFineCcVal || mMidiData[4] == newFineCcVal) &&
				!topActivated && !bottomActivated && !topDeactivated && !bottomDeactivated)
			{
				if (0 == newFineCcVal || 127 == newFineCcVal)
				{
					if (abs(mMidiData[3] - mMidiData[4]) < 3)
						return;
				}
				else
					return;
			}
#if 0
			byte coarseCh = mMidiData[1];
			byte fineCh = coarseCh + 32;

			byte oldCoarseCcVal = mMidiData[2];
			byte oldFineCcVal = mMidiData[3];
			
			static const int kFineIncVal = 64;

			while (oldCoarseCcVal != newCoarseCcVal)
			{
				if (oldCoarseCcVal < newCoarseCcVal)
				{
					for (oldFineCcVal += kFineIncVal; oldFineCcVal < 127; oldFineCcVal += kFineIncVal)
						midiOut->MidiOut(mMidiData[0], fineCh, oldFineCcVal, false);

					oldFineCcVal = 0;
					midiOut->MidiOut(mMidiData[0], coarseCh, ++oldCoarseCcVal, false);
				}
				else
				{
					for (oldFineCcVal -= kFineIncVal; oldFineCcVal > 0 && oldFineCcVal < 127; oldFineCcVal -= kFineIncVal)
						midiOut->MidiOut(mMidiData[0], fineCh, oldFineCcVal, false);

					oldFineCcVal = 127;
					midiOut->MidiOut(mMidiData[0], coarseCh, --oldCoarseCcVal, false);
				}
			}

			if (oldFineCcVal < newFineCcVal)
			{
				for (oldFineCcVal += kFineIncVal; oldFineCcVal < newFineCcVal; oldFineCcVal += kFineIncVal)
					midiOut->MidiOut(mMidiData[0], fineCh, oldFineCcVal, false);
			}
			else if (oldFineCcVal > newFineCcVal)
			{
				for (oldFineCcVal -= kFineIncVal; oldFineCcVal > newFineCcVal && oldFineCcVal < 127; oldFineCcVal -= kFineIncVal)
					midiOut->MidiOut(mMidiData[0], fineCh, oldFineCcVal, false);
			}

			midiOut->MidiOut(mMidiData[0], fineCh, newFineCcVal, showStatus);
#else
			midiOut->MidiOut(mMidiData[0], mMidiData[1], newCoarseCcVal, showStatus);
			midiOut->MidiOut(mMidiData[0], mMidiData[1] + 32, newFineCcVal, showStatus);
#endif

			mMidiData[4] = mMidiData[3];
			mMidiData[3] = newFineCcVal;
			mMidiData[2] = newCoarseCcVal;
		}
		else
		{
			if (mInverted)
				newCcVal = 127 - newCcVal;

			// jitter control
			if ((mMidiData[2] == newCcVal || mMidiData[3] == newCcVal) &&
				!topActivated && !bottomActivated && !topDeactivated && !bottomDeactivated)
			{
				if (0 == newCcVal || 127 == newCcVal)
				{
					if (abs(mMidiData[2] - mMidiData[3]) < 3)
						return;
				}
				else
					return;
			}

			mMidiData[3] = mMidiData[2];
			mMidiData[2] = newCcVal;
			midiOut->MidiOut(mMidiData[0], mMidiData[1], mMidiData[2], showStatus);
		}
	}

	if (mainDisplay)
	{
		static bool sHadStatus = false;
//		showStatus = true; // for testing
		if (showStatus || sHadStatus)
		{
			std::strstream displayMsg;
			if (bottomDeactivated)
				displayMsg << "bottom toggle deactivated" << std::endl;
			else if (bottomActivated)
				displayMsg << "bottom toggle activated" << std::endl;

			if (topDeactivated)
				displayMsg << "top toggle deactivated" << std::endl;
			else if (topActivated)
				displayMsg << "top toggle activated" << std::endl;

			if (doCcSend)
			{
				if (newCcVal == mMinCcVal)
					displayMsg << "______ min ______" << std::endl;
				else if (newCcVal == mMaxCcVal)
					displayMsg << "|||||| MAX ||||||" << std::endl;
			}
			else if (deadZone)
				displayMsg << "pedal deadzone" << std::endl;

			sHadStatus = showStatus;
			if (showStatus && doCcSend)
			{
				if (mIsDoubleByte)
				{
					displayMsg << "adc ch(" << (int) mChannel << "), ctrl(" << (int) mControlNumber << "): " << newAdcVal << " -> " << (int) mMidiData[2] << std::endl;
					displayMsg << "adc ch(" << (int) mChannel << "), ctrl(" << ((int) mControlNumber) + 31 << "): " << newAdcVal << " -> " << (int) mMidiData[3] << std::endl << std::ends;
				}
				else
					displayMsg << "adc ch(" << (int) mChannel << "), ctrl(" << (int) mControlNumber << "): " << newAdcVal << " -> " << (int) mMidiData[2] << std::endl << std::ends;
			}
			else
				displayMsg << std::endl << std::ends; // clear display

			mainDisplay->TextOut(displayMsg.str());
		}
	}
}

void
ExpressionControl::Refire(IMainDisplay * mainDisplay,
						  IMidiOut * midiOut)
{
	if (!mEnabled)
		return;

	// the refire command may not work as intended if a toggle is present
	if (mIsDoubleByte)
	{
		if (0xff != mMidiData[2] && 0xff != mMidiData[3])
		{
			midiOut->MidiOut(mMidiData[0], mMidiData[1], mMidiData[2], true);
			midiOut->MidiOut(mMidiData[0], mMidiData[1] + 32, mMidiData[3], true);
		}
	}
	else
	{
		if (mMidiData[2] != 0xff)
			midiOut->MidiOut(mMidiData[0], mMidiData[1], mMidiData[2], true);
	}
}







#if 0 // pedal logic testing

#include <windows.h>
#undef TextOut		// stupid unicode support defines TextOut to TextOutW

class TestDisplay : public IMainDisplay
{
public:
	TestDisplay() { }
	virtual void TextOut(const std::string & txt) { OutputDebugStringA(txt.c_str()); }
	virtual void ClearDisplay() { }
};

class TestMidiOut : public IMidiOut
{
public:
	TestMidiOut() { }
	virtual unsigned int GetMidiOutDeviceCount() const {return 0;}
	virtual std::string GetMidiOutDeviceName(unsigned int deviceIdx) const {return std::string();}
	virtual void SetActivityIndicator(ISwitchDisplay * activityIndicator, int activityIndicatorIdx) {}
	virtual void EnableActivityIndicator(bool enable) {}
	virtual bool OpenMidiOut(unsigned int deviceIdx) {return false;}
	virtual bool IsMidiOutOpen() const {return false;}
	virtual bool MidiOut(const Bytes & bytes) {return false;}
	virtual void MidiOut(byte singleByte, bool useIndicator = true) {}
	virtual void MidiOut(byte byte1, byte byte2, bool useIndicator = true) {}
	virtual void MidiOut(byte byte1, byte byte2, byte byte3, bool useIndicator = true) {}
	virtual void CloseMidiOut() {}
};

void TestPedals()
{
	TestDisplay disp;
	TestMidiOut midi;

	PedalCalibration calib;
	calib.Init(100, 1000);
//	calib.Init(0, PedalCalibration::MaxAdcVal);

	ExpressionControl ctl;
	ctl.Init(false, 1, 1, 0, 127, false);

	PatchCommands onCmds, offCmds;
	ctl.InitToggle(0, 25, 50, onCmds, offCmds);
	ctl.InitToggle(1, 25, 50, onCmds, offCmds);

	ctl.Calibrate(calib);

	int idx;
	for (idx = 0; idx <= PedalCalibration::MaxAdcVal; ++idx)
		ctl.AdcValueChange(&disp, &midi, idx);

	for (idx = PedalCalibration::MaxAdcVal + 1; idx > 0; )
		ctl.AdcValueChange(&disp, &midi, --idx);
}

#endif
