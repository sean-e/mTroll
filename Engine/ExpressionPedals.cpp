/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2011,2014-2015,2018,2020,2023,2025 Sean Echevarria
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
#include <sstream>
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
	mPatchIsActivated = true;
	if (mPatch->IsActive())
		return false;

	mPatch->OverridePedals(true);
	mPatch->SwitchPressed(nullptr, mSwitchDisplay);
	mPatch->OverridePedals(false);
	return true;
}

bool
PedalToggle::Deactivate()
{
	mPatchIsActivated = false;
	if (!mPatch->IsActive())
		return false;

	mPatch->OverridePedals(true);
	mPatch->SwitchPressed(nullptr, mSwitchDisplay);
	mPatch->OverridePedals(false);
	return true;
}

void
ExpressionControl::Init(int pedal, const InitParams & params)
{
	mEnabled = true;
	mPedalNumber = pedal;
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
	mOverrideBottomToggleDeadzoneSize = params.mOverrideBottomToggleDeadzoneSize;
	mOverrideTopToggleDeadzoneSize = params.mOverrideTopToggleDeadzoneSize;
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
		if (-1 == calibrationSetting.mBottomToggleZoneSize || (-1 == calibrationSetting.mBottomToggleDeadzoneSize && -1 == mOverrideBottomToggleDeadzoneSize))
		{
			if (traceDisp)
			{
				std::ostringstream traceMsg;
				traceMsg << "Error setting up expression pedal bottom toggle - bottomToggleZoneSize and/or bottomToggleDeadzoneSize are not set\n";
				traceDisp->Trace(traceMsg.str());
			}
		}
		else
		{
			mBottomToggle.mMinDeactivateAdcVal = mMinAdcVal;
			mBottomToggle.mMaxDeactivateAdcVal = mBottomToggle.mMinDeactivateAdcVal + calibrationSetting.mBottomToggleZoneSize;
			mBottomToggle.mMinActivateAdcVal = mBottomToggle.mMaxDeactivateAdcVal + (-1 != mOverrideBottomToggleDeadzoneSize ? mOverrideBottomToggleDeadzoneSize : calibrationSetting.mBottomToggleDeadzoneSize);
			mBottomToggle.mMaxActivateAdcVal = mMaxAdcVal;
			mAdcValRange = mMaxAdcVal - mBottomToggle.mMinActivateAdcVal;

			const int deadzoneLen = mBottomToggle.mMinActivateAdcVal - mBottomToggle.mMaxDeactivateAdcVal;
			if (deadzoneLen > 1)
				mBottomToggle.mTransitionToActivationAdcVal = (deadzoneLen / 2) + mBottomToggle.mMaxDeactivateAdcVal;

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
		if (-1 == calibrationSetting.mTopToggleZoneSize || (-1 == calibrationSetting.mTopToggleDeadzoneSize && -1 == mOverrideTopToggleDeadzoneSize))
		{
			if (traceDisp)
			{
				std::ostringstream traceMsg;
				traceMsg << "Error setting up expression pedal top toggle - topToggleZoneSize and/or topToggleDeadzoneSize are not set\n";
				traceDisp->Trace(traceMsg.str());
			}
		}
		else
		{
			mTopToggle.mMaxDeactivateAdcVal = mMaxAdcVal;
			mTopToggle.mMinDeactivateAdcVal = mTopToggle.mMaxDeactivateAdcVal - calibrationSetting.mTopToggleZoneSize;
			mTopToggle.mMaxActivateAdcVal = mTopToggle.mMinDeactivateAdcVal - (-1 != mOverrideTopToggleDeadzoneSize ? mOverrideTopToggleDeadzoneSize : calibrationSetting.mTopToggleDeadzoneSize);

			const int deadzoneLen = mTopToggle.mMinDeactivateAdcVal - mTopToggle.mMaxActivateAdcVal;
			if (deadzoneLen > 1)
				mTopToggle.mTransitionToActivationAdcVal = mTopToggle.mMinDeactivateAdcVal - (deadzoneLen / 2);

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

bool gEnableStatusDetails = false;

void
ExpressionControl::AdcValueChange(IMainDisplay * mainDisplay, 
								  int newVal)
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
	const int cappedAdcVal = newVal < mMinAdcVal ? 
			mMinAdcVal : 
			(newVal > mMaxAdcVal) ? mMaxAdcVal : newVal;

	int adcVal = cappedAdcVal - mActiveAdcRangeStart; // this might result in a value that is not valid for CC send, handled after curves
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
				if (Zones::activeZone != mCurrentZone && mBottomToggle.Activate())
					showStatus = bottomActivated = true;
				else if (!doCcSend)
					showStatus = true;

				if (Zones::activeZone != mCurrentZone)
				{
					mPreviousZone = mCurrentZone;
					mCurrentZone = Zones::activeZone;
				}
			}
			else if (mBottomToggle.IsInDeactivationZone(cappedAdcVal))
			{
				_ASSERTE(!doCcSend);
				if (Zones::deactivateZone != mCurrentZone && mBottomToggle.Deactivate())
				{
					showStatus = bottomDeactivated = true;
					newCcVal = 0;
				}
				else
				{
					if (!doCcSend)
						showStatus = true;
					// since deactivation was not necessary, clear previous text
					if (mainDisplay)
						mainDisplay->ClearTransientText();
				}

				if (Zones::deactivateZone != mCurrentZone)
				{
					mPreviousZone = mCurrentZone;
					mCurrentZone = Zones::deactivateZone;
				}
			}
			else if (mBottomToggle.IsInDeadzone(cappedAdcVal))
			{
				_ASSERTE(!doCcSend);
				showStatus = bottomDeadzone = true;
				newCcVal = 0;

				if (Zones::deadZoneButCloseToActive == mCurrentZone)
					;
				else if (Zones::deadZone == mCurrentZone)
				{
					if (Zones::activeZone != mPreviousZone && 
						mBottomToggle.IsInDeadZoneButCloseToActive(cappedAdcVal))
					{
						mPreviousZone = mCurrentZone;
						mCurrentZone = Zones::deadZoneButCloseToActive;
						if (mBottomToggle.Activate())
						{
							bottomActivated = true;
							doCcSend = true;
						}
					}
				}
				else
				{
					if (Zones::deadZone != mCurrentZone)
					{
						mPreviousZone = mCurrentZone;
						mCurrentZone = Zones::deadZone;
					}
				}
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
				if (Zones::activeZone != mCurrentZone && mTopToggle.Activate())
					showStatus = topActivated = true;
				else if (!doCcSend)
					showStatus = true;

				if (Zones::activeZone != mCurrentZone)
				{
					mPreviousZone = mCurrentZone;
					mCurrentZone = Zones::activeZone;
				}
			}
			else if (mTopToggle.IsInDeactivationZone(cappedAdcVal))
			{
				_ASSERTE(!doCcSend);
				if (Zones::deactivateZone != mCurrentZone && mTopToggle.Deactivate())
				{
					showStatus = topDeactivated = true;
					newCcVal = mMaxCcVal;
				}
				else
				{
					if (!doCcSend)
						showStatus = true;
					// since deactivation was not necessary, clear previous text
					if (mainDisplay)
						mainDisplay->ClearTransientText();
				}

				if (Zones::deactivateZone != mCurrentZone)
				{
					mPreviousZone = mCurrentZone;
					mCurrentZone = Zones::deactivateZone;
				}
			}
			else if (mTopToggle.IsInDeadzone(cappedAdcVal))
			{
				_ASSERTE(!doCcSend);
				showStatus = topDeadzone = true;
				newCcVal = mMaxCcVal;

				if (Zones::deadZoneButCloseToActive == mCurrentZone)
					;
				else if (Zones::deadZone == mCurrentZone)
				{
					if (Zones::activeZone != mPreviousZone && 
						mTopToggle.IsInDeadZoneButCloseToActive(cappedAdcVal))
					{
						mPreviousZone = mCurrentZone;
						mCurrentZone = Zones::deadZoneButCloseToActive;
						if (mTopToggle.Activate())
						{
							topActivated = true;
							doCcSend = true;
						}
					}
				}
				else
				{
					if (Zones::deadZone != mCurrentZone)
					{
						mPreviousZone = mCurrentZone;
						mCurrentZone = Zones::deadZone;
					}
				}
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
						mMidiOut->MidiOut(mMidiData[0], fineCh, oldFineCcVal, false);

					oldFineCcVal = 0;
					mMidiOut->MidiOut(mMidiData[0], coarseCh, ++oldCoarseCcVal, false);
				}
				else
				{
					for (oldFineCcVal -= kFineIncVal; oldFineCcVal > 0 && oldFineCcVal < 127; oldFineCcVal -= kFineIncVal)
						mMidiOut->MidiOut(mMidiData[0], fineCh, oldFineCcVal, false);

					oldFineCcVal = 127;
					mMidiOut->MidiOut(mMidiData[0], coarseCh, --oldCoarseCcVal, false);
				}
			}

			if (oldFineCcVal < newFineCcVal)
			{
				for (oldFineCcVal += kFineIncVal; oldFineCcVal < newFineCcVal; oldFineCcVal += kFineIncVal)
					mMidiOut->MidiOut(mMidiData[0], fineCh, oldFineCcVal, false);
			}
			else if (oldFineCcVal > newFineCcVal)
			{
				for (oldFineCcVal -= kFineIncVal; oldFineCcVal > newFineCcVal && oldFineCcVal < 127; oldFineCcVal -= kFineIncVal)
					mMidiOut->MidiOut(mMidiData[0], fineCh, oldFineCcVal, false);
			}

			mMidiOut->MidiOut(mMidiData[0], fineCh, newFineCcVal, showStatus);
#else
			if (mMidiOut)
			{
				mMidiOut->MidiOut(mMidiData[0], mMidiData[1], newCoarseCcVal, showStatus);
				mMidiOut->MidiOut(mMidiData[0], mMidiData[1] + 32, newFineCcVal, showStatus);
			}
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
			if (mMidiOut)
				mMidiOut->MidiOut(mMidiData[0], mMidiData[1], mMidiData[2], showStatus);
		}
	}

	if (mainDisplay)
	{
		static bool sHadStatus = false;
#ifdef PEDAL_TESTxx
		showStatus = true;
#endif
		if (showStatus || sHadStatus || doCcSend)
		{
			std::ostringstream displayMsg;
			if (gEnableStatusDetails)
			{
				if (bottomDeactivated)
					displayMsg << "bottom toggle deactivated\n";
				else if (bottomActivated)
					displayMsg << "bottom toggle activated\n";

				if (topDeactivated)
					displayMsg << "top toggle deactivated\n";
				else if (topActivated)
					displayMsg << "top toggle activated\n";

				if (mBottomToggle.mToggleIsEnabled && mBottomToggle.mPatch)
				{
					if (mBottomToggle.mPatchIsActivated)
						displayMsg << "pedal bottom patch active\n";
					else
						displayMsg << "pedal bottom patch inactive\n";
				}

				if (mTopToggle.mToggleIsEnabled && mTopToggle.mPatch)
				{
					if (mTopToggle.mPatchIsActivated)
						displayMsg << "pedal top patch active\n";
					else
						displayMsg << "pedal top patch inactive\n";
				}

				if (bottomDeadzone)
					displayMsg << "pedal bottom deadzone\n";
				else if (topDeadzone)
					displayMsg << "pedal top deadzone\n";
			}
			else
			{
				if (doCcSend)
				{
					const int ccRange = mMaxCcVal - mMinCcVal;
					constexpr int kMaxShortRange = 31;
					displayMsg << "Expr " << mPedalNumber << ": ";
					if (newCcVal != 0 && newCcVal == mMinCcVal)
						displayMsg << (int)mMidiData[2] << " (min)";
					else if (newCcVal != 127 && newCcVal == mMaxCcVal)
					{
						if (ccRange < kMaxShortRange)
							displayMsg << (int)mMidiData[2] << " (max)";
						else
							displayMsg << "|||||||||||||||||||X  (" << (int)mMidiData[2] << " max)";
					}
					else if (newCcVal >= 0)
					{
						if (mMinCcVal == 0 && mMaxCcVal == 127)
						{
							if (newCcVal == 0)
								displayMsg << "0";
							else if (newCcVal == 1)
								displayMsg << "1";
							else if (newCcVal == 2)
								displayMsg << "2";
							else if (newCcVal == 3)
								displayMsg << "3";
							else if (newCcVal == 4)
								displayMsg << "4";
							else if (newCcVal == 5)
								displayMsg << "5";
							else if (newCcVal <= 9)
								displayMsg << "|";
							else if (newCcVal <= 16)
								displayMsg << "||";
							else if (newCcVal <= 24)
								displayMsg << "|||";
							else if (newCcVal < 32)
								displayMsg << "||||";
							else if (newCcVal == 32)
								displayMsg << "||||:";
							else if (newCcVal <= 40)
								displayMsg << "||||:|";
							else if (newCcVal <= 48)
								displayMsg << "||||:||";
							else if (newCcVal <= 56)
								displayMsg << "||||:|||";
							else if (newCcVal < 64)
								displayMsg << "||||:||||";
							else if (newCcVal == 64)
								displayMsg << "||||:||||:";
							else if (newCcVal <= 72)
								displayMsg << "||||:||||:|";
							else if (newCcVal <= 80)
								displayMsg << "||||:||||:||";
							else if (newCcVal <= 88)
								displayMsg << "||||:||||:|||";
							else if (newCcVal < 96)
								displayMsg << "||||:||||:||||";
							else if (newCcVal == 96)
								displayMsg << "||||:||||:||||:";
							else if (newCcVal <= 104)
								displayMsg << "||||:||||:||||:|";
							else if (newCcVal <= 112)
								displayMsg << "||||:||||:||||:||";
							else if (newCcVal <= 120)
								displayMsg << "||||:||||:||||:|||";
							else if (newCcVal < 127)
								displayMsg << "||||:||||:||||:||||";
							else if (newCcVal == 127)
								displayMsg << "|||||||||||||||||||X";
							else
								displayMsg << "XXXXXXXXXXXXXXXXXXXX";
						}
						else if (ccRange < kMaxShortRange)
						{
							// short range, just show actual value
							displayMsg << (int)newCcVal;
						}
						else
						{
							// truncated version -- want to maintain :::: scaling for actual cc value rather than range approximation
							if (newCcVal < mMinCcVal + 6)
								displayMsg << (int)newCcVal;
							else if (newCcVal < 32)
								displayMsg << "||                    (" << (int)mMidiData[2] << ")";
							else if (newCcVal == 32)
								displayMsg << "||||:";
							else if (newCcVal < 64)
								displayMsg << "||||:||               (" << (int)mMidiData[2] << ")";
							else if (newCcVal == 64)
								displayMsg << "||||:||||:";
							else if (newCcVal < 96 && mMaxCcVal > 96)
								displayMsg << "||||:||||:||          (" << (int)mMidiData[2] << ")";
							else if (newCcVal == 96 && mMaxCcVal > 96)
								displayMsg << "||||:||||:||||:";
							else if (newCcVal < mMaxCcVal)
								displayMsg << "||||:||||:||||:||     (" << (int)mMidiData[2] << ")";
							else if (newCcVal == mMaxCcVal)
								displayMsg << "|||||||||||||||||||X";
							else
								displayMsg << "XXXXXXXXXXXXXXXXXXXX";
						}
					}
				}
				else if (bottomDeadzone)
				{
					std::string tmp;
					tmp.append(newVal > 50 ? 50 : newVal, '-');
					displayMsg << "Expr " << mPedalNumber << ": " << tmp.c_str();
				}
				else if (topDeadzone)
					displayMsg << "Expr " << mPedalNumber << ": ++++++";
			}

			sHadStatus = showStatus;
			if (doCcSend || (gEnableStatusDetails && showStatus))
			{
				if (gEnableStatusDetails)
				{
					std::ostringstream finalMsg;
					if (mIsDoubleByte)
					{
						finalMsg << "[ch " << (int)mChannel << ", ctrl " << (int)mControlNumber << "] " << newVal << " -> " << (int)mMidiData[2];
						finalMsg << "[ch " << (int)mChannel << ", ctrl " << ((int)mControlNumber) + 31 << "] " << newVal << " -> " << (int)mMidiData[3];
					}
					else
					{
#ifdef PEDAL_TESTxx
						// to ease insert into spreadsheet
						finalMsg << newVal << ", " << (int)mMidiData[2];
#else
						finalMsg << "[ch " << (int)mChannel << ", ctrl " << (int)mControlNumber << "] " << newVal << " -> " << (int)mMidiData[2];
#endif
					}

					finalMsg << '\n' << displayMsg.str();
					mainDisplay->TransientTextOut(finalMsg.str());
				}
				else
				{
					displayMsg << '\n';
					mainDisplay->TransientTextOut(displayMsg.str());
				}
			}
			else if (bottomDeadzone || topDeadzone)
			{
				displayMsg << '\n';
				mainDisplay->TransientTextOut(displayMsg.str());
			}
			else if (gEnableStatusDetails || displayMsg.str().empty())
			{
				mainDisplay->ClearTransientText();
			}
			else
			{ 
				displayMsg << '\n';
				mainDisplay->TransientTextOut(displayMsg.str());
			}
		}
	}
}

