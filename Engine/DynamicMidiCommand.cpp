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

#include "DynamicMidiCommand.h"
#include "IMidiOutGenerator.h"


class DynamicMidiData
{
public:
	DynamicMidiData(IMidiOutGenerator *midiOutGen, const MidiPortToDeviceIdxMap &portMap) :
		mMidiOutGenerator(midiOutGen),
		mMidiOutPortToDeviceIdxMap(portMap)
	{
		if (!mMidiOutPortToDeviceIdxMap.empty())
		{
			// set default out port to first in the map
			SetDynamicOutPort((*mMidiOutPortToDeviceIdxMap.begin()).first);
		}
	}

	void SetDynamicOutPort(int port)
	{
		if (!mMidiOutGenerator)
		{
			_ASSERTE(mMidiOutGenerator);
			return;
		}

		if (port < 0 || port >= kMaxDynamicPorts)
		{
			_ASSERTE(!"unhandled dynamic port number");
			return;
		}

		mDynamicOutPort = port;

		if (mMidiOutPortToDeviceIdxMap.find(mDynamicOutPort) != mMidiOutPortToDeviceIdxMap.end())
			mDynamicMidiOut[mDynamicOutPort] = mMidiOutGenerator->GetMidiOut(mMidiOutPortToDeviceIdxMap[mDynamicOutPort]);
	}

	IMidiOutPtr GetDynamicMidiOut() const
	{
		return mDynamicMidiOut[mDynamicOutPort];
	}

	int GetDynamicOutPort() const { return mDynamicOutPort; }

	void SetDynamicChannel(int ch)
	{
		if (ch >= 0 && ch < kMidiChannels)
			mDynamicChannel = ch;
	}

	int GetDynamicChannel() const { return mDynamicChannel; }

	void SetDynamicChannelVelocity(int vel)
	{
		mRandomNoteVelocityLower[mDynamicChannel] = mRandomNoteVelocityUpper[mDynamicChannel] = vel;
	}

	void SetDynamicChannelRandomVelocity(int lowerVel, int upperVel)
	{
		if (lowerVel == upperVel)
		{
			SetDynamicChannelVelocity(lowerVel);
			return;
		}

		mRandomNoteVelocityLower[mDynamicChannel] = lowerVel;
		mRandomNoteVelocityUpper[mDynamicChannel] = upperVel;
		mRandomVelocityDistribution[mDynamicChannel] = std::uniform_int_distribution<int>(mRandomNoteVelocityLower[mDynamicChannel], mRandomNoteVelocityUpper[mDynamicChannel]);
	}

	int GetDynamicVelocity()
	{
		if (mRandomNoteVelocityLower[mDynamicChannel] == mRandomNoteVelocityUpper[mDynamicChannel])
			return (byte)mRandomNoteVelocityLower[mDynamicChannel];
		return (byte)mRandomVelocityDistribution[mDynamicChannel](mGenerator);
	}

private:
	DynamicMidiData() = delete;

	// state used by SetDynamicOutPort, required to get IMidiOut for a given port
	IMidiOutGenerator *mMidiOutGenerator = nullptr;
	MidiPortToDeviceIdxMap mMidiOutPortToDeviceIdxMap{};


	static constexpr int kMidiChannels = 16;
	static constexpr int kMaxDynamicPorts = 16;
	static constexpr int kDefaultVelocity = 127;

	int mDynamicOutPort = 0; // used as index into the sDynamicMidiOut array
	IMidiOutPtr mDynamicMidiOut[kMaxDynamicPorts];

	int mDynamicChannel = 0; // used as index into the next 3 arrays
	// velocity can be set independently per channel (even though channel in a particular 
	// command instance might not be dynamic)
	// upper and lower are same if not random; start out dynamic but not random
	int mRandomNoteVelocityLower[kMidiChannels]{
		kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, kDefaultVelocity,
		kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, kDefaultVelocity,
		kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, kDefaultVelocity,
		kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, kDefaultVelocity
	};
	int mRandomNoteVelocityUpper[kMidiChannels]{
		kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, kDefaultVelocity,
		kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, kDefaultVelocity,
		kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, kDefaultVelocity,
		kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, kDefaultVelocity
	};
	std::uniform_int_distribution<int> mRandomVelocityDistribution[kMidiChannels];

	std::default_random_engine mGenerator;
};

DynamicMidiData* gDynamicMidiData = nullptr;


void
DynamicMidiCommand::InitDynamicData(IMidiOutGenerator *midiOutGen, 
								    const MidiPortToDeviceIdxMap &portMap)
{
	_ASSERTE(!gDynamicMidiData);
	gDynamicMidiData = new DynamicMidiData(midiOutGen, portMap);
}

void
DynamicMidiCommand::ReleaseDynamicData()
{
	auto tmp = gDynamicMidiData;
	gDynamicMidiData = nullptr;
	delete tmp;
}

void
DynamicMidiCommand::Exec()
{
	auto pMidiData = gDynamicMidiData;
	if (!pMidiData)
	{
		_ASSERTE(pMidiData);
		return;
	}

	IMidiOutPtr	curMidiOut = mMidiOut ? mMidiOut : pMidiData->GetDynamicMidiOut();
	if (!curMidiOut)
		return;

	Bytes commandString(mCommandStringTemplate);

	switch (commandString.size())
	{
	case 3:
		if ((commandString[0] & 0xF0) == 0x90)
		{
			// Note on
			if (mDynamicChannel)
				commandString[0] = commandString[0] | pMidiData->GetDynamicChannel();

			if (mDynamicVelocity)
				commandString[2] = pMidiData->GetDynamicVelocity();
		}
		else if (mDynamicChannel && (commandString[0] & 0xF0) == 0x80)
		{
			// Note off -- don't use dynamic velocity
			commandString[0] = commandString[0] | pMidiData->GetDynamicChannel();
		}

		curMidiOut->MidiOut(commandString[0], commandString[1], commandString[2]);
		break;
	case 2:
		if (mDynamicChannel && (commandString[0] & 0xF0) == 0xc0)
			commandString[0] = commandString[0] | pMidiData->GetDynamicChannel();

		curMidiOut->MidiOut(commandString[0], commandString[1]);
		break;
	case 0:
		break;
	default:
		curMidiOut->MidiOut(commandString);
	}
}

void
SetDynamicPortCommand::Exec()
{
	auto pMidiData = gDynamicMidiData;
	if (!pMidiData)
	{
		_ASSERTE(pMidiData);
		return;
	}

	pMidiData->SetDynamicOutPort(mPort);
}

void
SetDynamicChannelCommand::Exec()
{
	auto pMidiData = gDynamicMidiData;
	if (!pMidiData)
	{
		_ASSERTE(pMidiData);
		return;
	}

	pMidiData->SetDynamicChannel(mChannel);
}

void
SetDynamicChannelVelocityCommand::Exec()
{
	auto pMidiData = gDynamicMidiData;
	if (!pMidiData)
	{
		_ASSERTE(pMidiData);
		return;
	}

	pMidiData->SetDynamicChannelVelocity(mVelocity);
}

void
SetDynamicChannelRandomVelocityCommand::Exec()
{
	auto pMidiData = gDynamicMidiData;
	if (!pMidiData)
	{
		_ASSERTE(pMidiData);
		return;
	}

	pMidiData->SetDynamicChannelRandomVelocity(mMinVelocity, mMaxVelocity);
}
