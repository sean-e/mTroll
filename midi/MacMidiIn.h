/*
 * mTroll MIDI Controller
 * Copyright (C) 2010,2013,2018,2022 Sean Echevarria
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
 */

#ifndef MacMidiIn_h__
#define MacMidiIn_h__

#include "../Engine/IMidiIn.h"
#include <CoreMIDI/CoreMIDI.h>
#include <vector>
#include <thread>
#include <atomic>

class ITraceDisplay;

class MacMidiIn : public IMidiIn
{
public:
    MacMidiIn(ITraceDisplay * trace);
    virtual ~MacMidiIn();

    // IMidiIn
    virtual unsigned int GetMidiInDeviceCount() const override;
    virtual std::string GetMidiInDeviceName(unsigned int deviceIdx) const override;
    virtual bool OpenMidiIn(unsigned int deviceIdx) override;
    virtual bool IsMidiInOpen() const override { return mClient != 0 && mPort != 0; }
    virtual bool Subscribe(IMidiInSubscriberPtr sub) override;
    virtual void Unsubscribe(IMidiInSubscriberPtr sub) override;
    virtual bool SuspendMidiIn() override;
    virtual bool ResumeMidiIn() override;
    virtual void CloseMidiIn() override;

private:
    static void MidiInputProc(const MIDIPacketList *pktlist, void *refCon, void *connRefCon);
    void ProcessMidiData(const MIDIPacketList *pktlist);
    void ReportError(const std::string& msg);
    void ReportError(const std::string& msg, int param1);

    ITraceDisplay* mTrace;
    MIDIClientRef mClient;
    MIDIPortRef mPort;
    MIDIEndpointRef mSource;
    unsigned int mDeviceIdx;
    std::atomic<bool> mSuspended;
    
    using MidiInSubscribers = std::vector<IMidiInSubscriberPtr>;
    MidiInSubscribers mInputSubscribers;
};

#endif // MacMidiIn_h__