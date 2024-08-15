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


// class static member initializers
IMidiOutGenerator *DynamicMidiCommand::sMidiOutGenerator = nullptr;
MidiPortToDeviceIdxMap DynamicMidiCommand::sMidiOutPortToDeviceIdxMap { };
int DynamicMidiCommand::sDynamicOutPort = 0;
IMidiOutPtr DynamicMidiCommand::sDynamicMidiOut[kMaxDynamicPorts];
int DynamicMidiCommand::sDynamicChannel = 0;
constexpr int kDefaultVelocity = 127;
// upper and lower are same if not random; start out dynamic but not random
int DynamicMidiCommand::sRandomNoteVelocityLower[kMidiChannels] { 
	kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, 
	kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, 
	kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, 
	kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, kDefaultVelocity 
};
int DynamicMidiCommand::sRandomNoteVelocityUpper[kMidiChannels] { 
	kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, 
	kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, 
	kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, 
	kDefaultVelocity, kDefaultVelocity, kDefaultVelocity, kDefaultVelocity 
};
std::uniform_int_distribution<int> DynamicMidiCommand::sRandomVelocityDistribution[kMidiChannels] { };
std::default_random_engine DynamicMidiCommand::sGenerator{ };


void
DynamicMidiCommand::SetDynamicPortData(IMidiOutGenerator *midiOutGen, 
									   const MidiPortToDeviceIdxMap &portMap)
{
	sMidiOutGenerator = midiOutGen;
	sMidiOutPortToDeviceIdxMap = portMap;
}

void
DynamicMidiCommand::SetDynamicOutPort(int port)
{
	if (!sMidiOutGenerator)
	{
		_ASSERTE(sMidiOutGenerator);
		return;
	}

	if (port < 0 || port >= kMaxDynamicPorts)
	{
		_ASSERTE(!"unhandled dynamic port number");
		return;
	}

	sDynamicOutPort = port;

	if (sMidiOutPortToDeviceIdxMap.find(sDynamicOutPort) != sMidiOutPortToDeviceIdxMap.end())
		sDynamicMidiOut[sDynamicOutPort] = sMidiOutGenerator->GetMidiOut(sMidiOutPortToDeviceIdxMap[sDynamicOutPort]);
}

void
DynamicMidiCommand::SetDynamicChannel(int ch)
{
	if (ch >= 0 && ch < kMidiChannels) 
		sDynamicChannel = ch;
}

void
DynamicMidiCommand::SetDynamicChannelVelocity(int vel)
{
	sRandomNoteVelocityLower[sDynamicChannel] = sRandomNoteVelocityUpper[sDynamicChannel] = vel;
}

void
DynamicMidiCommand::SetDynamicChannelRandomVelocity(int lowerVel, int upperVel)
{
	if (lowerVel == upperVel)
	{
		SetDynamicChannelVelocity(lowerVel);
		return;
	}

	sRandomNoteVelocityLower[sDynamicChannel] = lowerVel;
	sRandomNoteVelocityUpper[sDynamicChannel] = upperVel;
	sRandomVelocityDistribution[sDynamicChannel] = std::uniform_int_distribution<int>(sRandomNoteVelocityLower[sDynamicChannel], sRandomNoteVelocityUpper[sDynamicChannel]);
}

void
DynamicMidiCommand::Exec()
{
	IMidiOutPtr	curMidiOut = mMidiOut ? mMidiOut : sDynamicMidiOut[sDynamicOutPort];
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
				commandString[0] = commandString[0] | sDynamicChannel;

			if (mDynamicVelocity)
			{
				if (sRandomNoteVelocityLower[sDynamicChannel] == sRandomNoteVelocityUpper[sDynamicChannel])
					commandString[2] = (byte)sRandomNoteVelocityLower[sDynamicChannel];
				else
					commandString[2] = (byte)sRandomVelocityDistribution[sDynamicChannel](sGenerator);
			}
		}
		else if (mDynamicChannel && (commandString[0] & 0xF0) == 0x80)
		{
			// Note off -- don't use dynamic velocity
			commandString[0] = commandString[0] | sDynamicChannel;
		}

		curMidiOut->MidiOut(commandString[0], commandString[1], commandString[2]);
		break;
	case 2:
		if (mDynamicChannel && (commandString[0] & 0xF0) == 0xc0)
			commandString[0] = commandString[0] | sDynamicChannel;

		curMidiOut->MidiOut(commandString[0], commandString[1]);
		break;
	case 0:
		break;
	default:
		curMidiOut->MidiOut(commandString);
	}
}
