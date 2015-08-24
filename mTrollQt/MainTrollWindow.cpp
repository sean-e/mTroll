/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2010,2012-2013,2015 Sean Echevarria
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

#include "MainTrollWindow.h"
#include <QMenu>
#include <QMenuBar>
#include <qsettings.h>
#include <QApplication>
#include <qcoreapplication.h>
#include <QFileDialog>
#include "AboutDlg.h"
#include "ControlUi.h"
#include "..\Engine\ScopeSet.h"
#if defined(Q_OS_WIN)
#include <windows.h>
#include <powrprof.h>
#endif


#define kOrganizationKey	QString("Fester")
#define kOrganizationDomain	QString("creepingfog.com")
#define kAppKey				QString("mTroll")
#define kActiveUiFile		QString("UiFile")
#define kActiveConfigFile	QString("ConfigFile")
#define kAdcOverride		QString("AdcOverride%1")


MainTrollWindow::MainTrollWindow() : 
	QMainWindow(),
	mUi(NULL),
	mMidiSuspendAction(NULL),
	mStartTime(QDateTime::currentDateTime()),
	mShutdownOnExit(soeExit)
{
	QCoreApplication::setOrganizationName(kOrganizationKey);
	QCoreApplication::setOrganizationDomain(kOrganizationDomain);
	QCoreApplication::setApplicationName(kAppKey);

	setWindowTitle(tr("mTroll MIDI Controller"));
	QSettings settings;

	// http://doc.qt.nokia.com/4.4/stylesheet-examples.html#customizing-qmenubar
	// http://qt-project.org/doc/qt-5.0/qtwidgets/stylesheet-examples.html#customizing-qmenubar
	QString menuBarStyle(" \
		QMenuBar { \
			background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, \
											   stop:0 lightgray, stop:1 darkgray); \
		} \
 \
		QMenuBar::item { \
			spacing: 3px; /* spacing between menu bar items */ \
			padding: 1px 10px; \
			background: transparent; \
		} \
 \
		QMenuBar::item:selected { /* when selected using mouse or keyboard */ \
			background: #a8a8a8; \
		} \
 \
		QMenuBar::item:pressed { \
			background: #888888; \
		} \
	");

	menuBar()->setStyleSheet(menuBarStyle);
#if defined(Q_OS_WIN)
	::UnregisterTouchWindow((HWND)menuBar()->winId());
#endif

	QString touchMenuStyle(" \
		QMenu::item { \
		    padding: 15px; \
			background: white; \
			foreground: black; \
			font: bold 14px; \
			min-width: 13em; \
		} \
\
		QMenu::item:selected { \
			background: #505050; \
			border: 1px solid black; \
			color: white; \
		} \
\
	");

	bool hasTouchInput = false;
#if defined(Q_OS_WIN)
	// http://msdn.microsoft.com/en-us/library/windows/desktop/dd371581%28v=vs.85%29.aspx
	int systemDigitizer = ::GetSystemMetrics(SM_DIGITIZER);
	if (systemDigitizer & NID_READY)
	{
		if (systemDigitizer & (NID_EXTERNAL_TOUCH | NID_INTEGRATED_TOUCH | NID_MULTI_INPUT))
			hasTouchInput = true;
	}
#endif

	QMenu * fileMenu = menuBar()->addMenu(tr("&File"));
#if defined(Q_OS_WIN)
	::UnregisterTouchWindow((HWND)fileMenu->winId());
#endif
	if (hasTouchInput)
		fileMenu->setStyleSheet(touchMenuStyle);
	fileMenu->addAction(tr("&Open..."), this, SLOT(OpenFile()), QKeySequence(tr("Ctrl+O")));
	fileMenu->addAction(tr("&Refresh"), this, SLOT(Refresh()), QKeySequence(tr("F5")));
	fileMenu->addAction(tr("Re&connect to monome device"), this, SLOT(Reconnect()), QKeySequence(tr("Ctrl+R")));
	fileMenu->addAction(tr("&Toggle trace window visibility"), this, SLOT(ToggleTraceWindow()), QKeySequence(tr("Ctrl+T")));

	mMidiSuspendAction = new QAction(tr("Suspend &MIDI connections"), this);
	mMidiSuspendAction->setCheckable(true);
	mMidiSuspendAction->setChecked(false);
	connect(mMidiSuspendAction, SIGNAL(toggled(bool)), this, "1SuspendMidiToggle(bool)");
	fileMenu->addAction(mMidiSuspendAction);

	if (!hasTouchInput)
		fileMenu->addSeparator();
	fileMenu->addAction(tr("E&xit"), this, SLOT(close()));
	// http://doc.qt.nokia.com/4.4/stylesheet-examples.html#customizing-qmenu

	QString slotName;
	QMenu * adcMenu = menuBar()->addMenu(tr("&Pedal Overrides"));
#if defined(Q_OS_WIN)
	::UnregisterTouchWindow((HWND)adcMenu->winId());
#endif
	if (hasTouchInput)
		adcMenu->setStyleSheet(touchMenuStyle);
	for (int idx = 0; idx < ExpressionPedals::PedalCount; ++idx)
	{
		mAdcForceDisable[idx] = settings.value(kAdcOverride.arg(idx), false).toBool();
		mAdcOverrideActions[idx] = new QAction(tr("Disable ADC &%1").arg(idx), this);
		mAdcOverrideActions[idx]->setCheckable(true);
		mAdcOverrideActions[idx]->setChecked(mAdcForceDisable[idx]);
		slotName = QString("1ToggleAdc%1Override(bool)").arg(idx);
		connect(mAdcOverrideActions[idx], SIGNAL(toggled(bool)), this, slotName.toUtf8().constData());
		adcMenu->addAction(mAdcOverrideActions[idx]);
	}

	QMenu * helpMenu = menuBar()->addMenu(tr("&Help"));
#if defined(Q_OS_WIN)
	::UnregisterTouchWindow((HWND)helpMenu->winId());
#endif
	if (hasTouchInput)
		helpMenu->setStyleSheet(touchMenuStyle);
	helpMenu->addAction(tr("&About mTroll..."), this, SLOT(About()));

	mUiFilename = settings.value(kActiveUiFile, "config/testdata.ui.xml").value<QString>();
	mConfigFilename = settings.value(kActiveConfigFile, "config/testdata.config.xml").value<QString>();

	Refresh();
}

