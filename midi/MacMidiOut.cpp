/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2008,2013,2018,2020,2022 Sean Echevarria
 */

#include "MacMidiOut.h"
#include "../Engine/ITraceDisplay.h"
#include "../Engine/ISwitchDisplay.h"
#include "../Engine/EngineLoader.h"
#include <CoreFoundation/CoreFoundation.h>
#include <iostream>
#include <sstream>

MacMidiOut::MacMidiOut(ITraceDisplay * trace)
    : mTrace(trace)
    , mActivityIndicator(nullptr)
    , mEnableActivityIndicator(false)
    , mActivityIndicatorIndex(-1)
    , mClient(0)
    , mPort(0)
    , mDestination(0)
    , mDeviceIdx(0)
    , mLedColor(kFirstColorPreset)
    , mSuspended(false)
    , mRunClockThread(false)
    , mClockEnabled(false)
    , mTempo(120)
    , mTickInterval(std::chrono::microseconds(20833)) // 120 BPM = 20833 microseconds per tick
{
    OSStatus result = MIDIClientCreate(CFSTR("mTroll MIDI Client"), nullptr, nullptr, &mClient);
    if (result != noErr) {
        ReportError("Failed to create MIDI client", result);
    }
}

MacMidiOut::~MacMidiOut()
{
    CloseMidiOut();
    
    if (mClient) {
        MIDIClientDispose(mClient);
        mClient = 0;
    }
}

unsigned int MacMidiOut::GetMidiOutDeviceCount() const
{
    return MIDIGetNumberOfDestinations();
}

std::string MacMidiOut::GetMidiOutDeviceName(unsigned int deviceIdx) const
{
    if (deviceIdx >= GetMidiOutDeviceCount())
        return "";
    
    MIDIEndpointRef dest = MIDIGetDestination(deviceIdx);
    if (dest == 0)
        return "";
    
    CFStringRef name = nullptr;
    OSStatus result = MIDIObjectGetStringProperty(dest, kMIDIPropertyName, &name);
    if (result != noErr || !name)
        return "";
    
    char buffer[256];
    CFStringGetCString(name, buffer, sizeof(buffer), kCFStringEncodingUTF8);
    CFRelease(name);
    
    return std::string(buffer);
}

std::string MacMidiOut::GetMidiOutDeviceName() const
{
    return mName;
}

void MacMidiOut::SetActivityIndicator(ISwitchDisplay * activityIndicator, int activityIndicatorIdx, unsigned int ledColor)
{
    mActivityIndicator = activityIndicator;
    mActivityIndicatorIndex = activityIndicatorIdx;
    mLedColor = ledColor;
}

void MacMidiOut::EnableActivityIndicator(bool enable)
{
    mEnableActivityIndicator = enable;
    if (!enable && mActivityIndicator) {
        TurnOffIndicator();
    }
}

bool MacMidiOut::OpenMidiOut(unsigned int deviceIdx)
{
    if (mPort != 0) {
        CloseMidiOut();
    }
    
    if (deviceIdx >= GetMidiOutDeviceCount()) {
        ReportError("Invalid MIDI output device index", deviceIdx);
        return false;
    }
    
    mDeviceIdx = deviceIdx;
    mDestination = MIDIGetDestination(deviceIdx);
    if (mDestination == 0) {
        ReportError("Failed to get MIDI destination", deviceIdx);
        return false;
    }
    
    if (mClient == 0) {
        ReportError("MIDI client not initialized");
        return false;
    }
    
    OSStatus result = MIDIOutputPortCreate(mClient, CFSTR("mTroll Output Port"), &mPort);
    if (result != noErr) {
        ReportError("Failed to create MIDI output port", result);
        return false;
    }
    
    mName = GetMidiOutDeviceName(deviceIdx);
    mSuspended = false;
    
    if (mTrace) {
        mTrace->Trace(std::string("Opened MIDI output device: ") + mName);
    }
    
    return true;
}

