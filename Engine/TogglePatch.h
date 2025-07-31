/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2009,2017-2018,2024-2025 Sean Echevarria
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

#ifndef TogglePatch_h__
#define TogglePatch_h__

#ifdef _WINDOWS
	#include <windows.h>
	#define CurTime	GetTickCount // time in ms (used to measure elapsed time between events, origin doesn't matter)
#else
	#define CurTime	??
#endif // _WINDOWS
#include "TwoStatePatch.h"


// TogglePatch
// -----------------------------------------------------------------------------
// Toggle PatchLogicStyle:
// responds to SwitchPressed; SwitchReleased does not affect patch state
// supports expression pedals (psAllowOnlyActive) - but should it?
// 
// Momentary PatchLogicStyle:
// responds to SwitchPressed and SwitchReleased
// No expression pedal support
// 
// Hybrid PatchLogicStyle:
// Not enabled by default since it is not compatible with bank switch secondary functions
// In clean state, on button press assume momentary, check elapsed duration on release to potentially convert to toggle;
// if is toggle, then no longer clean once it becomes active; 
// momentary state is always clean; (?)
// disable of active toggle reverts to clean state.
// No expression pedal support
//
class TogglePatch : public TwoStatePatch
{
public:
	enum class PatchLogicStyle
	{
		Momentary = 0,
		Toggle,
		Hybrid
	};


	TogglePatch(int number, 
				const std::string & name, 
				IMidiOutPtr midiOut, 
				PatchCommands & cmdsA, 
				PatchCommands & cmdsB,
				PatchLogicStyle style = PatchLogicStyle::Toggle) :
		TwoStatePatch(number, name, midiOut, cmdsA, cmdsB, 
			PatchLogicStyle::Toggle == style ? psAllowOnlyActive : psDisallow),
		mPatchLogicStyle(style)
	{
	}

	TogglePatch(int number, 
				const std::string & name, 
				IMidiOutPtr midiOut, 
				PatchCommands * cmdsA = nullptr, 
				PatchCommands * cmdsB = nullptr,
				PatchLogicStyle style = PatchLogicStyle::Toggle) :
		TwoStatePatch(number, name, midiOut, cmdsA, cmdsB, 
			PatchLogicStyle::Toggle == style ? psAllowOnlyActive : psDisallow),
		mPatchLogicStyle(style)
	{
	}

	virtual std::string GetPatchTypeStr() const override 
	{ 
		switch (mPatchLogicStyle)
		{
		case PatchLogicStyle::Toggle:				return "toggle";
		case PatchLogicStyle::Momentary:			return "momentary";
		case PatchLogicStyle::Hybrid:
			switch (mHybridState)
			{
			case HybridState::Reset:				return "hybridInReset";
			case HybridState::ConvertedToToggle:	return "hybridToggle";
			case HybridState::PressedAndWaitingForRelease: return "hybridWaitingForRelease";
			}
			[[fallthrough]];
		default:
			_ASSERTE(!"not possible");
			return "impossible ToggleStyle/HybridState state";
		}
	}

	virtual void SwitchPressed(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) override
	{
		__super::SwitchPressed(mainDisplay, switchDisplay);
		_ASSERTE(PatchLogicStyle::Hybrid != mPatchLogicStyle || mHybridState == HybridState::Reset || mHybridState == HybridState::ConvertedToToggle);
		if (IsActive() || IsTogglePatchType())
		{
			// _ASSERTE(PatchLogicStyle::Momentary != mPatchLogicStyle); -- this will fire if a momentary patch has been auto-activated on bank load
			if (IsActive())
			{
				ExecCommandsB();
				if (PatchLogicStyle::Hybrid == mPatchLogicStyle)
				{
					_ASSERTE(mHybridState == HybridState::ConvertedToToggle);
					mHybridState = HybridState::Reset;
				}
			}
			else
				ExecCommandsA();
		}
		else
		{
			_ASSERTE(PatchLogicStyle::Momentary == mPatchLogicStyle || PatchLogicStyle::Hybrid == mPatchLogicStyle);
			ExecCommandsA();

			if (PatchLogicStyle::Hybrid == mPatchLogicStyle)
			{
				mSwitchPressedEventTime = ::CurTime();
				mHybridState = HybridState::PressedAndWaitingForRelease;
			}
		}

		UpdateDisplays(mainDisplay, switchDisplay);

		if (HybridState::PressedAndWaitingForRelease != mHybridState && mSwitchPressedEventTime)
			mSwitchPressedEventTime = 0;
	}

	virtual void SwitchReleased(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) override
	{
		bool handleMomentaryRelease = PatchLogicStyle::Momentary == mPatchLogicStyle;
		if (HybridState::PressedAndWaitingForRelease == mHybridState)
		{
			const unsigned int kDuration = ::CurTime() - mSwitchPressedEventTime;
			_ASSERTE(mSwitchPressedEventTime);
			mSwitchPressedEventTime = 0;

			// would have preferred 200ms, but simple mouse click of button in debug build requires more time
			constexpr int kMaximumToggleDuration = 250;
			if (kDuration < kMaximumToggleDuration)
			{
				mHybridState = HybridState::ConvertedToToggle;
				return;
			}

			// treat as momentary release
			mHybridState = HybridState::Reset;
			handleMomentaryRelease = true;
		}

		if (handleMomentaryRelease)
		{
			ExecCommandsB();
			UpdateDisplays(mainDisplay, switchDisplay);
		}
	}

	virtual void Deactivate(IMainDisplay * mainDisplay, ISwitchDisplay * switchDisplay) override
	{
		if (!IsActive())
			return;

		if (!IsTogglePatchType())
			SwitchReleased(mainDisplay, switchDisplay); // this could convert hybrid to toggle mode...

		if (IsTogglePatchType())
			__super::Deactivate(mainDisplay, switchDisplay); // for toggle, calls SwitchPressed if is active
	}

	virtual const std::string & GetDisplayText(bool checkState = false) const override
	{
		if (IsTogglePatchType())
		{
			if (!checkState || IsActive())
				return GetName();

			// don't display the patch name when it has just been toggled off
			static const std::string empty;
			return empty;
		}

		return __super::GetDisplayText(checkState);
	}

protected:
	enum class HybridState
	{
		Reset, // from Reset, only possible to go to PressedAndWaitingForRelease (in SwitchPressed)
		PressedAndWaitingForRelease, // can go to either ConvertedToToggle or Reset (in SwitchReleased)
		ConvertedToToggle // from ConvertedToToggle, only possible to go to Reset (in SwitchPressed)
	};

	HybridState mHybridState = HybridState::Reset;
	const PatchLogicStyle	mPatchLogicStyle;

	bool IsTogglePatchType() const 
	{
		return PatchLogicStyle::Toggle == mPatchLogicStyle ||
			(PatchLogicStyle::Hybrid == mPatchLogicStyle && HybridState::ConvertedToToggle == mHybridState);
	}

private:
	unsigned int mSwitchPressedEventTime = 0;
};

#endif // TogglePatch_h__
