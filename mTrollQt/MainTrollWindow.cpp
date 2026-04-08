/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2010,2012-2013,2015,2018,2020,2023-2026 Sean Echevarria
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
#include <QScrollArea>
#include <QScrollBar>
#include "AboutDlg.h"
#include "ControlUi.h"
#include "../Engine/ScopeSet.h"
#if defined(Q_OS_WIN)
#include <windows.h>
#include <powrprof.h>
#include <dbt.h>
#include <ks.h>
#include <ksmedia.h>
#include <SetupAPI.h>
#include <initguid.h>   // include before devpropdef.h
#include <devpropdef.h>
#include <devpkey.h>
#include "../winUtil/WinDark.h"
#endif


#define kOrganizationKey	QString("Fester")
#define kOrganizationDomain	QString("creepingfog.com")
#define kAppKey				QString("mTroll")
#define kActiveUiFile		QString("UiFile")
#define kConfigMru			QString("MRUconfig")
#define kAdcOverride		QString("AdcOverride%1")
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#define kMainWindowGeom		QString("MainWindowGeometry5")
#else
#define kMainWindowGeom		QString("MainWindowGeometry")
#endif


MainTrollWindow::MainTrollWindow() : 
	mStartTime(QDateTime::currentDateTime()),
	mPauseTime(mStartTime)
{
	QCoreApplication::setOrganizationName(kOrganizationKey);
	QCoreApplication::setOrganizationDomain(kOrganizationDomain);
	QCoreApplication::setApplicationName(kAppKey);

#if defined(Q_OS_WIN)
	WinDark::setDarkTitlebar(HWND(winId()), true);
#endif

	setWindowTitle(tr("mTroll MIDI Controller"));
	QSettings settings;

	// http://doc.qt.nokia.com/4.4/stylesheet-examples.html#customizing-qmenubar
	// http://qt-project.org/doc/qt-5.0/qtwidgets/stylesheet-examples.html#customizing-qmenubar
	QString menuBarStyle(" \
		QMenuBar { \
			background-color: #1a1a1a; \
			color: white; \
		} \
 \
		QMenuBar::item { \
			spacing: 3px; /* spacing between menu bar items */ \
			padding: 1px 10px; \
			background: transparent; \
		} \
 \
		QMenuBar::item:selected { /* when selected using mouse or keyboard */ \
			background: #505050; \
		} \
 \
		QMenuBar::item:pressed { \
			background: #505050; \
		} \
	");

	menuBar()->setStyleSheet(menuBarStyle);
#if defined(Q_OS_WIN)
	::UnregisterTouchWindow((HWND)menuBar()->winId());
#endif

	QString touchMenuStyle(" \
		QMenu { \
			background: #1a1a1a; \
		} \
\
		QMenu::item { \
		    padding: 11px; \
			background: #1a1a1a; \
			color: white; \
			font: bold 10px; \
			min-width: 13em; \
		} \
\
		QMenu::item:selected { \
			background: #505050; \
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	fileMenu->addAction(tr("Open &Config..."), this, &MainTrollWindow::OpenDataFile, QKeySequence(tr("Ctrl+O")));
#else
	fileMenu->addAction(tr("Open &Config..."), QKeySequence(tr("Ctrl+O")), this, &MainTrollWindow::OpenDataFile);
#endif
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	fileMenu->addAction(tr("Open &UI..."), this, &MainTrollWindow::OpenUiFile, QKeySequence(tr("Ctrl+U")));
#else
	fileMenu->addAction(tr("Open &UI..."), QKeySequence(tr("Ctrl+U")), this, &MainTrollWindow::OpenUiFile);
#endif
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	fileMenu->addAction(tr("&Open..."), this, &MainTrollWindow::OpenConfigAndUiFiles, QKeySequence(tr("Ctrl+Shift+O")));
#else
	fileMenu->addAction(tr("&Open..."), QKeySequence(tr("Ctrl+Shift+O")), this, &MainTrollWindow::OpenConfigAndUiFiles);
#endif

	if (!hasTouchInput)
		fileMenu->addSeparator();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	fileMenu->addAction(tr("&Refresh"), this, &MainTrollWindow::Refresh, QKeySequence(tr("F5")));
#else
	fileMenu->addAction(tr("&Refresh"), QKeySequence(tr("F5")), this, &MainTrollWindow::Refresh);
#endif
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	fileMenu->addAction(tr("Reconnect to &monome device"), this, &MainTrollWindow::Reconnect, QKeySequence(tr("Ctrl+R")));
#else
	fileMenu->addAction(tr("Reconnect to &monome device"), QKeySequence(tr("Ctrl+R")), this, &MainTrollWindow::Reconnect);
#endif

	{
		decltype(&MainTrollWindow::LoadConfigMru1) configMruMembers[kMruCount] =
		{
			&MainTrollWindow::LoadConfigMru1,
			&MainTrollWindow::LoadConfigMru2,
			&MainTrollWindow::LoadConfigMru3,
			&MainTrollWindow::LoadConfigMru4
		};

		if (!hasTouchInput)
			fileMenu->addSeparator();

		for (int idx = 0; idx < kMruCount; ++idx)
		{
			mMruActions[idx] = fileMenu->addAction("", this, configMruMembers[idx]);
			mMruActions[idx]->setEnabled(false);
			mMruActions[idx]->setVisible(false);
		}
	}

	if (!hasTouchInput)
		fileMenu->addSeparator();
	fileMenu->addAction(tr("E&xit"), this, &MainTrollWindow::close);
	// http://doc.qt.nokia.com/4.4/stylesheet-examples.html#customizing-qmenu

	QMenu * viewMenu = menuBar()->addMenu(tr("&View"));
#if defined(Q_OS_WIN)
	::UnregisterTouchWindow((HWND)viewMenu->winId());
#endif
	if (hasTouchInput)
		viewMenu->setStyleSheet(touchMenuStyle);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	viewMenu->addAction(tr("&Toggle trace window visibility"), this, &MainTrollWindow::ToggleTraceWindow, QKeySequence(tr("Ctrl+T")));
#else
	viewMenu->addAction(tr("&Toggle trace window visibility"), QKeySequence(tr("Ctrl+T")), this, &MainTrollWindow::ToggleTraceWindow);
#endif
	if (!hasTouchInput)
		fileMenu->addSeparator();

	mIncreaseMainDisplayHeight = new QAction(tr("&Increase main display height"), this);
	mIncreaseMainDisplayHeight->setShortcut(QKeySequence(tr("Ctrl+=")));
	connect(mIncreaseMainDisplayHeight, &QAction::triggered, this, &MainTrollWindow::IncreaseMainDisplayHeight);
	viewMenu->addAction(mIncreaseMainDisplayHeight);

	mDecreaseMainDisplayHeight = new QAction(tr("&Decrease main display height"), this);
	mDecreaseMainDisplayHeight->setShortcut(QKeySequence(tr("Ctrl+-")));
	connect(mDecreaseMainDisplayHeight, &QAction::triggered, this, &MainTrollWindow::DecreaseMainDisplayHeight);
	viewMenu->addAction(mDecreaseMainDisplayHeight);

	QMenu * settingsMenu = menuBar()->addMenu(tr("&Settings"));
#if defined(Q_OS_WIN)
	::UnregisterTouchWindow((HWND)settingsMenu->winId());
#endif
	if (hasTouchInput)
		settingsMenu->setStyleSheet(touchMenuStyle);

	mToggleExpressionPedalDetailStatus = new QAction(tr("&Expression pedal detailed status"), this);
	mToggleExpressionPedalDetailStatus->setCheckable(true);
	mToggleExpressionPedalDetailStatus->setChecked(false);
	mToggleExpressionPedalDetailStatus->setShortcut(QKeySequence(tr("Ctrl+P")));
	connect(mToggleExpressionPedalDetailStatus, &QAction::toggled, this, &MainTrollWindow::ToggleExpressionPedalDetails);
	settingsMenu->addAction(mToggleExpressionPedalDetailStatus);

	mMidiSuspendAction = new QAction(tr("Suspend &MIDI connections"), this);
	mMidiSuspendAction->setCheckable(true);
	mMidiSuspendAction->setChecked(false);
	mMidiSuspendAction->setShortcut(QKeySequence(tr("Ctrl+M")));
	connect(mMidiSuspendAction, &QAction::toggled, this, &MainTrollWindow::SuspendMidiToggle);
	settingsMenu->addAction(mMidiSuspendAction);

	QMenu * adcMenu = menuBar()->addMenu(tr("&Pedal Overrides"));
#if defined(Q_OS_WIN)
	::UnregisterTouchWindow((HWND)adcMenu->winId());
#endif
	if (hasTouchInput)
		adcMenu->setStyleSheet(touchMenuStyle);

	decltype(&MainTrollWindow::ToggleAdc0Override) memberSlots[ExpressionPedals::PedalCount]
	{
		&MainTrollWindow::ToggleAdc0Override,
		&MainTrollWindow::ToggleAdc1Override,
		&MainTrollWindow::ToggleAdc2Override,
		&MainTrollWindow::ToggleAdc3Override
	};

	for (int idx = 0; idx < ExpressionPedals::PedalCount; ++idx)
	{
		mAdcForceDisable[idx] = settings.value(kAdcOverride.arg(idx), false).toBool();
		mAdcOverrideActions[idx] = new QAction(tr("Disable ADC &%1").arg(idx), this);
		mAdcOverrideActions[idx]->setCheckable(true);
		mAdcOverrideActions[idx]->setChecked(mAdcForceDisable[idx]);
		connect(mAdcOverrideActions[idx], &QAction::toggled, this, memberSlots[idx]);
		adcMenu->addAction(mAdcOverrideActions[idx]);
	}

	QMenu * helpMenu = menuBar()->addMenu(tr("&Help"));
#if defined(Q_OS_WIN)
	::UnregisterTouchWindow((HWND)helpMenu->winId());
#endif
	if (hasTouchInput)
		helpMenu->setStyleSheet(touchMenuStyle);
	helpMenu->addAction(tr("&About mTroll..."), this, &MainTrollWindow::About);

	mUiFilename = settings.value(kActiveUiFile, "config/autoGrid.ui.xml").value<QString>();
	mConfigFilename = settings.value(kConfigMru + QChar('1'), "config/axefx3v2.config.xml").value<QString>();

	restoreGeometry(settings.value(kMainWindowGeom).toByteArray());

	RegisterDevicesNotification();

	UpdateMru();
	Refresh();
}

MainTrollWindow::~MainTrollWindow()
{
	RegisterDevicesNotification(false);

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
	AboutDlg dlg(this);
	dlg.exec();
}

void
MainTrollWindow::OpenFile(bool config, bool ui)
{
	if (config)
	{
		const QString cfgFleSelection = QFileDialog::getOpenFileName(this,
			tr("Select Config Settings File"),
			mConfigFilename,
			tr("Config files (*.config.xml)"));
		if (cfgFleSelection.isEmpty())
			return;

		mConfigFilename = cfgFleSelection;
		UpdateMru();
	}

	if (ui)
	{
		const QString uiFileSelection = QFileDialog::getOpenFileName(this,
			tr("Select UI Settings File"),
			mUiFilename,
			tr("UI files (*.ui.xml)"));
		if (uiFileSelection.isEmpty())
			return;

		mUiFilename = uiFileSelection;
		QSettings settings;
		settings.setValue(kActiveUiFile, mUiFilename);
	}

	Refresh();
}

void
MainTrollWindow::UpdateMru()
{
	QSettings settings;

	// build new mru data
	std::vector<QString> mruFiles;
	if (QFileInfo::exists(mConfigFilename))
		mruFiles.push_back(mConfigFilename);
	for (int idx = 1; idx <= kMruCount; ++idx)
	{
		QString curVal(kConfigMru);
		curVal.append(QChar(0x30 + idx));

		QString mruItem = settings.value(curVal, "").value<QString>();
		if (mruItem.isEmpty() || mruItem == mConfigFilename)
			continue;

		bool isDupe = false;
		for (const auto & cur : mruFiles)
		{
			if (mruItem == cur)
			{
				isDupe = true;
				break;
			}
		}

		if (isDupe)
			continue;

		if (QFileInfo::exists(mruItem))
			mruFiles.push_back(mruItem);
		if (mruFiles.size() == kMruCount)
			break;
	}

	// update file menu mru actions
	int idx = 1;
	for (const QString curFile : mruFiles)
	{
		QString curMruName(kConfigMru);
		curMruName.append(QChar(0x30 + idx));
		settings.setValue(curMruName, curFile);

		QString actionTxt("&");
		actionTxt.append(QChar(0x30 + idx));
		actionTxt.append(" ");
		{
			QFileInfo fi(curFile);
			actionTxt.append(fi.fileName());
		}
		_ASSERTE((idx - 1) < kMruCount);
		mMruActions[idx - 1]->setText(actionTxt);
		mMruActions[idx - 1]->setVisible(true);
		mMruActions[idx - 1]->setEnabled(true);
		++idx;
	}

	// ensure any now unused actions (that might have been visible before), are now hidden
	for (; idx <= kMruCount; ++idx)
	{
		mMruActions[idx - 1]->setVisible(false);
		mMruActions[idx - 1]->setEnabled(false);
	}
}

void
MainTrollWindow::Refresh()
{
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	if (mUi)
		mUi->Unload();
	mUi = new ControlUi(this, this);

	const std::string uiFile(mUiFilename.toUtf8());
	const std::string cfgFile(mConfigFilename.toUtf8());
	mUi->Load(uiFile, cfgFile, mAdcForceDisable);

	if (mUi->HasAutoGrid())
	{
		// create a scroll area so that if defined controls exceed space of main 
		// window, they remain accessible via scrollbars (though it doesn't work 
		// without QGridLayout (?) )
		QScrollArea *scrollArea = new QScrollArea(this);
		extern QString sHorizontalScrollStyle;
		extern QString sVerticalScrollStyle;
		scrollArea->horizontalScrollBar()->setStyleSheet(sHorizontalScrollStyle);
		scrollArea->verticalScrollBar()->setStyleSheet(sVerticalScrollStyle);
		scrollArea->setFrameShape(QFrame::NoFrame);
		scrollArea->setWidgetResizable(true);
		scrollArea->setWidget(mUi);

		QPalette pal;
		pal.setColor(QPalette::Base, mUi->GetBackGroundColor());
		scrollArea->setPalette(pal);
		setCentralWidget(scrollArea);	// Qt deletes the previous central widget

		mIncreaseMainDisplayHeight->setEnabled(true);
		mDecreaseMainDisplayHeight->setEnabled(true);
	}
	else
	{
		setCentralWidget(mUi);	// Qt deletes the previous central widget

		mIncreaseMainDisplayHeight->setEnabled(false);
		mDecreaseMainDisplayHeight->setEnabled(false);
	}

	int width, height;
	mUi->GetPreferredSize(width, height);
	if (width && height && !isMaximized())
		resize(width, height);

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

bool
MainTrollWindow::IsHardwareAdcEnabled(int idx) const
{
	if (!mUi)
		return false;
	return mUi->IsHardwareAdcEnabled(idx);
}

void
MainTrollWindow::EnableHardwareAdc(int idx, bool enable) const
{
	if (mUi)
		mUi->EnableHardwareAdc(idx, enable);
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

void
MainTrollWindow::LoadConfigMruItem(int idx)
{
	QString curVal(kConfigMru);
	curVal.append(QChar(0x30 + idx));

	QSettings settings;
	QString mruItem = settings.value(curVal, "").value<QString>();
	if (mruItem.isEmpty())
		return;
		
	mConfigFilename = mruItem;
	UpdateMru();
	Refresh();
}

std::string
MainTrollWindow::GetElapsedTimeStr()
{
	const int kSecsInMinute = 60;
	const int kSecsInHour = kSecsInMinute * 60;
	const int kSecsInDay = kSecsInHour * 24;
	QDateTime now(QDateTime::currentDateTime());
	QString ts(now.toString("h:mm:ss ap \nddd, MMM d, yyyy"));
	if (mPauseTime != mStartTime)
		now = mPauseTime;

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
		tmp = QString::asprintf("\nelapsed: %d days, %d:%02d:%02d", days, hours, minutes, secs);
	else if (hours)
		tmp = QString::asprintf("\nelapsed: %d hours, %02d:%02d", hours, minutes, secs);
	else
		tmp = QString::asprintf("\nelapsed: %02d:%02d", minutes, secs);
	ts += tmp;

	const std::string msg(ts.toUtf8());
	return msg;
}

void
MainTrollWindow::ResetTime()
{
	mStartTime = mPauseTime = QDateTime::currentDateTime();
}

void
MainTrollWindow::PauseOrResumeTime()
{
	if (mPauseTime == mStartTime)
	{
		// pause
		mPauseTime = QDateTime::currentDateTime();
	}
	else
	{
		// resume
		// calculate amount of time paused
		QDateTime now = QDateTime::currentDateTime();
		qint64 diff = mPauseTime.secsTo(now);

		// increase mStartTime by the duration of the pause
		mStartTime = mStartTime.addSecs(diff);

		// resume by setting pause time to (new) start time
		mPauseTime = mStartTime;
	}
}

void
MainTrollWindow::Exit(ExitAction action)
{
	mShutdownOnExit = action;

	// ExitEvent
	// --------------------------------------------------------------------
	// Because Exit can be called from the hardware monitoring thread,
	// handle via postEvent on ui thread.
	//
	class ExitEvent : public QEvent
	{
		ControlUi * mWnd;
	public:
		ExitEvent(ControlUi * wnd) :
			QEvent(User),
			mWnd(wnd)
		{
		}

		virtual void exec()
		{
			mWnd->ExitEventFired();
		}
	};

	if (mUi)
		QCoreApplication::postEvent(mUi, new ExitEvent(mUi));
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
MainTrollWindow::IncreaseMainDisplayHeight()
{
	if (mUi && mUi->HasAutoGrid())
		mUi->IncreaseMainDisplayHeight();
}

void
MainTrollWindow::DecreaseMainDisplayHeight()
{
	if (mUi && mUi->HasAutoGrid())
		mUi->DecreaseMainDisplayHeight();
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

void
MainTrollWindow::ToggleExpressionPedalDetails(bool checked)
{
	extern bool gEnableStatusDetails;
	gEnableStatusDetails = checked;
}

void
MainTrollWindow::Trace(const std::string & txt)
{
	if (mUi)
		mUi->Trace(txt);
}

void
MainTrollWindow::closeEvent(QCloseEvent *event)
{
	QSettings settings;
	settings.setValue(kMainWindowGeom, saveGeometry());
	QMainWindow::closeEvent(event);
}

#if defined(Q_OS_WIN)
// For MIDI interfaces, use a Device Interface Class GUID rather than the Device Setup Class GUID. 
// MIDI devices typically register under the Kernel Streaming(KS) audio 
// category and are internally handled as KS audio endpoints.
// MIDI interfaces, including USB MIDI controllers and external sound cards, register as audio interface instances.
// MIDI devices are supposed to register as:
//	KSCATEGORY_AUDIO
//	KSCATEGORY_RENDER
//	KSCATEGORY_CAPTURE
// other class guids:
// USB Devices (General) 				{ 0x36FC9E60, 0xC465, 0x11CF, 0x80, 0x56, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 }
// HID (Human Interface Devices)	 	{ 0x4D1E55B2, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 }
// Serial Ports 						{ 0x86E0D1E0, 0x8089, 0x11D0, 0x9C, 0xE4, 0x08, 0x00, 0x3E, 0x30, 0x1F, 0x73 }
// Media device setup class				{ 0x4d36e96c, 0xe325, 0x11ce, 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 }
// KSCATEGORY_PREFERRED_WAVEOUT_DEVICE	{ 0xD6C50674, 0x72C1, 0x11D2, 0x97, 0x55, 0x00, 0x00, 0xF8, 0x00, 0x47, 0x88 }
// KSCATEGORY_WDMAUD
// https://learn.microsoft.com/en-us/windows-hardware/drivers/install/system-defined-device-setup-classes-available-to-vendors
// https://learn.microsoft.com/en-us/windows-hardware/drivers/install/kscategory-preferred-midiout-device
// https://www.lifewire.com/device-class-guids-for-most-common-types-of-hardware-2619208

std::string
GetSetupDiDeviceName(const wchar_t *device_interface)
{
	QString qname;
	HDEVINFO devInfo = SetupDiCreateDeviceInfoList(NULL, NULL);
	if (devInfo && INVALID_HANDLE_VALUE != devInfo)
	{
		SP_DEVICE_INTERFACE_DATA interfaceData;
		ZeroMemory(&interfaceData, sizeof(interfaceData));
		interfaceData.cbSize = sizeof(interfaceData);
		if (SetupDiOpenDeviceInterfaceW(devInfo, device_interface, 0, &interfaceData))
		{
			SP_DEVINFO_DATA deviceData;
			ZeroMemory(&deviceData, sizeof(deviceData));
			deviceData.cbSize = sizeof(SP_DEVINFO_DATA);
			if (!SetupDiGetDeviceInterfaceDetailW(devInfo, &interfaceData, NULL, 0, NULL, &deviceData) &&
				GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				constexpr int kIdLen = 256;
				BYTE nameBytes[kIdLen * 2];
				ZeroMemory(nameBytes, kIdLen * 2);
				DEVPROPTYPE propType = 0;
				if (SetupDiGetDevicePropertyW(devInfo, &deviceData, &DEVPKEY_NAME, &propType, nameBytes, kIdLen, nullptr, 0))
				{
					if (DEVPROP_TYPE_STRING == propType)
						qname = QString::fromRawData((const char16_t*)nameBytes, wcslen((wchar_t*)nameBytes));
				}
			}

			SetupDiDeleteDeviceInterfaceData(devInfo, &interfaceData);
		}

		SetupDiDestroyDeviceInfoList(devInfo);
	}

	return qname.toStdString();
}
#endif

bool
MainTrollWindow::nativeEventFilter(const QByteArray &eventType, void *message,
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	qintptr *result
#else
	long *result
#endif
	)
{
	Q_UNUSED(eventType);
	Q_UNUSED(result);
#if defined(Q_OS_WIN)
	MSG *msg = (MSG *)message;
	if (WM_DEVICECHANGE == msg->message)
	{
		auto pDev = reinterpret_cast<PDEV_BROADCAST_HDR>(msg->lParam);
		switch (msg->wParam)
		{
		case DBT_DEVICEARRIVAL:
			if (pDev && DBT_DEVTYP_DEVICEINTERFACE == pDev->dbch_devicetype)
			{
				auto pInter = reinterpret_cast<const PDEV_BROADCAST_DEVICEINTERFACE>(pDev);
				if (KSCATEGORY_AUDIO == pInter->dbcc_classguid)
				{
					std::string name(::GetSetupDiDeviceName(reinterpret_cast<wchar_t*>(&pInter->dbcc_name[0])));
					if (name.empty())
					{
						const QString devName(reinterpret_cast<wchar_t*>(&pInter->dbcc_name[0]));
						name = devName.toStdString();
					}
					Trace(std::format("Audio/MIDI device attached: {}\n", name));
				}
			}
			break;
		case DBT_DEVICEREMOVECOMPLETE:
			if (pDev && DBT_DEVTYP_DEVICEINTERFACE == pDev->dbch_devicetype)
			{
				auto pInter = reinterpret_cast<const PDEV_BROADCAST_DEVICEINTERFACE>(pDev);
				if (KSCATEGORY_AUDIO == pInter->dbcc_classguid)
				{
					std::string name(::GetSetupDiDeviceName(reinterpret_cast<wchar_t*>(&pInter->dbcc_name[0])));
					if (name.empty())
					{
						const QString devName(reinterpret_cast<wchar_t*>(&pInter->dbcc_name[0]));
						name = devName.toStdString();
					}
					Trace(std::format("Audio/MIDI device detached: {}\n", name));
				}
			}
			break;
		case DBT_DEVNODES_CHANGED:
			break;
		default:
#ifdef _DEBUG
			Trace(std::format("Unhandled device notification: {}\n", msg->wParam))
#endif
			;
		}
	}
#endif

	return false;
}

void
MainTrollWindow::RegisterDevicesNotification(bool registerDevNotification /*= true*/) noexcept
{
	if (registerDevNotification)
	{
#if defined(Q_OS_WIN)
		_ASSERTE(!mDevNotify);
		DEV_BROADCAST_DEVICEINTERFACE notificationFilter;
		ZeroMemory(&notificationFilter, sizeof(notificationFilter));
		notificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
		notificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
		mDevNotify = ::RegisterDeviceNotification((HANDLE)winId(), &notificationFilter,
			DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
		if (nullptr == mDevNotify)
			Trace("ERROR: failed to register audio/midi device notification\n");
#endif
	}
	else
	{
#if defined(Q_OS_WIN)
		if (mDevNotify)
		{
			::UnregisterDeviceNotification(mDevNotify);
			mDevNotify = nullptr;
		}
#endif
	}
}
