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

#include <algorithm>
#include <strstream>

#include <QApplication>
#include <qthread.h>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QtEvents>

#include "ControlUi.h"
#include "../Engine/EngineLoader.h"
#include "../Engine/MidiControlEngine.h"
#include "../Engine/UiLoader.h"
#include "../Monome40h/IMonome40h.h"
#include "../Monome40h/qt/Monome40hFtqt.h"

#ifdef _WINDOWS
	#include "../winUtil/SEHexception.h"
	#include "../midi/WinMidiOut.h"
	typedef WinMidiOut	XMidiOut;
	#define SLEEP	Sleep
#else
	#error "include the midiOut header file for this platform"
	typedef YourMidiOut		XMidiOut;
	#define SLEEP	sleep
#endif


ControlUi::ControlUi(QWidget * parent) :
	QWidget(parent),
	mEngine(NULL),
	mMainDisplay(NULL),
	mTraceDisplay(NULL),
	mPreferredHeight(0),
	mPreferredWidth(0),
	mMaxSwitchId(0),
	mHardwareUi(NULL),
	mLedIntensity(0),
	mInvertLeds(false),
	mSystemPowerOverride(NULL)
{
}

ControlUi::~ControlUi()
{
	Unload();
}

struct DeleteMidiOut
{
	void operator()(const std::pair<unsigned int, IMidiOut *> & pr)
	{
		delete pr.second;
	}
};

struct DeleteSwitchLed
{
	void operator()(const std::pair<int, ControlUi::SwitchLed *> & pr)
	{
		delete pr.second;
	}
};

struct DeleteSwitch
{
	void operator()(const std::pair<int, ControlUi::Switch *> & pr)
	{
		delete pr.second;
	}
};

struct DeleteSwitchTextDisplay
{
	void operator()(const std::pair<int, ControlUi::SwitchTextDisplay *> & pr)
	{
		delete pr.second;
	}
};

class ControlUiEvent : public QEvent
{
public:
	ControlUiEvent(QEvent::Type t) : QEvent(t) {}
	virtual ~ControlUiEvent() {}

	virtual void exec() = 0;
};

void
ControlUi::Unload()
{
	if (mHardwareUi)
	{
		mHardwareUi->Unsubscribe(mEngine);
		mHardwareUi->Unsubscribe(this);
	}

	CloseMidiOuts();

	delete mEngine;
	mEngine = NULL;

	// clear leds
	if (mHardwareUi)
	{
		for (std::map<int, bool>::iterator it = mStupidSwitchStates.begin();
			it != mStupidSwitchStates.end();
			++it)
		{
			SetSwitchDisplay((*it).first, false);
		}
	}

	QCoreApplication::removePostedEvents(this, QEvent::User);
	mStupidSwitchStates.clear();

	delete mHardwareUi;
	mHardwareUi = NULL;

	delete mMainDisplay;
	mMainDisplay = NULL;

	delete mTraceDisplay;
	mTraceDisplay = NULL;

	mMaxSwitchId = 0;

	QFont tmp;
	mTraceFont = mSwitchButtonFont = tmp;

	for_each(mMidiOuts.begin(), mMidiOuts.end(), DeleteMidiOut());
	mMidiOuts.clear();

	for_each(mLeds.begin(), mLeds.end(), DeleteSwitchLed());
	mLeds.clear();

	for_each(mSwitches.begin(), mSwitches.end(), DeleteSwitch());
	mSwitches.clear();

	for_each(mSwitchTextDisplays.begin(), mSwitchTextDisplays.end(), DeleteSwitchTextDisplay());
	mSwitchTextDisplays.clear();

	mSwitchNumberToRowCol.clear();
	mRowColToSwitchNumber.clear();

	delete mSystemPowerOverride;
	mSystemPowerOverride = NULL;
}

void
ControlUi::Load(const std::string & uiSettingsFile, 
				const std::string & configSettingsFile,
				const bool adcOverrides[ExpressionPedals::PedalCount])
{
	Unload();

	for (int idx = 0; idx < ExpressionPedals::PedalCount; ++idx)
		mUserAdcSettings[idx] = false;

	LoadUi(uiSettingsFile);
	LoadMonome(true);
	LoadMidiSettings(configSettingsFile, adcOverrides);

	if (mHardwareUi)
	{
		mHardwareUi->Subscribe(this);
		mHardwareUi->Subscribe(mEngine);

		bool anyMidiOutOpen = false;
		for (MidiOuts::iterator it = mMidiOuts.begin();
			it != mMidiOuts.end();
			++it)
		{
			IMidiOut * curOut = (*it).second;
			if (curOut->IsMidiOutOpen())
			{
				anyMidiOutOpen = true;
				break;
			}
		}

		if (anyMidiOutOpen)
			mSystemPowerOverride = new KeepDisplayOn;
	}

	for (int idx = 0; mInvertLeds && idx < 64; ++idx)
		SetSwitchDisplay(idx, false);
}

