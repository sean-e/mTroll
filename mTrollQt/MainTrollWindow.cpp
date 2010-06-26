/*
 * mTroll MIDI Controller
 * Copyright (C) 2007-2010 Sean Echevarria
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


#define kOrganizationKey	QString("Fester")
#define kOrganizationDomain	QString("creepingfog.com")
#define kAppKey				QString("mTroll")
#define kActiveUiFile		QString("UiFile")
#define kActiveConfigFile	QString("ConfigFile")
#define kAdcOverride		QString("AdcOverride%1")


MainTrollWindow::MainTrollWindow() : 
	QMainWindow(),
	mUi(NULL)
{
	QCoreApplication::setOrganizationName(kOrganizationKey);
	QCoreApplication::setOrganizationDomain(kOrganizationDomain);
	QCoreApplication::setApplicationName(kAppKey);

	setWindowTitle(tr("mTroll MIDI Controller"));
	QSettings settings;

	QMenu * fileMenu = menuBar()->addMenu(tr("&File"));
	fileMenu->addAction(tr("&Open..."), this, SLOT(OpenFile()), QKeySequence(tr("Ctrl+O")));
	fileMenu->addAction(tr("&Refresh"), this, SLOT(Refresh()), QKeySequence(tr("F5")));
	fileMenu->addAction(tr("Re&connect to monome device"), this, SLOT(Reconnect()), QKeySequence(tr("Ctrl+R")));
	fileMenu->addAction(tr("&Toggle trace window visibility"), this, SLOT(ToggleTraceWindow()), QKeySequence(tr("Ctrl+T")));
	fileMenu->addSeparator();
	fileMenu->addAction(tr("E&xit"), this, SLOT(close()));

	QString slotName;
	QMenu * adcMenu = menuBar()->addMenu(tr("&Pedal Overrides"));
	for (int idx = 0; idx < ExpressionPedals::PedalCount; ++idx)
	{
		mAdcForceDisable[idx] = settings.value(kAdcOverride.arg(idx), false).toBool();
		mAdcOverrideActions[idx] = new QAction(tr("Disable ADC &%1").arg(idx), this);
		mAdcOverrideActions[idx]->setCheckable(true);
		mAdcOverrideActions[idx]->setChecked(mAdcForceDisable[idx]);
		slotName = QString("1ToggleAdc%1Override(bool)").arg(idx);
		connect(mAdcOverrideActions[idx], SIGNAL(toggled(bool)), this, slotName.toAscii().constData());
		adcMenu->addAction(mAdcOverrideActions[idx]);
	}

	QMenu * helpMenu = menuBar()->addMenu(tr("&Help"));
	helpMenu->addAction(tr("&About mTroll..."), this, SLOT(About()));

	mUiFilename = settings.value(kActiveUiFile, "testdata.ui.xml").value<QString>();
	mConfigFilename = settings.value(kActiveConfigFile, "testdata.config.xml").value<QString>();

	Refresh();
}

MainTrollWindow::~MainTrollWindow()
{
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
	mUi = new ControlUi(this, this);
	setCentralWidget(mUi);	// Qt deletes the previous central widget

	QByteArray tmp(mUiFilename.toAscii());
	const std::string uiFile(tmp.constData(), tmp.count());
	tmp = mConfigFilename.toAscii();
	const std::string cfgFile(tmp.constData(), tmp.count());
	mUi->Load(uiFile, cfgFile, mAdcForceDisable);

	int width, height;
	mUi->GetPreferredSize(width, height);
	if (width && height)
	{
		if (isMaximized())
		{
#ifdef _WINDOWS
			// don't resize if maximized, but do change the size that
			// will be used when it becomes unmaximized.
			// Is there a way to do this with Qt?
			WINDOWPLACEMENT wp;
			::ZeroMemory(&wp, sizeof(WINDOWPLACEMENT));
			::GetWindowPlacement(winId(), &wp);
			wp.rcNormalPosition.right = wp.rcNormalPosition.left + width;
			wp.rcNormalPosition.bottom = wp.rcNormalPosition.top + height;
			::SetWindowPlacement(winId(), &wp);
#endif
		}
		else
			resize(width, height);
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
