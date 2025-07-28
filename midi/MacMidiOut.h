/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2008,2013,2018,2020,2022 Sean Echevarria
 */

#ifndef MacMidiOut_h__
#define MacMidiOut_h__

#include "../Engine/IMidiOut.h"
#include <CoreMIDI/CoreMIDI.h>
#include <thread>
#include <atomic>
#include <chrono>

class ITraceDisplay;

class MacMidiOut : public IMidiOut
{
public:
    MacMidiOut(ITraceDisplay * trace);
    virtual ~MacMidiOut();

    // IMidiOut
    virtual unsigned int GetMidiOutDeviceCount() const override;
    virtual std::string GetMidiOutDeviceName(unsigned int deviceIdx) const override;
    virtual std::string GetMidiOutDeviceName() const override;
    virtual void SetActivityIndicator(ISwitchDisplay * activityIndicator, int activityIndicatorIdx, unsigned int ledColor) override;
    virtual void EnableActivityIndicator(bool enable) override;
    virtual bool OpenMidiOut(unsigned int deviceIdx) override;
    virtual bool IsMidiOutOpen() const override { return mClient != 0 && mPort != 0; }
    virtual bool MidiOut(const Bytes & bytes, bool useIndicator = true) override;
    virtual void MidiOut(byte singleByte, bool useIndicator = true) override;
    virtual void MidiOut(byte byte1, byte byte2, bool useIndicator = true) override;
    virtual void MidiOut(byte byte1, byte byte2, byte byte3, bool useIndicator = true) override;
    virtual void EnableMidiClock(bool enable) override;
    virtual bool IsMidiClockEnabled() override { return mClockEnabled && mClockThread.joinable(); }
    virtual void SetTempo(int bpm) override;
    virtual int GetTempo() const override;
    virtual bool SuspendMidiOut() override;
    virtual bool ResumeMidiOut() override;
    virtual void CloseMidiOut() override;

private:
    void ReportError(const std::string& msg);
    void ReportError(const std::string& msg, int param1);
    void IndicateActivity();
    void TurnOffIndicator();
    void ClockThreadFunc();
    void StopClockThread();

    std::string mName;
    ITraceDisplay* mTrace;
    ISwitchDisplay* mActivityIndicator;
    std::atomic<bool> mEnableActivityIndicator;
    int mActivityIndicatorIndex;
    MIDIClientRef mClient;
    MIDIPortRef mPort;
    MIDIEndpointRef mDestination;
    unsigned int mDeviceIdx;
    unsigned int mLedColor;
    std::atomic<bool> mSuspended;

    // Clock support
    std::thread mClockThread;
    std::atomic<bool> mRunClockThread;
    std::atomic<bool> mClockEnabled;
    std::atomic<int> mTempo;
    std::chrono::microseconds mTickInterval;
};

#endif // MacMidiOut_h__