void
ControlUi::LoadUi(const std::string & uiSettingsFile)
{
	UiLoader ldr(this, uiSettingsFile);

	Trace("Midi Devices:\n");
	IMidiOut * midiOut = CreateMidiOut(0, -1);
	if (midiOut)
	{
		const int kMidiOutCnt = midiOut->GetMidiOutDeviceCount();
		for (int idx = 0; idx < kMidiOutCnt; ++idx)
		{
			std::strstream msg;
			msg << "  " << idx << ": " << midiOut->GetMidiOutDeviceName(idx) << std::endl << std::ends;
			Trace(msg.str());
		}
		Trace("\n");
		midiOut->CloseMidiOut();
		mMidiOuts.erase(0);
	}

	if (mSwitches[0])
		mSwitches[0]->setFocus();
}

void
ControlUi::LoadMonome(bool displayStartSequence)
{
	IMonome40h * monome = NULL;
	try
	{
		monome = new Monome40hFtqt(this);
		int devIdx = monome->LocateMonomeDeviceIdx();
		if (-1 != devIdx)
		{
			const std::string devSerial(monome->GetDeviceSerialNumber(devIdx));
			if (!devSerial.empty())
			{
				if (monome->AcquireDevice(devSerial))
				{
					mHardwareUi = monome;
					monome = NULL;
					if (displayStartSequence)
						MonomeStartupSequence();
					mHardwareUi->SetLedIntensity(mLedIntensity);
				}
			}
		}
	}
	catch (const std::string & e)
	{
		Trace(e);
	}
#ifdef _WINDOWS
	catch (const SEHexception &)
	{
		Trace("ERROR: failed to load monome\n");
	}
#endif // _WINDOWS

	delete monome;
}

void
ControlUi::LoadMidiSettings(const std::string & file, 
							const bool adcOverrides[ExpressionPedals::PedalCount])
{
	delete mEngine;
	EngineLoader ldr(this, this, this, this);
	mEngine = ldr.CreateEngine(file);
	if (mEngine)
	{
		if (mHardwareUi)
			ldr.InitMonome(mHardwareUi, adcOverrides, mUserAdcSettings);
	}
	else
		TextOut("Failed to load MIDI settings.");
}

void
ControlUi::ButtonPressed(const int idx)
{
	if (!mStupidSwitchStates[idx])
	{
		byte row, col;
		RowColFromSwitchNumber(idx, row, col);
		SwitchPressed(row, col);
	}
}

void
ControlUi::ButtonReleased(const int idx)
{
	if (mStupidSwitchStates[idx])
	{
		byte row, col;
		RowColFromSwitchNumber(idx, row, col);
		SwitchReleased(row, col);
	}
}

class LabelTextOutEvent2 : public ControlUiEvent
{
	QPlainTextEdit *mLabel;
	QString mText;

public:
	LabelTextOutEvent2(QPlainTextEdit* label, const QString & text) : 
		ControlUiEvent(User),
		mText(text),
		mLabel(label)
	{
	}

	virtual void exec()
	{
		const QString prevTxt(mLabel->toPlainText());
		if (prevTxt != mText)
			mLabel->setPlainText(mText);
	}
};

class LabelTextOutEvent : public ControlUiEvent
{
	QLabel *mLabel;
	QString mText;

public:
	LabelTextOutEvent(QLabel * label, const QString & text) : 
		ControlUiEvent(User),
		mText(text),
		mLabel(label)
	{
	}

	virtual void exec()
	{
		const QString prevTxt(mLabel->text());
		if (prevTxt != mText)
			mLabel->setText(mText);
	}
};

// IMainDisplay
void
ControlUi::TextOut(const std::string & txt)
{
	if (!mMainDisplay)
		return;

	QCoreApplication::postEvent(this, 
		new LabelTextOutEvent2(mMainDisplay, txt.c_str()));
}

