#include "MainTrollWindow.h"
#include <QMenu>
#include <QMenuBar>
#include <qsettings.h>
#include <QApplication>
#include <qcoreapplication.h>
#include <QFileDialog>
#include "AboutDlg.h"


#define kOrganizationKey	QString("Fester")
#define kOrganizationDomain	QString("creepingfog.com")
#define kAppKey				QString("mTroll")
#define kActiveUiFile		QString("UiFile")
#define kActiveConfigFile	QString("ConfigFile")


MainTrollWindow::MainTrollWindow() : QMainWindow()
{
	QCoreApplication::setOrganizationName(kOrganizationKey);
	QCoreApplication::setOrganizationDomain(kOrganizationDomain);
	QCoreApplication::setApplicationName(kAppKey);

	setWindowTitle(tr("mTroll MIDI Controller"));

	mFileMenu = menuBar()->addMenu(tr("&File"));
	mFileMenu->addAction(tr("&Open..."), this, SLOT(OpenFile()), QKeySequence(tr("Ctrl+O")));
	mFileMenu->addAction(tr("&Refresh"), this, SLOT(Refresh()), QKeySequence(tr("F5")));
	mFileMenu->addSeparator();
	mFileMenu->addAction(tr("E&xit"), this, SLOT(close()));

	mHelpMenu = menuBar()->addMenu(tr("&Help"));
	mHelpMenu->addAction(tr("&About mTroll..."), this, SLOT(About()));

	QSettings settings;
	mUiFilename = settings.value(kActiveUiFile, "testdata.ui.xml").value<QString>();
	mConfigFilename = settings.value(kActiveConfigFile, "testdata.config.xml").value<QString>();

//	m_hWndClient = mView.Create(m_hWnd);

	Refresh();
}

MainTrollWindow::~MainTrollWindow()
{
//	mView.Unload();

	delete mFileMenu;
	delete mHelpMenu;
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
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
// 	mView.Unload();
	QApplication::restoreOverrideCursor();

	QString fileSelection;
	fileSelection = QFileDialog::getOpenFileName(this, 
			tr("Select Config Settings File"),
			mConfigFilename,
			tr("Config files (*.config.xml)"));
	if (fileSelection.isEmpty())
		return;
	mConfigFilename = fileSelection;

	fileSelection = QFileDialog::getOpenFileName(this, 
			tr("Select UI Settings File"),
			mUiFilename,
			tr("UI files (*.ui.xml)"));
	if (fileSelection.isEmpty())
		return;
	mUiFilename = fileSelection;

	QSettings settings;
	settings.setValue(kActiveUiFile, mUiFilename);
	settings.setValue(kActiveConfigFile, mConfigFilename);

	Refresh();
}

void
MainTrollWindow::Refresh()
{
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
// 	mView.Unload();
// 	mView.Load(mUiFilename, mConfigFilename);

	int width, height;
// 	mView.GetPreferredSize(width, height);
	if (width && height)
	{
// 		WINDOWPLACEMENT wp;
// 		ZeroMemory(&wp, sizeof(WINDOWPLACEMENT));
// 		GetWindowPlacement(&wp);

		if (isMaximized() || isMinimized())
		{
// 			wp.rcNormalPosition.right = wp.rcNormalPosition.left + width;
// 			wp.rcNormalPosition.bottom = wp.rcNormalPosition.top + height;
// 			SetWindowPlacement(&wp);
		}
		else
		{
// 			CRect wndRc;
// 			GetWindowRect(&wndRc);
// 			wndRc.right = wndRc.left + width;
// 			wndRc.bottom = wndRc.top + height;
// 			MoveWindow(&wndRc);
		}
	}

	QApplication::restoreOverrideCursor();
}
