/*
 * mTroll MIDI Controller
 * Copyright (C) 2010 Sean Echevarria
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

#ifndef IMidiInSubscriber_h__
#define IMidiInSubscriber_h__

typedef unsigned char byte;
class IMidiIn;


// IMidiInSubscriber
// ----------------------------------------------------------------------------
// Implement to get notification of MIDI IN events
//
class IMidiInSubscriber
{
public:
	virtual void ReceivedData(byte b1, byte b2, byte b3) = 0;
	virtual void ReceivedSysex(const byte * bytes, int len) = 0;
	virtual void Closed(IMidiIn * midIn) = 0;
};

#endif // IMidiInSubscriber_h__