MainTrollWindow::~MainTrollWindow()
{
	switch (mShutdownOnExit)
	{
	case soeExit:
		break;
	case soeExitAndSleep:
#if defined(Q_OS_WIN)
		SetSuspendState(FALSE, FALSE, FALSE);
#endif
		break;
	case soeExitAndHibernate:
#if defined(Q_OS_WIN)
		SetSuspendState(TRUE, FALSE, FALSE);
#endif
		break;
	}
}

void
MainTrollWindow::About()
{
	AboutDlg dlg;
	dlg.exec();
}

void
MainTrollWindow::OpenFile()
{
	const QString cfgFleSelection = QFileDialog::getOpenFileName(this, 
			tr("Select Config Settings File"),
			mConfigFilename,
			tr("Config files (*.config.xml)"));
	if (cfgFleSelection.isEmpty())
		return;

	const QString uiFileSelection = QFileDialog::getOpenFileName(this, 
			tr("Select UI Settings File"),
			mUiFilename,
			tr("UI files (*.ui.xml)"));
	if (uiFileSelection.isEmpty())
		return;

	mConfigFilename = cfgFleSelection;
	mUiFilename = uiFileSelection;

	QSettings settings;
	settings.setValue(kActiveUiFile, mUiFilename);
	settings.setValue(kActiveConfigFile, mConfigFilename);

	Refresh();
}

void
MainTrollWindow::Refresh()
{
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	if (mUi)
		mUi->Unload();
	mUi = new ControlUi(this, this);
	setCentralWidget(mUi);	// Qt deletes the previous central widget

	const std::string uiFile(mUiFilename.toUtf8());
	const std::string cfgFile(mConfigFilename.toUtf8());
	mUi->Load(uiFile, cfgFile, mAdcForceDisable);

	int width, height;
	mUi->GetPreferredSize(width, height);
	if (width && height)
	{
#ifdef _WINDOWS
		// don't resize if maximized, but do change the size that
		// will be used when it becomes unmaximized.
		// Is there a way to do this with Qt?
		WINDOWPLACEMENT wp;
		::ZeroMemory(&wp, sizeof(WINDOWPLACEMENT));
		::GetWindowPlacement((HWND)winId(), &wp);
		if (SW_MAXIMIZE == wp.showCmd)
		{
			NONCLIENTMETRICS ncm;
			::ZeroMemory(&ncm, sizeof(NONCLIENTMETRICS));
			ncm.cbSize = sizeof(NONCLIENTMETRICS);
			::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
			wp.rcNormalPosition.right = wp.rcNormalPosition.left + width + (ncm.iBorderWidth * 2);
			wp.rcNormalPosition.bottom = wp.rcNormalPosition.top + height + ncm.iCaptionHeight + (ncm.iBorderWidth * 2);
			::SetWindowPlacement((HWND)winId(), &wp);
		}
		else
			resize(width, height);
#else
		if (!isMaximized())
			resize(width, height);
#endif
	}

	QApplication::restoreOverrideCursor();
}

