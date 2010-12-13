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
#include <complex>
#include "ExpressionPedals.h"
#include "IMainDisplay.h"
#include "IMidiOut.h"
#include "Patch.h"
#include "MidiControlEngine.h"
#include "ITraceDisplay.h"


//#define PEDAL_TEST


bool
PedalToggle::Activate()
{
	if (mPatch->IsActive())
		return false;

	mPatch->OverridePedals(true);
	mPatch->SwitchPressed(NULL, mSwitchDisplay);
	mPatch->OverridePedals(false);
	return true;
}

bool
PedalToggle::Deactivate()
{
	if (!mPatch->IsActive())
		return false;

	mPatch->OverridePedals(true);
	mPatch->SwitchPressed(NULL, mSwitchDisplay);
	mPatch->OverridePedals(false);
	return true;
}

void
ExpressionControl::Init(const InitParams & params)
{
	mEnabled = true;
	mInverted = params.mInvert;
	mChannel = params.mChannel;
	mControlNumber = params.mControlNumber;
	mMinCcVal = params.mMinVal < params.mMaxVal ? params.mMinVal : params.mMaxVal;
	if (mMinCcVal < 0)
		mMinCcVal = 0;
	mMaxCcVal = params.mMaxVal > params.mMinVal ? params.mMaxVal : params.mMinVal;
	mSweepCurve = params.mCurve;

	// http://www.midi.org/techspecs/midimessages.php
	if (params.mDoubleByte && params.mControlNumber >= 0 && params.mControlNumber < 32)
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

	mBottomToggle.mTogglePatchNumber = params.mBottomTogglePatchNumber;
	mTopToggle.mTogglePatchNumber = params.mTopTogglePatchNumber;
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
	bool doCcSend = true;
	bool bottomActivated = false;
	bool bottomDeactivated = false;
	bool topActivated = false;
	bool topDeactivated = false;
	bool bottomDeadzone = false;
	bool topDeadzone = false;

	// unaffected by toggle zones
	const int cappedAdcVal = newAdcVal < mMinAdcVal ? 
			mMinAdcVal : 
			(newAdcVal > mMaxAdcVal) ? mMaxAdcVal : newAdcVal;

	int adcVal = cappedAdcVal - mActiveAdcRangeStart;
	switch (mSweepCurve)
	{
	case scLinear:
		// normal 127 range is 1023
		newCcVal = (adcVal * mCcValRange) / mAdcValRange;
		break;
	case scAudioLog:
	case scShallowLog:
		// http://www.maxim-ic.com/app-notes/index.mvp/id/3996
		// http://en.wikipedia.org/wiki/Logarithm
		// http://beavisaudio.com/techpages/Pots/
		if (adcVal < 1)
			newCcVal = 0;
		else if (adcVal >= mAdcValRange)
			newCcVal = mCcValRange;
		else
		{
			double tmp_b = (mAdcValRange - adcVal) / (double)mAdcValRange;
			double dbVal = log10(tmp_b); // was * 20
			newCcVal = (int)((dbVal * mCcValRange) / (scAudioLog == mSweepCurve ? -3 : -2.25)); // was / -60
		}
		break;
	case scReversePseudoAudioLog:
		{
		// made this up myself while trying to do reverseAudioLog
		double tmp_b = (mAdcValRange - adcVal) / (double)mAdcValRange;
		double dbVal = pow((mCcValRange + 1), tmp_b);
		newCcVal = mCcValRange - (int)((dbVal * mCcValRange) / (mCcValRange + 1));
		}
		break;
	case scReverseAudioLog:
	case scReverseShallowLog:
		{
		// take audioLog and then reverse and mirror using linear sweep as mirror line
		// is there an alternate way to do this?
		int oppositeAdcVal = mAdcValRange - adcVal;

		// opposite is audioLog
		if (oppositeAdcVal < 1)
			newCcVal = mCcValRange;
		if (oppositeAdcVal >= mAdcValRange)
			newCcVal = 0;
		else
		{
			const int oppositeLinearCcVal = (oppositeAdcVal * mCcValRange) / mAdcValRange;
			double tmp_b = (mAdcValRange - oppositeAdcVal) / (double)mAdcValRange;
			double dbVal = log10(tmp_b); // was * 20
			const int oppositeNewCcVal = (int)((dbVal * mCcValRange) / (scReverseAudioLog == mSweepCurve ? -3 : -2.25)); // was / -60

			newCcVal = (adcVal * mCcValRange) / mAdcValRange; // linear calc
			newCcVal += (oppositeLinearCcVal - oppositeNewCcVal); // add opposite offset
		}
		}
		break;
	case scPseudoAudioLog:
		{
		// take pseudoReverseAudioLog and then reverse and mirror using linear as mirror line
		// is there an alternate way to do this?
		int oppositeAdcVal = mAdcValRange - adcVal;
		const int oppositeLinearCcVal = (oppositeAdcVal * mCcValRange) / mAdcValRange;
			
		double tmp_b = (mAdcValRange - oppositeAdcVal) / (double)mAdcValRange;
		double dbVal = pow((mCcValRange + 1), tmp_b);
		const int oppositeNewCcVal = mCcValRange - (int)((dbVal * mCcValRange) / (mCcValRange + 1));

		newCcVal = (adcVal * mCcValRange) / mAdcValRange; // linear calc
		newCcVal -= (oppositeNewCcVal - oppositeLinearCcVal); // subtract opposite offset
		}
		break;
	default:
		_ASSERTE(!"unhandled sweep");
		return;
	}

	if (mBottomToggle.mToggleIsEnabled || mTopToggle.mToggleIsEnabled)
	{
		if (!(cappedAdcVal >= mActiveAdcRangeStart && cappedAdcVal <= mActiveAdcRangeEnd))
			doCcSend = false;

		if (mBottomToggle.mToggleIsEnabled)
		{
			if (mBottomToggle.IsInActivationZone(cappedAdcVal))
			{
				_ASSERTE(doCcSend);
				if (mBottomToggle.Activate())
					showStatus = bottomActivated = true;
				else if (!doCcSend)
					showStatus = true;
			}
			else if (mBottomToggle.IsInDeactivationZone(cappedAdcVal))
			{
				_ASSERTE(!doCcSend);
				if (mBottomToggle.Deactivate())
				{
					showStatus = bottomDeactivated = true;
					newCcVal = mMinCcVal;
				}
				else if (!doCcSend)
					showStatus = true;
			}
			else if (mBottomToggle.IsInDeadzone(cappedAdcVal))
			{
				_ASSERTE(!doCcSend);
				showStatus = bottomDeadzone = true;
				newCcVal = mMinCcVal;
			}
			else
			{
				_ASSERTE(mTopToggle.mToggleIsEnabled && (mTopToggle.IsInDeactivationZone(cappedAdcVal) || mTopToggle.IsInDeadzone(cappedAdcVal)));
			}
		}

		if (!bottomDeadzone && !bottomDeactivated && mTopToggle.mToggleIsEnabled)
		{
			if (mTopToggle.IsInActivationZone(cappedAdcVal))
			{
				_ASSERTE(doCcSend);
				if (mTopToggle.Activate())
					showStatus = topActivated = true;
				else if (!doCcSend)
					showStatus = true;
			}
			else if (mTopToggle.IsInDeactivationZone(cappedAdcVal))
			{
				_ASSERTE(!doCcSend);
				if (mTopToggle.Deactivate())
				{
					showStatus = topDeactivated = true;
					newCcVal = mMaxCcVal;
				}
				else if (!doCcSend)
					showStatus = true;
			}
			else if (mTopToggle.IsInDeadzone(cappedAdcVal))
			{
				_ASSERTE(!doCcSend);
				showStatus = topDeadzone = true;
				newCcVal = mMaxCcVal;
			}
			else
			{
				_ASSERTE(mBottomToggle.mToggleIsEnabled && (mBottomToggle.IsInDeactivationZone(cappedAdcVal) || mBottomToggle.IsInDeadzone(cappedAdcVal)));
			}
		}
	}

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

		if (bottomDeadzone || bottomDeactivated || topDeadzone || topDeactivated)
		{
			if (mMidiData[2] != newCoarseCcVal || mMidiData[3] != newFineCcVal)
				doCcSend = true; // make sure min/maxCcVal is sent before deadzone entry
		}
		else if (mMidiData[2] == newCoarseCcVal && (mMidiData[3] == newFineCcVal || mMidiData[4] == newFineCcVal) &&
			!topActivated && !bottomActivated)
		{
			// jitter control
			if (0 == newFineCcVal || 127 == newFineCcVal)
			{
				if (abs(mMidiData[3] - mMidiData[4]) < 3)
					return;
			}
			else
				return;
		}

		if (doCcSend)
		{
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
	}
	else
	{
		if (mInverted)
			newCcVal = 127 - newCcVal;

		if (bottomDeadzone || bottomDeactivated || topDeadzone || topDeactivated)
		{
			if (mMidiData[2] != newCcVal)
				doCcSend = true; // make sure min/maxCcVal is sent before deadzone entry
		}
		else if ((mMidiData[2] == newCcVal || mMidiData[3] == newCcVal) &&
			!topActivated && !bottomActivated)
		{
			// jitter control
			if (mMinCcVal == newCcVal || mMaxCcVal == newCcVal)
			{
				if (abs(mMidiData[2] - mMidiData[3]) < 3)
					return;
			}
			else
				return;
		}

		if (doCcSend)
		{
			mMidiData[3] = mMidiData[2];
			mMidiData[2] = newCcVal;
			midiOut->MidiOut(mMidiData[0], mMidiData[1], mMidiData[2], showStatus);
		}
	}

	if (mainDisplay)
	{
		static bool sHadStatus = false;
#ifdef PEDAL_TEST
		showStatus = true;
#endif
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
			if (bottomDeadzone || topDeadzone)
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
				{
#ifdef PEDAL_TEST
					// to ease insert into spreadsheet
					displayMsg << newAdcVal << ", " << (int) mMidiData[2] << std::endl << std::ends;
#else
					displayMsg << "adc ch(" << (int) mChannel << "), ctrl(" << (int) mControlNumber << "): " << newAdcVal << " -> " << (int) mMidiData[2] << std::endl << std::ends;
#endif
				}
	
				mainDisplay->TransientTextOut(displayMsg.str());
			}
			else if (bottomDeadzone || topDeadzone)
				mainDisplay->TransientTextOut(displayMsg.str());
			else
				mainDisplay->ClearTransientText();
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







#ifdef PEDAL_TEST // pedal logic testing

#include <windows.h>
#undef TextOut		// stupid unicode support defines TextOut to TextOutW

class TestDisplay : public IMainDisplay, public ITraceDisplay
{
public:
	TestDisplay() { }
	virtual void TextOut(const std::string & txt) { OutputDebugStringA(txt.c_str()); }
	virtual void ClearDisplay() { }
	virtual void Trace(const std::string & txt) { TextOut(txt); }
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
// 	calib.mMaxAdcVal = 900;
// 	calib.mMinAdcVal = 200;
	ExpressionControl ctl;
	ExpressionControl::InitParams params;
	params.mCurve = ExpressionControl::scReversePseudoAudioLog;
// 	params.mMinVal = 20;
// 	params.mMaxVal = 100;
	ctl.Init(params);
	ctl.Calibrate(calib, NULL, &disp);

	int idx;
	for (idx = 0; idx <= PedalCalibration::MaxAdcVal; ++idx)
		ctl.AdcValueChange(&disp, &midi, idx);

// 	for (idx = PedalCalibration::MaxAdcVal + 1; idx > 0; )
// 		ctl.AdcValueChange(&disp, &midi, --idx);
}

#endif
