/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2010,2013,2015,2018,2020,2024-2026 Sean Echevarria
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

#ifndef MainTrollWindow_h__
#define MainTrollWindow_h__

#include <QMainWindow>
#include <QDateTime>
#include <QAbstractNativeEventFilter>
#include "../Engine/ExpressionPedals.h"
#include "../Engine/ITrollApplication.h"
#include "../Engine/ITraceDisplay.h"

class ControlUi;
class QAction;


class MainTrollWindow : public QMainWindow, 
	public ITrollApplication,
	public ITraceDisplay,
	public QAbstractNativeEventFilter
{
	Q_OBJECT;
public:
	MainTrollWindow();
	~MainTrollWindow();

private slots:
	void About();
	void OpenDataFile() { OpenFile(true, false); }
	void OpenUiFile() { OpenFile(false, true); }
	void OpenConfigAndUiFiles() { OpenFile(true, true); }
	void SuspendMidiToggle(bool checked);
	void ToggleExpressionPedalDetails(bool checked);
	virtual void Refresh();
	virtual void Reconnect() override;
	virtual void ToggleTraceWindow() override;
	virtual std::string ApplicationDirectory() override;
	void OpenFile(bool config, bool ui);

	void ToggleAdc0Override(bool checked) { ToggleAdcOverride(0, checked); }
	void ToggleAdc1Override(bool checked) { ToggleAdcOverride(1, checked); }
	void ToggleAdc2Override(bool checked) { ToggleAdcOverride(2, checked); }
	void ToggleAdc3Override(bool checked) { ToggleAdcOverride(3, checked); }
	void IncreaseMainDisplayHeight();
	void DecreaseMainDisplayHeight();
	void LoadConfigMru1() { LoadConfigMruItem(1); }
	void LoadConfigMru2() { LoadConfigMruItem(2); }
	void LoadConfigMru3() { LoadConfigMruItem(3); }
	void LoadConfigMru4() { LoadConfigMruItem(4); }

public: // ITraceDisplay
	virtual void Trace(const std::string & txt) override;

	// QAbstractNativeEventFilter
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	virtual bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
#else
	virtual bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;
#endif

protected:
	virtual void closeEvent(QCloseEvent *event) override;

private:
	virtual bool IsHardwareAdcEnabled(int idx) const override;
	virtual void EnableHardwareAdc(int idx, bool enable) const override;
	virtual bool IsAdcOverridden(int adc) override { if (adc >=0 && adc < 4) return mAdcForceDisable[adc]; return false;}
	virtual void ToggleAdcOverride(int adc) override { if (adc >=0 && adc < 4) ToggleAdcOverride(adc, !mAdcForceDisable[adc]); }
	virtual bool EnableTimeDisplay(bool enable) override;
	virtual std::string GetElapsedTimeStr() override;
	virtual void ResetTime() override;
	virtual void PauseOrResumeTime() override;
	virtual void Exit(ExitAction action) override;

private:
	static constexpr int kMruCount = 4;
	ControlUi	* mUi = nullptr;
	QString		mConfigFilename;
	QString		mUiFilename;
	bool		mAdcForceDisable[ExpressionPedals::PedalCount];
	QAction		* mAdcOverrideActions[ExpressionPedals::PedalCount] = {};
	QAction		* mMidiSuspendAction = nullptr;
	QAction		* mToggleExpressionPedalDetailStatus = nullptr;
	QAction		* mIncreaseMainDisplayHeight = nullptr;
	QAction		* mDecreaseMainDisplayHeight = nullptr;
	QAction		* mMruActions[kMruCount] = {};
	QDateTime	mStartTime;
	QDateTime	mPauseTime;
#if defined(Q_OS_WIN)
	void		* mDevNotify = nullptr;
#endif
	ExitAction	mShutdownOnExit = soeExit;

private:
	void ToggleAdcOverride(int adc, bool checked);
	void LoadConfigMruItem(int idx);
	void UpdateMru();
	void RegisterDevicesNotification(bool registerDevNotification = true) noexcept;
};

#endif // MainTrollWindow_h__
