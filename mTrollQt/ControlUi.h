#pragma once

#include <QWidget>
#include <QFont>
#include <map>

#include "../Engine/IMainDisplay.h"
#include "../Engine/ISwitchDisplay.h"
#include "../Engine/ITraceDisplay.h"
#include "../Engine/MidiControlEngine.h"
#include "../Engine/IMidiControlUi.h"
#include "../Engine/IMidiOutGenerator.h"
#include "../Monome40h/IMonome40hInputSubscriber.h"

#ifdef _WINDOWS
#include "../midi/WinMidiOut.h"
#include "../winUtil/KeepDisplayOn.h"
#else
#error "include the midiout header file for this platform"
struct KeepDisplayOn {};
#endif


class QLabel;
class QPushButton;
class QTextEdit;
class IMonome40h;


class ControlUi : public QWidget, 
						 IMainDisplay, 
						 ISwitchDisplay, 
						 ITraceDisplay,
						 IMidiControlUi,
						 IMidiOutGenerator,
						 IMonome40hSwitchSubscriber
{
	Q_OBJECT;
public:
	ControlUi(QWidget * parent);
	virtual ~ControlUi();

	typedef QWidget				SwitchLed;
	typedef QLabel				SwitchTextDisplay;
	typedef QPushButton			Switch;

			void Load(const std::string & uiSettingsFile, const std::string & configSettingsFile);
			void Unload();

			void GetPreferredSize(int & width, int & height) const {width = mPreferredWidth; height = mPreferredHeight;}

public: // IMidiOutGenerator
	virtual IMidiOut *	CreateMidiOut(unsigned int deviceIdx, int activityIndicatorIdx);
	virtual IMidiOut *	GetMidiOut(unsigned int deviceIdx);
	virtual void		OpenMidiOuts();
	virtual void		CloseMidiOuts();

public: // IMainDisplay
	virtual void		TextOut(const std::string & txt);
	virtual void		ClearDisplay();

public: // ITraceDisplay
	virtual void		Trace(const std::string & txt);

public: // ISwitchDisplay
	virtual void		SetSwitchDisplay(int switchNumber, bool isOn);
	virtual void		SetSwitchText(int switchNumber, const std::string & txt);
	virtual void		ClearSwitchText(int switchNumber);

public: // IMonome40hSwitchSubscriber
	virtual void		SwitchPressed(byte row, byte column);
	virtual void		SwitchReleased(byte row, byte column);

private: // IMidiControlUi
	virtual void		AddSwitchMapping(int switchNumber, int row, int col);
	virtual void		SetSwitchLedConfig(int width, int height, unsigned int onColor, unsigned int offColor);
	virtual void		CreateSwitchLed(int id, int top, int left);
	virtual void		SetSwitchConfig(int width, int height, const std::string & fontName, int fontHeight, bool bold);
	virtual void		CreateSwitch(int id, const std::string & label, int top, int left);
	virtual void		SetSwitchTextDisplayConfig(int width, int height, const std::string & fontName, int fontHeight, bool bold, unsigned int bgColor, unsigned int fgColor);
	virtual void		CreateSwitchTextDisplay(int id, int top, int left);
	virtual void		CreateMainDisplay(int top, int left, int width, int height, const std::string & fontName, int fontHeight, bool bold, unsigned int bgColor, unsigned int fgColor);
	virtual void		CreateTraceDisplay(int top, int left, int width, int height, const std::string & fontName, int fontHeight, bool bold);
	virtual void		CreateStaticLabel(const std::string & label, int top, int left, int width, int height, const std::string & fontName, int fontHeight, bool bold, unsigned int bgColor, unsigned int fgColor);
	virtual void		SetMainSize(int width, int height);
	virtual void		SetHardwareLedIntensity(short brightness) { mLedIntensity = brightness; }
	virtual void		SetLedDisplayState(bool invert) { mInvertLeds = invert; }

private slots:
#define UiButtonClickedHandler(x) \
	virtual void UiButtonClicked_##x(bool checked = false) { ButtonPressed(x); ButtonReleased(x); }

	UiButtonClickedHandler(1);
	UiButtonClickedHandler(2);
	UiButtonClickedHandler(3);
	UiButtonClickedHandler(4);
	UiButtonClickedHandler(5);
	UiButtonClickedHandler(6);
	UiButtonClickedHandler(7);
	UiButtonClickedHandler(8);
	UiButtonClickedHandler(9);
	UiButtonClickedHandler(10);
	UiButtonClickedHandler(11);
	UiButtonClickedHandler(12);
	UiButtonClickedHandler(13);
	UiButtonClickedHandler(14);
	UiButtonClickedHandler(15);
	UiButtonClickedHandler(16);
	UiButtonClickedHandler(17);
	UiButtonClickedHandler(18);
	UiButtonClickedHandler(19);
	UiButtonClickedHandler(20);
	UiButtonClickedHandler(21);
	UiButtonClickedHandler(22);
	UiButtonClickedHandler(23);
	UiButtonClickedHandler(24);
	UiButtonClickedHandler(25);
	UiButtonClickedHandler(26);
	UiButtonClickedHandler(27);
	UiButtonClickedHandler(28);
	UiButtonClickedHandler(29);
	UiButtonClickedHandler(30);
	UiButtonClickedHandler(31);
	UiButtonClickedHandler(32);

#define UiButtonPressedHandler(x) \
	virtual void UiButtonPressed_##x() { ButtonPressed(x); }

	UiButtonPressedHandler(1);
	UiButtonPressedHandler(2);
	UiButtonPressedHandler(3);
	UiButtonPressedHandler(4);
	UiButtonPressedHandler(5);
	UiButtonPressedHandler(6);
	UiButtonPressedHandler(7);
	UiButtonPressedHandler(8);
	UiButtonPressedHandler(9);
	UiButtonPressedHandler(10);
	UiButtonPressedHandler(11);
	UiButtonPressedHandler(12);
	UiButtonPressedHandler(13);
	UiButtonPressedHandler(14);
	UiButtonPressedHandler(15);
	UiButtonPressedHandler(16);
	UiButtonPressedHandler(17);
	UiButtonPressedHandler(18);
	UiButtonPressedHandler(19);
	UiButtonPressedHandler(20);
	UiButtonPressedHandler(21);
	UiButtonPressedHandler(22);
	UiButtonPressedHandler(23);
	UiButtonPressedHandler(24);
	UiButtonPressedHandler(25);
	UiButtonPressedHandler(26);
	UiButtonPressedHandler(27);
	UiButtonPressedHandler(28);
	UiButtonPressedHandler(29);
	UiButtonPressedHandler(30);
	UiButtonPressedHandler(31);
	UiButtonPressedHandler(32);

#define UiButtonReleasedHandler(x) \
	virtual void UiButtonReleased_##x() { ButtonReleased(x); }

	UiButtonReleasedHandler(1);
	UiButtonReleasedHandler(2);
	UiButtonReleasedHandler(3);
	UiButtonReleasedHandler(4);
	UiButtonReleasedHandler(5);
	UiButtonReleasedHandler(6);
	UiButtonReleasedHandler(7);
	UiButtonReleasedHandler(8);
	UiButtonReleasedHandler(9);
	UiButtonReleasedHandler(10);
	UiButtonReleasedHandler(11);
	UiButtonReleasedHandler(12);
	UiButtonReleasedHandler(13);
	UiButtonReleasedHandler(14);
	UiButtonReleasedHandler(15);
	UiButtonReleasedHandler(16);
	UiButtonReleasedHandler(17);
	UiButtonReleasedHandler(18);
	UiButtonReleasedHandler(19);
	UiButtonReleasedHandler(20);
	UiButtonReleasedHandler(21);
	UiButtonReleasedHandler(22);
	UiButtonReleasedHandler(23);
	UiButtonReleasedHandler(24);
	UiButtonReleasedHandler(25);
	UiButtonReleasedHandler(26);
	UiButtonReleasedHandler(27);
	UiButtonReleasedHandler(28);
	UiButtonReleasedHandler(29);
	UiButtonReleasedHandler(30);
	UiButtonReleasedHandler(31);
	UiButtonReleasedHandler(32);

private:
	LRESULT AsyncTextOut(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);

	void LoadUi(const std::string & uiSettingsFile);
	void LoadMonome();
	void LoadMidiSettings(const std::string & file);
	void ActivityIndicatorHack();

	void ButtonReleased(const int idx);
	void ButtonPressed(const int idx);

	void MonomeStartupSequence();
	inline void RowColFromSwitchNumber(int ord, byte & row, byte & col)
	{
		int rc = mSwitchNumberToRowCol[ord];
		col = (byte)(rc & 0xff);
		row = (byte)((rc >> 16) & 0xff);
	}
	inline int SwitchNumberFromRowCol(byte row, byte col)
	{
		return mRowColToSwitchNumber[(row << 16) | col];
	}

private:
	IMonome40h					* mHardwareUi;
	MidiControlEngine			* mEngine;
	QLabel						* mMainDisplay;
	QTextEdit					* mTraceDisplay;
	std::map<int, SwitchLed*>	mLeds;
	std::map<int, SwitchTextDisplay *>		mSwitchTextDisplays;
	std::map<int, Switch *>		mSwitches;
	std::map<int, bool>			mStupidSwitchStates;
	std::map<int, int>			mSwitchNumberToRowCol;
	std::map<int, int>			mRowColToSwitchNumber;
	QFont						mSwitchButtonFont;
	QFont						mTraceFont;
	int							mPreferredHeight, mPreferredWidth;
	int							mMaxSwitchId;
	typedef std::map<unsigned int, TMidiOut*> MidiOuts;
	MidiOuts					mMidiOuts;
	int							mLedIntensity;
	bool						mInvertLeds;
	KeepDisplayOn				* mSystemPowerOverride;

	struct SwitchTextDisplayConfig
	{
		int						mHeight;
		int						mWidth;
		int						mFontHeight;
		QString					mFontname;
		DWORD					mFgColor;
		DWORD					mBgColor;
		bool					mBold;
	};
	SwitchTextDisplayConfig		mSwitchTextDisplayConfig;

	struct SwitchConfig
	{
		int						mHeight;
		int						mWidth;
		QString					mFontname;
		int						mFontHeight;
		bool					mBold;
	};
	SwitchConfig				mSwitchConfig;

	struct LedConfig
	{
		int						mHeight;
		int						mWidth;
		DWORD					mOnColor;
		DWORD					mOffColor;
	};
	LedConfig					mLedConfig;
};