void
ControlUi::ClearDisplay()
{
	if (!mMainDisplay)
		return;

	QCoreApplication::postEvent(this, 
		new LabelTextOutEvent2(mMainDisplay, ""));
}


// ITraceDisplay
void
ControlUi::Trace(const std::string & txt)
{
	class TextEditAppend : public ControlUiEvent
	{
		QPlainTextEdit *mTextEdit;
		QString mText;

	public:
		TextEditAppend(QPlainTextEdit * txtEdit, const QString & text) : 
			ControlUiEvent(User),
			mText(text),
			mTextEdit(txtEdit)
		{
		}

		virtual void exec()
		{
			mTextEdit->moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
			mTextEdit->insertPlainText(mText);
		}
	};

	if (mTraceDisplay)
	{
		QCoreApplication::postEvent(this, 
			new TextEditAppend(mTraceDisplay, txt.c_str()));
	}
}


// ISwitchDisplay
void
ControlUi::SetSwitchDisplay(int switchNumber, 
							bool isOn)
{
	if (mInvertLeds)
		isOn = !isOn;

	if (mHardwareUi)
	{
		byte row, col;
		RowColFromSwitchNumber(switchNumber, row, col);
		mHardwareUi->EnableLed(row, col, isOn);
	}

	if (!mLeds[switchNumber] || !mLeds[switchNumber]->isEnabled())
		return;


	class UpdateSwitchDisplayEvent : public ControlUiEvent
	{
		ControlUi::SwitchLed * mLed;
		DWORD mColor;

	public:
		UpdateSwitchDisplayEvent(ControlUi::SwitchLed * led, DWORD color) : 
			ControlUiEvent(User),
			mLed(led),
			mColor(color)
		{
		}

		virtual void exec()
		{
			QPalette pal;
			pal.setColor(QPalette::Window, mColor);
			mLed->setPalette(pal);
		}
	};

	QCoreApplication::postEvent(this, 
		new UpdateSwitchDisplayEvent(mLeds[switchNumber], isOn ? mLedConfig.mOnColor : mLedConfig.mOffColor));
}

void
ControlUi::SetSwitchText(int switchNumber, 
						 const std::string & txt)
{
	if (!mSwitchTextDisplays[switchNumber] || !mSwitchTextDisplays[switchNumber]->isEnabled())
		return;

	QCoreApplication::postEvent(this, 
		new LabelTextOutEvent(mSwitchTextDisplays[switchNumber], txt.c_str()));
}

void
ControlUi::ClearSwitchText(int switchNumber)
{
	SetSwitchText(switchNumber, std::string(""));
}


// IMidiControlUi
void
ControlUi::AddSwitchMapping(int switchNumber, 
							int row, 
							int col)
{
	int rc = (row << 16) | col;
	_ASSERTE(!mRowColToSwitchNumber[rc]);
	mRowColToSwitchNumber[rc] = switchNumber;
	_ASSERTE(!mSwitchNumberToRowCol[switchNumber]);
	mSwitchNumberToRowCol[switchNumber] = rc;
}

void
ControlUi::SetSwitchLedConfig(int width, 
							  int height, 
							  unsigned int onColor, 
							  unsigned int offColor)
{
	mLedConfig.mWidth = width;
	mLedConfig.mHeight = height;
	mLedConfig.mOnColor = onColor;
	mLedConfig.mOffColor = offColor;
}

void
ControlUi::CreateSwitchLed(int id, 
						   int top, 
						   int left)
{
	SwitchLed * curSwitchLed = new SwitchLed(this);
//	curSwitchLed->SetShape(ID_SHAPE_SQUARE);
//	curSwitchLed->SetColor(mLedConfig.mOnColor, mLedConfig.mOffColor);

	curSwitchLed->setFrameShape(QFrame::Panel);
	curSwitchLed->setFrameShadow(QFrame::Sunken);
	curSwitchLed->setAutoFillBackground(true);
	curSwitchLed->move(left, top);
	curSwitchLed->resize(mLedConfig.mWidth, mLedConfig.mHeight);

	QPalette pal;
	pal.setColor(QPalette::Window, mLedConfig.mOffColor);
	curSwitchLed->setPalette(pal);

	_ASSERTE(!mLeds[id]);
	mLeds[id] = curSwitchLed;
}

