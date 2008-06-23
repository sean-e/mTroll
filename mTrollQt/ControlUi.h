#ifndef ControlUi_h__
#define ControlUi_h__

#include <map>
#include <QWidget>
#include <QFont>

#include "../Engine/IMainDisplay.h"
#include "../Engine/ISwitchDisplay.h"
#include "../Engine/ITraceDisplay.h"
#include "../Engine/MidiControlEngine.h"
#include "../Engine/IMidiControlUi.h"
#include "../Engine/IMidiOutGenerator.h"
#include "../Monome40h/IMonome40hInputSubscriber.h"

#ifdef _WINDOWS
	#include "../winUtil/KeepDisplayOn.h"
	#include "../midi/WinMidiOut.h"
	typedef WinMidiOut	XMidiOut;
	#undef TextOut		// Win A/W preprocessing hoses IMainDisplay::TextOut impl
#else
	struct KeepDisplayOn {};
	#error "include the midiout header file for this platform"
	typedef YourMidiOut	XMidiOut;
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
	// sigh... the one time that I would use a macro but the Qt MOC doesn't support it (or the use of tokenization)!!
	virtual void UiButtonClicked_00(bool checked = false) { ButtonPressed(0); ButtonReleased(0); }
	virtual void UiButtonClicked_01(bool checked = false) { ButtonPressed(1); ButtonReleased(1); }
	virtual void UiButtonClicked_02(bool checked = false) { ButtonPressed(2); ButtonReleased(2); }
	virtual void UiButtonClicked_03(bool checked = false) { ButtonPressed(3); ButtonReleased(3); }
	virtual void UiButtonClicked_04(bool checked = false) { ButtonPressed(4); ButtonReleased(4); }
	virtual void UiButtonClicked_05(bool checked = false) { ButtonPressed(5); ButtonReleased(5); }
	virtual void UiButtonClicked_06(bool checked = false) { ButtonPressed(6); ButtonReleased(6); }
	virtual void UiButtonClicked_07(bool checked = false) { ButtonPressed(7); ButtonReleased(7); }
	virtual void UiButtonClicked_08(bool checked = false) { ButtonPressed(8); ButtonReleased(8); }
	virtual void UiButtonClicked_09(bool checked = false) { ButtonPressed(9); ButtonReleased(9); }
	virtual void UiButtonClicked_10(bool checked = false) { ButtonPressed(10); ButtonReleased(10); }
	virtual void UiButtonClicked_11(bool checked = false) { ButtonPressed(11); ButtonReleased(11); }
	virtual void UiButtonClicked_12(bool checked = false) { ButtonPressed(12); ButtonReleased(12); }
	virtual void UiButtonClicked_13(bool checked = false) { ButtonPressed(13); ButtonReleased(13); }
	virtual void UiButtonClicked_14(bool checked = false) { ButtonPressed(14); ButtonReleased(14); }
	virtual void UiButtonClicked_15(bool checked = false) { ButtonPressed(15); ButtonReleased(15); }
	virtual void UiButtonClicked_16(bool checked = false) { ButtonPressed(16); ButtonReleased(16); }
	virtual void UiButtonClicked_17(bool checked = false) { ButtonPressed(17); ButtonReleased(17); }
	virtual void UiButtonClicked_18(bool checked = false) { ButtonPressed(18); ButtonReleased(18); }
	virtual void UiButtonClicked_19(bool checked = false) { ButtonPressed(19); ButtonReleased(19); }
	virtual void UiButtonClicked_20(bool checked = false) { ButtonPressed(20); ButtonReleased(20); }
	virtual void UiButtonClicked_21(bool checked = false) { ButtonPressed(21); ButtonReleased(21); }
	virtual void UiButtonClicked_22(bool checked = false) { ButtonPressed(22); ButtonReleased(22); }
	virtual void UiButtonClicked_23(bool checked = false) { ButtonPressed(23); ButtonReleased(23); }
	virtual void UiButtonClicked_24(bool checked = false) { ButtonPressed(24); ButtonReleased(24); }
	virtual void UiButtonClicked_25(bool checked = false) { ButtonPressed(25); ButtonReleased(25); }
	virtual void UiButtonClicked_26(bool checked = false) { ButtonPressed(26); ButtonReleased(26); }
	virtual void UiButtonClicked_27(bool checked = false) { ButtonPressed(27); ButtonReleased(27); }
	virtual void UiButtonClicked_28(bool checked = false) { ButtonPressed(28); ButtonReleased(28); }
	virtual void UiButtonClicked_29(bool checked = false) { ButtonPressed(29); ButtonReleased(29); }
	virtual void UiButtonClicked_30(bool checked = false) { ButtonPressed(30); ButtonReleased(30); }
	virtual void UiButtonClicked_31(bool checked = false) { ButtonPressed(31); ButtonReleased(31); }

	virtual void UiButtonPressed_00() { ButtonPressed(0); }
	virtual void UiButtonPressed_01() { ButtonPressed(1); }
	virtual void UiButtonPressed_02() { ButtonPressed(2); }
	virtual void UiButtonPressed_03() { ButtonPressed(3); }
	virtual void UiButtonPressed_04() { ButtonPressed(4); }
	virtual void UiButtonPressed_05() { ButtonPressed(5); }
	virtual void UiButtonPressed_06() { ButtonPressed(6); }
	virtual void UiButtonPressed_07() { ButtonPressed(7); }
	virtual void UiButtonPressed_08() { ButtonPressed(8); }
	virtual void UiButtonPressed_09() { ButtonPressed(9); }
	virtual void UiButtonPressed_10() { ButtonPressed(10); }
	virtual void UiButtonPressed_11() { ButtonPressed(11); }
	virtual void UiButtonPressed_12() { ButtonPressed(12); }
	virtual void UiButtonPressed_13() { ButtonPressed(13); }
	virtual void UiButtonPressed_14() { ButtonPressed(14); }
	virtual void UiButtonPressed_15() { ButtonPressed(15); }
	virtual void UiButtonPressed_16() { ButtonPressed(16); }
	virtual void UiButtonPressed_17() { ButtonPressed(17); }
	virtual void UiButtonPressed_18() { ButtonPressed(18); }
	virtual void UiButtonPressed_19() { ButtonPressed(19); }
	virtual void UiButtonPressed_20() { ButtonPressed(20); }
	virtual void UiButtonPressed_21() { ButtonPressed(21); }
	virtual void UiButtonPressed_22() { ButtonPressed(22); }
	virtual void UiButtonPressed_23() { ButtonPressed(23); }
	virtual void UiButtonPressed_24() { ButtonPressed(24); }
	virtual void UiButtonPressed_25() { ButtonPressed(25); }
	virtual void UiButtonPressed_26() { ButtonPressed(26); }
	virtual void UiButtonPressed_27() { ButtonPressed(27); }
	virtual void UiButtonPressed_28() { ButtonPressed(28); }
	virtual void UiButtonPressed_29() { ButtonPressed(29); }
	virtual void UiButtonPressed_30() { ButtonPressed(30); }
	virtual void UiButtonPressed_31() { ButtonPressed(31); }

	virtual void UiButtonReleased_00() { ButtonReleased(0); }
	virtual void UiButtonReleased_01() { ButtonReleased(1); }
	virtual void UiButtonReleased_02() { ButtonReleased(2); }
	virtual void UiButtonReleased_03() { ButtonReleased(3); }
	virtual void UiButtonReleased_04() { ButtonReleased(4); }
	virtual void UiButtonReleased_05() { ButtonReleased(5); }
	virtual void UiButtonReleased_06() { ButtonReleased(6); }
	virtual void UiButtonReleased_07() { ButtonReleased(7); }
	virtual void UiButtonReleased_08() { ButtonReleased(8); }
	virtual void UiButtonReleased_09() { ButtonReleased(9); }
	virtual void UiButtonReleased_10() { ButtonReleased(10); }
	virtual void UiButtonReleased_11() { ButtonReleased(11); }
	virtual void UiButtonReleased_12() { ButtonReleased(12); }
	virtual void UiButtonReleased_13() { ButtonReleased(13); }
	virtual void UiButtonReleased_14() { ButtonReleased(14); }
	virtual void UiButtonReleased_15() { ButtonReleased(15); }
	virtual void UiButtonReleased_16() { ButtonReleased(16); }
	virtual void UiButtonReleased_17() { ButtonReleased(17); }
	virtual void UiButtonReleased_18() { ButtonReleased(18); }
	virtual void UiButtonReleased_19() { ButtonReleased(19); }
	virtual void UiButtonReleased_20() { ButtonReleased(20); }
	virtual void UiButtonReleased_21() { ButtonReleased(21); }
	virtual void UiButtonReleased_22() { ButtonReleased(22); }
	virtual void UiButtonReleased_23() { ButtonReleased(23); }
	virtual void UiButtonReleased_24() { ButtonReleased(24); }
	virtual void UiButtonReleased_25() { ButtonReleased(25); }
	virtual void UiButtonReleased_26() { ButtonReleased(26); }
	virtual void UiButtonReleased_27() { ButtonReleased(27); }
	virtual void UiButtonReleased_28() { ButtonReleased(28); }
	virtual void UiButtonReleased_29() { ButtonReleased(29); }
	virtual void UiButtonReleased_30() { ButtonReleased(30); }
	virtual void UiButtonReleased_31() { ButtonReleased(31); }

private:
	void AsyncTextOut(void * wParam);

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
	typedef std::map<unsigned int, XMidiOut*> MidiOuts;
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

#endif // ControlUi_h__
