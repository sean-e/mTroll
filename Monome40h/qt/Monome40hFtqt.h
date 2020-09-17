/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2009,2014,2018,2020 Sean Echevarria
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

#ifndef Monome40hFtqt_h__
#define Monome40hFtqt_h__

#include <qthread.h>
#include <qmutex.h>
#include <list>
#include "../IMonome40h.h"

class ITraceDisplay;
class IMonome40hSwitchSubscriber;
class IMonome40hAdcSubscriber;
class MonomeSerialProtocolData;


class Monome40hFtqt : public IMonome40h
{
public:
	Monome40hFtqt(ITraceDisplay * trace);
	virtual ~Monome40hFtqt();

public: // IMonome40h
	virtual void EnableLed(byte row, byte col, bool on) override;
	virtual void SetLedIntensity(byte brightness) override;
	virtual void TestLed(bool on) override;
	virtual void EnableAdc(byte port, bool on) override;
	virtual void Shutdown(bool state) override;
	virtual void EnableLedRow(byte row, byte columnValues) override;
	virtual void EnableLedColumn(byte column, byte rowValues) override;

	virtual bool Subscribe(IMonome40hSwitchSubscriber * sub) override;
	virtual bool Unsubscribe(IMonome40hSwitchSubscriber * sub) override;
	virtual bool Subscribe(IMonome40hAdcSubscriber * sub) override;
	virtual bool Unsubscribe(IMonome40hAdcSubscriber * sub) override;

	virtual int LocateMonomeDeviceIdx() override;
	virtual std::string GetDeviceSerialNumber(int devidx) override;
	virtual bool AcquireDevice(const std::string & devSerialNum) override;
	virtual bool IsAdcEnabled(int portIdx) const override;

	void DeviceServiceThread();

private:
	bool AcquireDevice();
	void ReleaseDevice();
	int Send(const MonomeSerialProtocolData & data);
	bool ReadInput(byte * readData);
	void ServiceCommands();
	void DispatchCommand(const MonomeSerialProtocolData * data);

	std::string						mDevSerialNumber;
	IMonome40hSwitchSubscriber		* mInputSubscriber;
	IMonome40hAdcSubscriber			* mAdcInputSubscriber;
	using OutputCommandQueue = std::list<const MonomeSerialProtocolData *>;
	QMutex							mOutputCommandsLock;
	OutputCommandQueue				mOutputCommandQueue;
	ITraceDisplay					* mTrace;
	using FT_HANDLE = void *;
	FT_HANDLE						mFtDevice;
	QThread							* mThread;
	Qt::HANDLE						mThreadId;
	volatile bool					mServicingSubscribers;
	volatile bool					mShouldContinueListening;
	int								mLedBrightness;
	enum {kAdcPortCount = 4, kAdcValhist = 3};
	bool							mAdcEnable[kAdcPortCount];

	// these members are for adc port filtering of jitter
	int								mPrevAdcVals[kAdcPortCount][kAdcValhist];
	int								mPrevAdcValsIndex[kAdcPortCount];
};

#endif // Monome40hFtqt_h__