void
ControlUi::SetSwitchConfig(int width, 
						   int height, 
						   const std::string & fontName, 
						   int fontHeight, 
						   bool bold)
{
	mSwitchConfig.mWidth = width;
	mSwitchConfig.mHeight = height;
	mSwitchConfig.mFontname = fontName.c_str();
	mSwitchConfig.mFontHeight = fontHeight;
	mSwitchConfig.mBold = bold;

	mSwitchButtonFont.setFamily(mSwitchConfig.mFontname);
	mSwitchButtonFont.setPointSize(mSwitchConfig.mFontHeight);
	mSwitchButtonFont.setBold(mSwitchConfig.mBold);
}

void
ControlUi::CreateSwitch(int id, 
						const std::string & label, 
						int top, 
						int left)
{
	Switch * curSwitch = new Switch(label.c_str(), this);
	curSwitch->setFont(mSwitchButtonFont);
	curSwitch->move(left, top);
	curSwitch->resize(mSwitchConfig.mWidth, mSwitchConfig.mHeight);

	QString qSlot;

	qSlot = QString("1UiButtonPressed_%1()").arg(id);
	connect(curSwitch, SIGNAL(pressed()), qSlot.toAscii().constData());

	qSlot = QString("1UiButtonReleased_%1()").arg(id);
	connect(curSwitch, SIGNAL(released()), qSlot.toAscii().constData());

	std::string::size_type pos = label.find("&");
	if (std::string::npos != pos && pos+1 < label.length())
	{
		// override default shortcut so that Alt key does 
		// not need to be held down
		const QString shortCutKey = label[pos + 1];
		curSwitch->setShortcut(QKeySequence(shortCutKey));
	}
	
	_ASSERTE(!mSwitches[id]);
	mSwitches[id] = curSwitch;
	if (id > mMaxSwitchId)
		mMaxSwitchId = id;
}

void
ControlUi::SetSwitchTextDisplayConfig(int width, 
									  int height, 
									  const std::string & fontName, 
									  int fontHeight, 
									  bool bold, 
									  unsigned int bgColor, 
									  unsigned int fgColor)
{
	mSwitchTextDisplayConfig.mHeight = height;
	mSwitchTextDisplayConfig.mWidth = width;
	mSwitchTextDisplayConfig.mFontname = fontName.c_str();
	mSwitchTextDisplayConfig.mFontHeight = fontHeight;
	mSwitchTextDisplayConfig.mBold = bold;
	mSwitchTextDisplayConfig.mFgColor = fgColor;
	mSwitchTextDisplayConfig.mBgColor = bgColor;
}

void
ControlUi::CreateSwitchTextDisplay(int id, 
								   int top, 
								   int left)
{
	SwitchTextDisplay * curSwitchDisplay = new SwitchTextDisplay(this);
	curSwitchDisplay->setMargin(2);
	curSwitchDisplay->setFrameShape(QFrame::Panel);
	curSwitchDisplay->setFrameShadow(QFrame::Sunken);
	curSwitchDisplay->setAutoFillBackground(true);
	curSwitchDisplay->move(left, top);
	curSwitchDisplay->resize(mSwitchTextDisplayConfig.mWidth, mSwitchTextDisplayConfig.mHeight);

	QFont font(mSwitchTextDisplayConfig.mFontname, mSwitchTextDisplayConfig.mFontHeight, mSwitchTextDisplayConfig.mBold ? QFont::Bold : QFont::Normal);
    curSwitchDisplay->setFont(font);

	QPalette pal;
	pal.setColor(QPalette::Window, mSwitchTextDisplayConfig.mBgColor);
	pal.setColor(QPalette::WindowText, mSwitchTextDisplayConfig.mFgColor);
	curSwitchDisplay->setPalette(pal);

	_ASSERTE(!mSwitchTextDisplays[id]);
	mSwitchTextDisplays[id] = curSwitchDisplay;
}

void
ControlUi::CreateMainDisplay(int top, 
							 int left, 
							 int width, 
							 int height,
							 const std::string & fontName,
							 int fontHeight, 
							 bool bold,
							 unsigned int bgColor, 
							 unsigned int fgColor)
{
	_ASSERTE(!mMainDisplay);
	mMainDisplay = new QPlainTextEdit(this);
	mMainDisplay->setFrameShape(QFrame::Panel);
	mMainDisplay->setFrameShadow(QFrame::Sunken);
	mMainDisplay->setReadOnly(true);
	mMainDisplay->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	mMainDisplay->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	mMainDisplay->setLineWrapMode(QPlainTextEdit::NoWrap);
	mMainDisplayRc.setTopLeft(QPoint(left, top));
	mMainDisplayRc.setBottomRight(QPoint(left+width, top+height));
	mMainDisplay->move(left, top);
	mMainDisplay->resize(width, height);

	QFont font(fontName.c_str(), fontHeight, bold ? QFont::Bold : QFont::Normal);
    mMainDisplay->setFont(font);

	QPalette pal;
	pal.setColor(QPalette::Window, bgColor);
	pal.setColor(QPalette::WindowText, fgColor);
	pal.setColor(QPalette::Text, fgColor);
	pal.setColor(QPalette::Base, bgColor);
	mMainDisplay->setPalette(pal);
}

