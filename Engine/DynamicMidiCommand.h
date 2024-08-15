/*
 * mTroll MIDI Controller
 * Copyright (C) 2024 Sean Echevarria
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

#ifndef DynamicMidiCommand_h__
#define DynamicMidiCommand_h__

#include <random>
#include "IPatchCommand.h"
#include "IMidiOut.h"
#include "EngineLoader.h"


class IMidiOutGenerator;


class DynamicMidiCommand : public IPatchCommand
{
	static constexpr int kMidiChannels = 16;
	static constexpr int kMaxDynamicPorts = 16;

public:
	DynamicMidiCommand(IMidiOutPtr midiOut,
					  Bytes & midiString,
					  bool dynamicChannel = false,
					  bool dynamicVelocity = false) :
		mMidiOut(midiOut),
		mDynamicChannel(dynamicChannel),
		mDynamicVelocity(dynamicVelocity)
	{
		mCommandStringTemplate.swap(midiString);
	}

	virtual void Exec() override;

	static void SetDynamicPortData(IMidiOutGenerator *midiOutGen, const MidiPortToDeviceIdxMap &portMap);
	static void SetDynamicOutPort(int port);
	static void SetDynamicChannel(int ch);
	static void SetDynamicChannelVelocity(int vel);
	static void SetDynamicChannelRandomVelocity(int lowerVel, int upperVel);

private:
	DynamicMidiCommand();

private:
	IMidiOutPtr	mMidiOut = nullptr;
	Bytes	mCommandStringTemplate;
	bool	mDynamicChannel = false;
	bool	mDynamicVelocity = false;


	// state used by SetDynamicOutPort, required to get IMidiOut for a given port
	static IMidiOutGenerator *sMidiOutGenerator;
	static MidiPortToDeviceIdxMap sMidiOutPortToDeviceIdxMap;

	static int sDynamicOutPort; // used as index into the sDynamicMidiOut array
	static IMidiOutPtr sDynamicMidiOut[kMaxDynamicPorts];

	static int sDynamicChannel; // used as index into the next 3 arrays
	// velocity can be set independently per channel (even though channel in a particular 
	// command instance might not be dynamic)
	static int sRandomNoteVelocityLower[kMidiChannels];
	static int sRandomNoteVelocityUpper[kMidiChannels];
	static std::uniform_int_distribution<int> sRandomVelocityDistribution[kMidiChannels];

	static std::default_random_engine sGenerator;
};


class SetDynamicPortCommand : public IPatchCommand
{
public:
	SetDynamicPortCommand(int port) : mPort(port) { }

	virtual void Exec() override
	{
		DynamicMidiCommand::SetDynamicOutPort(mPort);
	}

private:
	const int mPort;

	SetDynamicPortCommand() = delete;
};


class SetDynamicChannelCommand : public IPatchCommand
{
public:
	SetDynamicChannelCommand(int channel) : mChannel(channel) { }

	virtual void Exec() override
	{
		DynamicMidiCommand::SetDynamicChannel(mChannel);
	}

private:
	const int mChannel;

	SetDynamicChannelCommand() = delete;
};


class SetDynamicChannelVelocityCommand : public IPatchCommand
{
public:
	SetDynamicChannelVelocityCommand(int velocity) : mVelocity(velocity) { }

	virtual void Exec() override
	{
		DynamicMidiCommand::SetDynamicChannelVelocity(mVelocity);
	}

private:
	const int mVelocity;

	SetDynamicChannelVelocityCommand() = delete;
};


class SetDynamicChannelRandomVelocityCommand : public IPatchCommand
{
public:
	SetDynamicChannelRandomVelocityCommand(int minVelocity, int maxVelocity) :
		mMinVelocity(minVelocity), mMaxVelocity(maxVelocity) { }

	virtual void Exec() override
	{
		DynamicMidiCommand::SetDynamicChannelRandomVelocity(mMinVelocity, mMaxVelocity);
	}

private:
	const int mMinVelocity;
	const int mMaxVelocity;

	SetDynamicChannelRandomVelocityCommand() = delete;
};

#endif // DynamicMidiCommand_h__