bool MacMidiOut::MidiOut(const Bytes & bytes, bool useIndicator)
{
    if (mSuspended || mPort == 0 || mDestination == 0 || bytes.empty())
        return false;
    
    // Create MIDI packet
    Byte packetBuffer[1024];
    MIDIPacketList* packetList = (MIDIPacketList*)packetBuffer;
    MIDIPacket* packet = MIDIPacketListInit(packetList);
    
    packet = MIDIPacketListAdd(packetList, sizeof(packetBuffer), packet, 0, bytes.size(), bytes.data());
    if (packet == nullptr) {
        ReportError("Failed to create MIDI packet");
        return false;
    }
    
    OSStatus result = MIDISend(mPort, mDestination, packetList);
    if (result != noErr) {
        ReportError("Failed to send MIDI data", result);
        return false;
    }
    
    if (useIndicator) {
        IndicateActivity();
    }
    
    return true;
}

void MacMidiOut::MidiOut(byte singleByte, bool useIndicator)
{
    Bytes bytes = { singleByte };
    MidiOut(bytes, useIndicator);
}

void MacMidiOut::MidiOut(byte byte1, byte byte2, bool useIndicator)
{
    Bytes bytes = { byte1, byte2 };
    MidiOut(bytes, useIndicator);
}

void MacMidiOut::MidiOut(byte byte1, byte byte2, byte byte3, bool useIndicator)
{
    Bytes bytes = { byte1, byte2, byte3 };
    MidiOut(bytes, useIndicator);
}

void MacMidiOut::EnableMidiClock(bool enable)
{
    if (enable == mClockEnabled)
        return;
    
    mClockEnabled = enable;
    
    if (enable) {
        if (!mClockThread.joinable()) {
            mRunClockThread = true;
            mClockThread = std::thread(&MacMidiOut::ClockThreadFunc, this);
        }
    } else {
        StopClockThread();
    }
}

void MacMidiOut::SetTempo(int bpm)
{
    if (bpm < 30) bpm = 30;
    if (bpm > 300) bpm = 300;
    
    mTempo = bpm;
    
    // Calculate tick interval in microseconds
    // MIDI clock sends 24 ticks per quarter note
    // So for BPM beats per minute: (60 * 1000000) / (BPM * 24) microseconds per tick
    int tickIntervalMicros = (60 * 1000000) / (bpm * 24);
    mTickInterval = std::chrono::microseconds(tickIntervalMicros);
}

int MacMidiOut::GetTempo() const
{
    return mTempo;
}

bool MacMidiOut::SuspendMidiOut()
{
    mSuspended = true;
    StopClockThread();
    return true;
}

bool MacMidiOut::ResumeMidiOut()
{
    mSuspended = false;
    if (mClockEnabled) {
        EnableMidiClock(true);
    }
    return true;
}

void MacMidiOut::CloseMidiOut()
{
    StopClockThread();
    
    if (mPort != 0) {
        MIDIPortDispose(mPort);
        mPort = 0;
    }
    
    mDestination = 0;
    mName.clear();
    
    if (mTrace) {
        mTrace->Trace("Closed MIDI output device");
    }
}

void MacMidiOut::IndicateActivity()
{
    if (mEnableActivityIndicator && mActivityIndicator && mActivityIndicatorIndex >= 0) {
        mActivityIndicator->SetSwitchDisplay(mActivityIndicatorIndex, mLedColor);
        
        // Schedule to turn off indicator after a short delay
        // Note: This is a simplified implementation. A production version might use a timer.
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            TurnOffIndicator();
        }).detach();
    }
}

void MacMidiOut::TurnOffIndicator()
{
    if (mActivityIndicator && mActivityIndicatorIndex >= 0) {
        mActivityIndicator->TurnOffSwitchDisplay(mActivityIndicatorIndex);
    }
}

void MacMidiOut::ClockThreadFunc()
{
    const byte midiClockByte = 0xF8;
    
    while (mRunClockThread && mClockEnabled) {
        auto nextTick = std::chrono::steady_clock::now() + mTickInterval;
        
        if (!mSuspended) {
            MidiOut(midiClockByte, false); // Don't use activity indicator for clock
        }
        
        std::this_thread::sleep_until(nextTick);
    }
}

void MacMidiOut::StopClockThread()
{
    mRunClockThread = false;
    if (mClockThread.joinable()) {
        mClockThread.join();
    }
}

void MacMidiOut::ReportError(const std::string& msg)
{
    if (mTrace) {
        mTrace->Trace(std::string("MacMidiOut Error: ") + msg);
    }
}

void MacMidiOut::ReportError(const std::string& msg, int param1)
{
    if (mTrace) {
        std::ostringstream oss;
        oss << "MacMidiOut Error: " << msg << " (" << param1 << ")";
        mTrace->Trace(oss.str());
    }
}