void
ExpressionControl::Refire(IMainDisplay * mainDisplay)
{
	if (!mEnabled)
		return;

	// the refire command may not work as intended if a toggle is present
	if (mIsDoubleByte)
	{
		if (0xff != mMidiData[2] && 0xff != mMidiData[3])
		{
			if (mMidiOut)
			{
				mMidiOut->MidiOut(mMidiData[0], mMidiData[1], mMidiData[2], true);
				mMidiOut->MidiOut(mMidiData[0], mMidiData[1] + 32, mMidiData[3], true);
			}
		}
	}
	else
	{
		if (mMidiData[2] != 0xff && mMidiOut)
			mMidiOut->MidiOut(mMidiData[0], mMidiData[1], mMidiData[2], true);
	}
}







#ifdef PEDAL_TEST // pedal logic testing

#include <windows.h>
#undef TextOut		// stupid unicode support defines TextOut to TextOutW

class TestDisplay : public IMainDisplay, public ITraceDisplay
{
public:
	TestDisplay() { }
	void TextOut(const std::string & txt) override { OutputDebugStringA(txt.c_str()); }
	void ClearDisplay() override { }
	void Trace(const std::string & txt) override { TextOut(txt); }
	void AppendText(const std::string & text) override { TextOut(text); }
	void TransientTextOut(const std::string & txt) override { TextOut(txt); }
	void ClearTransientText() override { }
	std::string GetCurrentText() override { return std::string{}; }
};