void
MainTrollWindow::Reconnect()
{
	if (mUi)
		mUi->Reconnect();
}

void
MainTrollWindow::ToggleTraceWindow()
{
	if (mUi)
		mUi->ToggleTraceWindow();
}

void
MainTrollWindow::ToggleAdcOverride(int adc, 
								   bool checked)
{
	static bool sHandlingToggle = false; // handle recursion
	if (sHandlingToggle)
		return;

	ScopeSet<bool> s(&sHandlingToggle, true);
	mAdcForceDisable[adc] = checked;

	if (checked)
	{
		mAdcOverrideActions[adc]->setChecked(true);
		// if 2 is disabled, then 3 and 4 also need to be disabled
		for (int idx = adc + 1; idx < ExpressionPedals::PedalCount; ++idx)
		{
			if (!mAdcForceDisable[idx])
			{
				mAdcForceDisable[idx] = true;
				mAdcOverrideActions[idx]->setChecked(true);
			}
		}
	}
	else
	{
		mAdcOverrideActions[adc]->setChecked(false);
		// if 2 is enabled, then 0 and 1 also need to be enabled
		for (int idx = 0; idx < adc; ++idx)
		{
			if (mAdcForceDisable[idx])
			{
				mAdcForceDisable[idx] = false;
				mAdcOverrideActions[idx]->setChecked(false);
			}
		}
	}

	if (mUi)
		mUi->UpdateAdcs(mAdcForceDisable);

	QSettings settings;
	for (int idx = 0; idx < ExpressionPedals::PedalCount; ++idx)
		settings.setValue(kAdcOverride.arg(idx), mAdcForceDisable[idx]);
}

std::string
MainTrollWindow::GetElapsedTimeStr()
{
	const int kSecsInMinute = 60;
	const int kSecsInHour = kSecsInMinute * 60;
	const int kSecsInDay = kSecsInHour * 24;
	QDateTime now(QDateTime::currentDateTime());
	QString ts(now.toString("h:mm:ss ap \nddd, MMM d, yyyy"));

	qint64 secs = mStartTime.secsTo(now);
	const int days = secs > kSecsInDay ? secs / kSecsInDay : 0;
	if (days)
		secs -= (days * kSecsInDay);
	const int hours = secs > kSecsInHour ? secs / kSecsInHour : 0;
	if (hours)
		secs -= (hours * kSecsInHour);
	const int minutes = secs > kSecsInMinute ? secs / kSecsInMinute : 0;
	if (minutes)
		secs -= (minutes * kSecsInMinute);

	QString tmp;
	if (days)
		tmp.sprintf("\nelapsed: %d days, %d:%02d:%02d", days, hours, minutes, secs);
	else if (hours)
		tmp.sprintf("\nelapsed: %d hours, %02d:%02d", hours, minutes, secs);
	else
		tmp.sprintf("\nelapsed: %02d:%02d", minutes, secs);
	ts += tmp;

	const std::string msg(ts.toUtf8());
	return msg;
}

void
MainTrollWindow::ResetTime()
{
	mStartTime = QDateTime::currentDateTime();
}

void
MainTrollWindow::Exit(ExitAction action)
{
	mShutdownOnExit = action;
	// exit application
	close();
}

bool
MainTrollWindow::EnableTimeDisplay(bool enable)
{
	if (mUi)
		return mUi->EnableTimeDisplay(enable);
	return false;
}

std::string
MainTrollWindow::ApplicationDirectory()
{
	const QString path(QApplication::applicationDirPath());
	const std::string pathStd(path.toLatin1());
	return pathStd;
}

void
MainTrollWindow::SuspendMidiToggle(bool checked)
{
	if (checked)
	{
		if (mUi)
			mUi->SuspendMidi();
	}
	else 
	{
		if (mUi)
			mUi->ResumeMidi();
	}
}
