#include "MainTrollWindow.h"
#include <QMenu>
#include <QMenuBar>
#include <qsettings.h>
#include <QApplication>
#include <qcoreapplication.h>
#include <QFileDialog>
#include "AboutDlg.h"
#include "ControlUi.h"


#define kOrganizationKey	QString("Fester")
#define kOrganizationDomain	QString("creepingfog.com")
#define kAppKey				QString("mTroll")
#define kActiveUiFile		QString("UiFile")
#define kActiveConfigFile	QString("ConfigFile")


MainTrollWindow::MainTrollWindow() : 
	QMainWindow()
{
	QCoreApplication::setOrganizationName(kOrganizationKey);
	QCoreApplication::setOrganizationDomain(kOrganizationDomain);
	QCoreApplication::setApplicationName(kAppKey);

	setWindowTitle(tr("mTroll MIDI Controller"));

	QMenu * fileMenu = menuBar()->addMenu(tr("&File"));
	fileMenu->addAction(tr("&Open..."), this, SLOT(OpenFile()), QKeySequence(tr("Ctrl+O")));
	fileMenu->addAction(tr("&Refresh"), this, SLOT(Refresh()), QKeySequence(tr("F5")));
	fileMenu->addSeparator();
	fileMenu->addAction(tr("E&xit"), this, SLOT(close()));

	QMenu * helpMenu = menuBar()->addMenu(tr("&Help"));
	helpMenu->addAction(tr("&About mTroll..."), this, SLOT(About()));

	QSettings settings;
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
	ControlUi * newUi = new ControlUi(this);
	setCentralWidget(newUi);	// Qt deletes the previous central widget

	QByteArray tmp(mUiFilename.toAscii());
	const std::string uiFile(tmp.constData(), tmp.count());
	tmp = mConfigFilename.toAscii();
	const std::string cfgFile(tmp.constData(), tmp.count());
	newUi->Load(uiFile, cfgFile);

	int width, height;
	newUi->GetPreferredSize(width, height);
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
			::GetWindowPlacement(effectiveWinId(), &wp);
			wp.rcNormalPosition.right = wp.rcNormalPosition.left + width;
			wp.rcNormalPosition.bottom = wp.rcNormalPosition.top + height;
			::SetWindowPlacement(effectiveWinId(), &wp);
#endif
		}
		else
			resize(width, height);
	}

	QApplication::restoreOverrideCursor();
}