void
ControlUi::CreateTraceDisplay(int top, 
							  int left, 
							  int width, 
							  int height, 
							  const std::string & fontName,
							  int fontHeight, 
							  bool bold)
{
	_ASSERTE(!mTraceDisplay);
	mTraceDisplay = new QPlainTextEdit(this);
	mTraceDisplay->setFrameShape(QFrame::Panel);
	mTraceDisplay->setFrameShadow(QFrame::Sunken);
	mTraceDisplay->setReadOnly(true);
	mTraceDisplay->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	mTraceDisplay->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	mTraceDisplay->setLineWrapMode(QPlainTextEdit::NoWrap);
	mTraceDiplayRc.setTopLeft(QPoint(left, top));
	mTraceDiplayRc.setBottomRight(QPoint(left+width, top+height));
	mTraceDisplay->move(left, top);
	mTraceDisplay->resize(width, height);

	mTraceFont.setFamily(fontName.c_str());
	mTraceFont.setPointSize(fontHeight);
	mTraceFont.setBold(bold);
	mTraceDisplay->setFont(mTraceFont);
}

void
ControlUi::CreateStaticLabel(const std::string & label, 
							 int top, 
							 int left, 
							 int width, 
							 int height, 
							 const std::string & fontName,
							 int fontHeight, 
							 bool bold,
							 unsigned int bgColor, 
							 unsigned int fgColor)
{
	_ASSERTE(!"not implemented yet");
}

void
ControlUi::SetMainSize(int width, 
					   int height)
{
	mPreferredHeight = height;
	mPreferredWidth = width;
}

IMidiOut *
ControlUi::CreateMidiOut(unsigned int deviceIdx,
						 int activityIndicatorIdx)
{
	if (!mMidiOuts[deviceIdx])
		mMidiOuts[deviceIdx] = new XMidiOut(this);

	if (activityIndicatorIdx > 0)
		mMidiOuts[deviceIdx]->SetActivityIndicator(this, activityIndicatorIdx);

	return mMidiOuts[deviceIdx];
}

IMidiOut *
ControlUi::GetMidiOut(unsigned int deviceIdx)
{
	return mMidiOuts[deviceIdx];
}

void
ControlUi::OpenMidiOuts()
{
	for (MidiOuts::iterator it = mMidiOuts.begin();
		it != mMidiOuts.end();
		++it)
	{
		IMidiOut * curOut = (*it).second;
		if (curOut->IsMidiOutOpen())
			continue;

		std::strstream traceMsg;
		const unsigned int kDeviceIdx = (*it).first;

		if (curOut->OpenMidiOut(kDeviceIdx))
			traceMsg << "Opened MIDI out " << kDeviceIdx << " " << curOut->GetMidiOutDeviceName(kDeviceIdx) << std::endl << std::ends;
		else
			traceMsg << "Failed to open MIDI out " << kDeviceIdx << std::endl << std::ends;

		Trace(traceMsg.str());
	}
}

void
ControlUi::CloseMidiOuts()
{
	for (MidiOuts::iterator it = mMidiOuts.begin();
		it != mMidiOuts.end();
		++it)
	{
		IMidiOut * curOut = (*it).second;
		if (curOut && curOut->IsMidiOutOpen())
			curOut->CloseMidiOut();
	}
}

// IMonome40hSwitchSubscriber
void
ControlUi::SwitchPressed(byte row, byte column)
{
	const int switchNumber = SwitchNumberFromRowCol(row, column);
	mStupidSwitchStates[switchNumber] = true;
	if (mEngine)
		mEngine->SwitchPressed(switchNumber);
}

void
ControlUi::SwitchReleased(byte row, byte column)
{
	const int switchNumber = SwitchNumberFromRowCol(row, column);
	mStupidSwitchStates[switchNumber] = false;
	if (mEngine)
		mEngine->SwitchReleased(switchNumber);
}

