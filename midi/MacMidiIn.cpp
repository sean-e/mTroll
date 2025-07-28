/*
 * mTroll MIDI Controller
 * Copyright (C) 2010,2013,2018,2022 Sean Echevarria
 */

#include "MacMidiIn.h"
#include "../Engine/ITraceDisplay.h"
#include "../Engine/IMidiInSubscriber.h"
#include <CoreFoundation/CoreFoundation.h>
#include <iostream>
#include <sstream>

MacMidiIn::MacMidiIn(ITraceDisplay * trace)
    : mTrace(trace)
    , mClient(0)
    , mPort(0)
    , mSource(0)
    , mDeviceIdx(0)
    , mSuspended(false)
{
    OSStatus result = MIDIClientCreate(CFSTR("mTroll MIDI Client"), nullptr, nullptr, &mClient);
    if (result != noErr) {
        ReportError("Failed to create MIDI client", result);
    }
}

MacMidiIn::~MacMidiIn()
{
    CloseMidiIn();
    
    if (mClient) {
        MIDIClientDispose(mClient);
        mClient = 0;
    }
}

unsigned int MacMidiIn::GetMidiInDeviceCount() const
{
    return MIDIGetNumberOfSources();
}

std::string MacMidiIn::GetMidiInDeviceName(unsigned int deviceIdx) const
{
    if (deviceIdx >= GetMidiInDeviceCount())
        return "";
    
    MIDIEndpointRef source = MIDIGetSource(deviceIdx);
    if (source == 0)
        return "";
    
    CFStringRef name = nullptr;
    OSStatus result = MIDIObjectGetStringProperty(source, kMIDIPropertyName, &name);
    if (result != noErr || !name)
        return "";
    
    char buffer[256];
    CFStringGetCString(name, buffer, sizeof(buffer), kCFStringEncodingUTF8);
    CFRelease(name);
    
    return std::string(buffer);
}

bool MacMidiIn::OpenMidiIn(unsigned int deviceIdx)
{
    if (mPort != 0) {
        CloseMidiIn();
    }
    
    if (deviceIdx >= GetMidiInDeviceCount()) {
        ReportError("Invalid MIDI input device index", deviceIdx);
        return false;
    }
    
    mDeviceIdx = deviceIdx;
    mSource = MIDIGetSource(deviceIdx);
    if (mSource == 0) {
        ReportError("Failed to get MIDI source", deviceIdx);
        return false;
    }
    
    if (mClient == 0) {
        ReportError("MIDI client not initialized");
        return false;
    }
    
    OSStatus result = MIDIInputPortCreate(mClient, CFSTR("mTroll Input Port"), MidiInputProc, this, &mPort);
    if (result != noErr) {
        ReportError("Failed to create MIDI input port", result);
        return false;
    }
    
    result = MIDIPortConnectSource(mPort, mSource, nullptr);
    if (result != noErr) {
        ReportError("Failed to connect to MIDI source", result);
        MIDIPortDispose(mPort);
        mPort = 0;
        return false;
    }
    
    mSuspended = false;
    
    if (mTrace) {
        std::string deviceName = GetMidiInDeviceName(deviceIdx);
        mTrace->Trace(std::string("Opened MIDI input device: ") + deviceName);
    }
    
    return true;
}

bool MacMidiIn::Subscribe(IMidiInSubscriberPtr sub)
{
    if (!sub)
        return false;
    
    // Check if already subscribed
    for (const auto& existing : mInputSubscribers) {
        if (existing == sub)
            return true;
    }
    
    mInputSubscribers.push_back(sub);
    return true;
}

void MacMidiIn::Unsubscribe(IMidiInSubscriberPtr sub)
{
    if (!sub)
        return;
    
    mInputSubscribers.erase(
        std::remove(mInputSubscribers.begin(), mInputSubscribers.end(), sub),
        mInputSubscribers.end()
    );
}

bool MacMidiIn::SuspendMidiIn()
{
    mSuspended = true;
    return true;
}

bool MacMidiIn::ResumeMidiIn()
{
    mSuspended = false;
    return true;
}

void MacMidiIn::CloseMidiIn()
{
    if (mPort != 0) {
        if (mSource != 0) {
            MIDIPortDisconnectSource(mPort, mSource);
            mSource = 0;
        }
        MIDIPortDispose(mPort);
        mPort = 0;
    }
    
    if (mTrace) {
        mTrace->Trace("Closed MIDI input device");
    }
}

void MacMidiIn::MidiInputProc(const MIDIPacketList *pktlist, void *refCon, void *connRefCon)
{
    MacMidiIn* midiIn = static_cast<MacMidiIn*>(refCon);
    if (midiIn) {
        midiIn->ProcessMidiData(pktlist);
    }
}

void MacMidiIn::ProcessMidiData(const MIDIPacketList *pktlist)
{
    if (mSuspended || mInputSubscribers.empty())
        return;
    
    const MIDIPacket *packet = &pktlist->packet[0];
    for (UInt32 i = 0; i < pktlist->numPackets; ++i) {
        // Process each MIDI message in the packet
        const Byte* data = packet->data;
        UInt16 length = packet->length;
        
        // Parse MIDI messages and call subscribers
        for (UInt16 j = 0; j < length; ) {
            if (data[j] >= 0xF0) {
                // System message (including SysEx)
                if (data[j] == 0xF0) {
                    // SysEx start - find end
                    UInt16 sysexStart = j;
                    while (j < length && data[j] != 0xF7) {
                        j++;
                    }
                    if (j < length && data[j] == 0xF7) {
                        j++; // Include the 0xF7
                        // Send SysEx to subscribers
                        for (const auto& subscriber : mInputSubscribers) {
                            if (subscriber) {
                                subscriber->ReceivedSysex(&data[sysexStart], j - sysexStart);
                            }
                        }
                    }
                } else {
                    // Other system messages (1 byte)
                    for (const auto& subscriber : mInputSubscribers) {
                        if (subscriber) {
                            subscriber->ReceivedData(data[j], 0, 0);
                        }
                    }
                    j++;
                }
            } else if (data[j] >= 0x80) {
                // Channel message
                byte status = data[j++];
                byte data1 = (j < length) ? data[j++] : 0;
                byte data2 = 0;
                
                // Determine if this message needs a second data byte
                byte msgType = status & 0xF0;
                if (msgType != 0xC0 && msgType != 0xD0) { // Not Program Change or Channel Pressure
                    data2 = (j < length) ? data[j++] : 0;
                }
                
                for (const auto& subscriber : mInputSubscribers) {
                    if (subscriber) {
                        subscriber->ReceivedData(status, data1, data2);
                    }
                }
            } else {
                // Running status or invalid data - skip
                j++;
            }
        }
        
        packet = MIDIPacketNext(packet);
    }
}

void MacMidiIn::ReportError(const std::string& msg)
{
    if (mTrace) {
        mTrace->Trace(std::string("MacMidiIn Error: ") + msg);
    }
}

void MacMidiIn::ReportError(const std::string& msg, int param1)
{
    if (mTrace) {
        std::ostringstream oss;
        oss << "MacMidiIn Error: " << msg << " (" << param1 << ")";
        mTrace->Trace(oss.str());
    }
}