class TestMidiOut : public IMidiOut
{
public:
	TestMidiOut() { }
	unsigned int GetMidiOutDeviceCount() const override {return 0;}
	std::string GetMidiOutDeviceName(unsigned int deviceIdx) const override {return std::string();}
	void SetActivityIndicator(ISwitchDisplay * activityIndicator, int activityIndicatorIdx, unsigned int ledColor) override {}
	void EnableActivityIndicator(bool enable) override {}
	bool OpenMidiOut(unsigned int deviceIdx) override {return false;}
	bool IsMidiOutOpen() const override {return false;}
	bool MidiOut(const Bytes & bytes) override {return false;}
	void MidiOut(byte singleByte, bool useIndicator = true) override {}
	void MidiOut(byte byte1, byte byte2, bool useIndicator = true) override {}
	void MidiOut(byte byte1, byte byte2, byte byte3, bool useIndicator = true) override {}
	void CloseMidiOut() override {}
	bool SuspendMidiOut() override { return false; }
	bool ResumeMidiOut() override { return false; }
};

void TestPedals()
{
	TestDisplay disp;
	TestMidiOut midi;

	PedalCalibration calib;
	calib.mMaxAdcVal = 900;
	calib.mMinAdcVal = 200;
	calib.mBottomToggleZoneSize = 50;
	calib.mTopToggleZoneSize = 50;
	calib.mBottomToggleDeadzoneSize = 50;
	calib.mTopToggleDeadzoneSize = 50;
	ExpressionControl ctl;
	ExpressionControl::InitParams params;
	params.mCurve = ExpressionControl::scReversePseudoAudioLog;
// 	params.mMinVal = 20;
// 	params.mMaxVal = 100;
	ctl.Init(1, params);
	ctl.Calibrate(calib, NULL, &disp);

	int idx;
	for (idx = 0; idx <= PedalCalibration::MaxAdcVal; ++idx)
		ctl.AdcValueChange(&disp, idx);
	for (idx = PedalCalibration::MaxAdcVal + 1; idx > 0; )
		ctl.AdcValueChange(&disp, --idx);
}

#endif