void
ControlUi::MonomeStartupSequence()
{
	if (!mHardwareUi)
		return;

	int idx;
	byte vals = 0xff;
	for (idx = 0; idx < 8; ++idx)
	{
		mHardwareUi->EnableLedRow(idx, vals);
		SLEEP(100);
		mHardwareUi->EnableLedRow(idx, 0);
	}

	for (idx = 0; idx < 8; ++idx)
	{
		mHardwareUi->EnableLedColumn(idx, vals);
		SLEEP(100);
		mHardwareUi->EnableLedColumn(idx, 0);
	}

	mHardwareUi->TestLed(true);
	SLEEP(250);
	mHardwareUi->TestLed(false);
}

void
ControlUi::Reconnect()
{
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	const int kPorts = ExpressionPedals::PedalCount;
	// temporaries - possibly different than startup state due to interactive override
	bool adcEnables[kPorts] = {false, false, false, false};

	if (mHardwareUi)
	{
		mHardwareUi->Unsubscribe(mEngine);
		mHardwareUi->Unsubscribe(this);
		for (int idx = 0; idx < kPorts; ++idx)
			adcEnables[idx] = mHardwareUi->IsAdcEnabled(idx);
		delete mHardwareUi;
		mHardwareUi = NULL;
	}

	LoadMonome(false);

	if (mHardwareUi)
	{
		for (int idx = 0; idx < kPorts; ++idx)
			mHardwareUi->EnableAdc(idx, adcEnables[idx]);

		mHardwareUi->Subscribe(this);
		mHardwareUi->Subscribe(mEngine);
	}

	QApplication::restoreOverrideCursor();
}

void
ControlUi::ToggleTraceWindow()
{
	if (!mTraceDisplay)
		return;

	if (mTraceDisplay->isVisible())
	{
		mTraceDisplay->hide();
		QRect mainRc(mMainDisplayRc);

		if (mTraceDiplayRc.top() >= mMainDisplayRc.top() &&
			mTraceDiplayRc.bottom() <= mMainDisplayRc.bottom())
		{
			// change width of main display to use space vacated by trace display
			if (mMainDisplayRc.left() > mTraceDiplayRc.left())
				mainRc.setLeft(mTraceDiplayRc.left());
			else if (mMainDisplayRc.right() < mTraceDiplayRc.right())
				mainRc.setRight(mTraceDiplayRc.left() + mTraceDiplayRc.width());
		}

		if (mTraceDiplayRc.left() >= mMainDisplayRc.left() &&
			mTraceDiplayRc.right() <= mMainDisplayRc.right())
		{
			// change height of main display to use space vacated by trace display
			if (mMainDisplayRc.top() > mTraceDiplayRc.top())
				mainRc.setTop(mTraceDiplayRc.top());
			else if (mMainDisplayRc.bottom() < mTraceDiplayRc.bottom())
				mainRc.setBottom(mTraceDiplayRc.top() + mTraceDiplayRc.height());
		}

		mMainDisplay->move(mainRc.left(), mainRc.top());
		mMainDisplay->resize(mainRc.width(), mainRc.height());
	}
	else
	{
		mMainDisplay->move(mMainDisplayRc.left(), mMainDisplayRc.top());
		mMainDisplay->resize(mMainDisplayRc.width(), mMainDisplayRc.height());
		mTraceDisplay->show();
	}
}

bool 
ControlUi::event(QEvent* event)
{
	if (QEvent::User == event->type())
	{
		ControlUiEvent * evt = static_cast<ControlUiEvent*>(event);
		evt->exec();
		evt->accept();
		return true;
	}

	return QWidget::event(event);
}

void
ControlUi::UpdateAdcs(const bool adcOverrides[ExpressionPedals::PedalCount])
{
	if (!mHardwareUi)
		return;

	// handle in reverse order using (idx - 1)
	for (int idx = ExpressionPedals::PedalCount; idx; --idx)
	{
		const bool enable = !adcOverrides[idx - 1] && mUserAdcSettings[idx - 1];
		mHardwareUi->EnableAdc(idx - 1, enable);
		if (mTraceDisplay)
		{
			std::strstream traceMsg;
			traceMsg << "  ADC port " << idx << (enable ? " enabled" : " disabled") << std::endl << std::ends;
			Trace(std::string(traceMsg.str()));
		}
	}
}
