/*
 * mTroll MIDI Controller
 * Copyright (C) 2023 Sean Echevarria
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

#ifndef RepeatingPatch_h__
#define RepeatingPatch_h__

#include "TwoStatePatch.h"
#include <atomic>
#include <qthread.h>


 // RepeatingPatch
 // -----------------------------------------------------------------------------
 // might respond to SwitchPressed and SwitchReleased
 // No expression pedal support
 //
class RepeatingPatch : public TwoStatePatch, private QThread
{
public:
	RepeatingPatch(int number,
		const std::string & name,
		IMidiOutPtr midiOut,
		PatchCommands & cmdsA,
		PatchCommands & cmdsB) :
		TwoStatePatch(number, name, midiOut, cmdsA, cmdsB, psDisallow)
	{
	}

	~RepeatingPatch()
	{
		if (IsRunning())
			StopThread();
	}

	bool IsRunning() const noexcept { return mThreadIsRunning; }

	bool StartThread()
	{
		mThreadShouldRun = true;
		start(QThread::HighPriority);

		while (!IsRunning())
			yieldCurrentThread();

		return true;
	}

	bool StopThread()
	{
		mThreadShouldRun = false;

		while (IsRunning())
			yieldCurrentThread();

		return true;
	}

	virtual void run() override
	{
		class ManageRunState
		{
		public:
			ManageRunState(std::atomic_bool& ref) : mRef(ref)
			{
				mRef = true;
			}

			~ManageRunState()
			{
				mRef = false;
			}

		private:
			std::atomic_bool& mRef;
		};

		ManageRunState rs(mThreadIsRunning);
		while (mThreadShouldRun)
			ExecCommandsA();
	}

private:
	std::atomic_bool mThreadIsRunning = false;
	std::atomic_bool mThreadShouldRun = false;
};

#endif // RepeatingPatch_h__
