/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2010,2013,2015 Sean Echevarria
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
#include "..\Engine\ExpressionPedals.h"
#include "..\Engine\ITrollApplication.h"

class ControlUi;
class QAction;


class MainTrollWindow : public QMainWindow, ITrollApplication
{
	Q_OBJECT;
public:
	MainTrollWindow();
	~MainTrollWindow();

private slots:
	void About();
	void OpenFile();
	void SuspendMidiToggle(bool checked);
	virtual void Refresh();
	virtual void Reconnect();
	virtual void ToggleTraceWindow();
	virtual std::string ApplicationDirectory();
	void ToggleAdc0Override(bool checked) { ToggleAdcOverride(0, checked); }
	void ToggleAdc1Override(bool checked) { ToggleAdcOverride(1, checked); }
	void ToggleAdc2Override(bool checked) { ToggleAdcOverride(2, checked); }
	void ToggleAdc3Override(bool checked) { ToggleAdcOverride(3, checked); }

private:
	virtual bool IsAdcOverridden(int adc) { if (adc >=0 && adc < 4) return mAdcForceDisable[adc]; return false;}
	virtual void ToggleAdcOverride(int adc) { if (adc >=0 && adc < 4) ToggleAdcOverride(adc, !mAdcForceDisable[adc]); }
	virtual bool EnableTimeDisplay(bool enable);
	virtual std::string GetElapsedTimeStr() override;
	virtual void ResetTime() override;
	virtual void PauseOrResumeTime() override;
	virtual void Exit(ExitAction action);

private:
	ControlUi	* mUi;
	QString		mConfigFilename;
	QString		mUiFilename;
	bool		mAdcForceDisable[ExpressionPedals::PedalCount];
	QAction		* mAdcOverrideActions[ExpressionPedals::PedalCount];
	QAction		* mMidiSuspendAction;
	QDateTime	mStartTime;
	QDateTime	mPauseTime;
	ExitAction	mShutdownOnExit;

private:
	void ToggleAdcOverride(int adc, bool checked);
};

#endif // MainTrollWindow_h__
