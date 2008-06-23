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
class QFrame;
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

	typedef QFrame				SwitchLed;
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
	virtual void UiButtonPressed_0() { ButtonPressed(0); }
	virtual void UiButtonPressed_1() { ButtonPressed(1); }
	virtual void UiButtonPressed_2() { ButtonPressed(2); }
	virtual void UiButtonPressed_3() { ButtonPressed(3); }
	virtual void UiButtonPressed_4() { ButtonPressed(4); }
	virtual void UiButtonPressed_5() { ButtonPressed(5); }
	virtual void UiButtonPressed_6() { ButtonPressed(6); }
	virtual void UiButtonPressed_7() { ButtonPressed(7); }
	virtual void UiButtonPressed_8() { ButtonPressed(8); }
	virtual void UiButtonPressed_9() { ButtonPressed(9); }
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
	virtual void UiButtonPressed_32() { ButtonPressed(32); }
	virtual void UiButtonPressed_33() { ButtonPressed(33); }
	virtual void UiButtonPressed_34() { ButtonPressed(34); }
	virtual void UiButtonPressed_35() { ButtonPressed(35); }
	virtual void UiButtonPressed_36() { ButtonPressed(36); }
	virtual void UiButtonPressed_37() { ButtonPressed(37); }
	virtual void UiButtonPressed_38() { ButtonPressed(38); }
	virtual void UiButtonPressed_39() { ButtonPressed(39); }
	virtual void UiButtonPressed_40() { ButtonPressed(40); }
	virtual void UiButtonPressed_41() { ButtonPressed(41); }
	virtual void UiButtonPressed_42() { ButtonPressed(42); }
	virtual void UiButtonPressed_43() { ButtonPressed(43); }
	virtual void UiButtonPressed_44() { ButtonPressed(44); }
	virtual void UiButtonPressed_45() { ButtonPressed(45); }
	virtual void UiButtonPressed_46() { ButtonPressed(46); }
	virtual void UiButtonPressed_47() { ButtonPressed(47); }
	virtual void UiButtonPressed_48() { ButtonPressed(48); }
	virtual void UiButtonPressed_49() { ButtonPressed(49); }
	virtual void UiButtonPressed_50() { ButtonPressed(50); }
	virtual void UiButtonPressed_51() { ButtonPressed(51); }
	virtual void UiButtonPressed_52() { ButtonPressed(52); }
	virtual void UiButtonPressed_53() { ButtonPressed(53); }
	virtual void UiButtonPressed_54() { ButtonPressed(54); }
	virtual void UiButtonPressed_55() { ButtonPressed(55); }
	virtual void UiButtonPressed_56() { ButtonPressed(56); }
	virtual void UiButtonPressed_57() { ButtonPressed(57); }
	virtual void UiButtonPressed_58() { ButtonPressed(58); }
	virtual void UiButtonPressed_59() { ButtonPressed(59); }
	virtual void UiButtonPressed_60() { ButtonPressed(60); }
	virtual void UiButtonPressed_61() { ButtonPressed(61); }
	virtual void UiButtonPressed_62() { ButtonPressed(62); }
	virtual void UiButtonPressed_63() { ButtonPressed(63); }
	virtual void UiButtonPressed_64() { ButtonPressed(64); }

	virtual void UiButtonReleased_0() { ButtonReleased(0); }
	virtual void UiButtonReleased_1() { ButtonReleased(1); }
	virtual void UiButtonReleased_2() { ButtonReleased(2); }
	virtual void UiButtonReleased_3() { ButtonReleased(3); }
	virtual void UiButtonReleased_4() { ButtonReleased(4); }
	virtual void UiButtonReleased_5() { ButtonReleased(5); }
	virtual void UiButtonReleased_6() { ButtonReleased(6); }
	virtual void UiButtonReleased_7() { ButtonReleased(7); }
	virtual void UiButtonReleased_8() { ButtonReleased(8); }
	virtual void UiButtonReleased_9() { ButtonReleased(9); }
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
	virtual void UiButtonReleased_32() { ButtonReleased(32); }
	virtual void UiButtonReleased_33() { ButtonReleased(33); }
	virtual void UiButtonReleased_34() { ButtonReleased(34); }
	virtual void UiButtonReleased_35() { ButtonReleased(35); }
	virtual void UiButtonReleased_36() { ButtonReleased(36); }
	virtual void UiButtonReleased_37() { ButtonReleased(37); }
	virtual void UiButtonReleased_38() { ButtonReleased(38); }
	virtual void UiButtonReleased_39() { ButtonReleased(39); }
	virtual void UiButtonReleased_40() { ButtonReleased(40); }
	virtual void UiButtonReleased_41() { ButtonReleased(41); }
	virtual void UiButtonReleased_42() { ButtonReleased(42); }
	virtual void UiButtonReleased_43() { ButtonReleased(43); }
	virtual void UiButtonReleased_44() { ButtonReleased(44); }
	virtual void UiButtonReleased_45() { ButtonReleased(45); }
	virtual void UiButtonReleased_46() { ButtonReleased(46); }
	virtual void UiButtonReleased_47() { ButtonReleased(47); }
	virtual void UiButtonReleased_48() { ButtonReleased(48); }
	virtual void UiButtonReleased_49() { ButtonReleased(49); }
	virtual void UiButtonReleased_50() { ButtonReleased(50); }
	virtual void UiButtonReleased_51() { ButtonReleased(51); }
	virtual void UiButtonReleased_52() { ButtonReleased(52); }
	virtual void UiButtonReleased_53() { ButtonReleased(53); }
	virtual void UiButtonReleased_54() { ButtonReleased(54); }
	virtual void UiButtonReleased_55() { ButtonReleased(55); }
	virtual void UiButtonReleased_56() { ButtonReleased(56); }
	virtual void UiButtonReleased_57() { ButtonReleased(57); }
	virtual void UiButtonReleased_58() { ButtonReleased(58); }
	virtual void UiButtonReleased_59() { ButtonReleased(59); }
	virtual void UiButtonReleased_60() { ButtonReleased(60); }
	virtual void UiButtonReleased_61() { ButtonReleased(61); }
	virtual void UiButtonReleased_62() { ButtonReleased(62); }
	virtual void UiButtonReleased_63() { ButtonReleased(63); }
	virtual void UiButtonReleased_64() { ButtonReleased(64); }

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
