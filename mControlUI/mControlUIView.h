// mControlUIView.h : interface of the CMControlUIView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <map>
#include "..\Engine\IMainDisplay.h"
#include "..\Engine\ISwitchDisplay.h"
#include "..\Engine\ITraceDisplay.h"
#include "..\Engine\MidiControlEngine.h"
#include "..\Engine\IMidiControlUi.h"
#include "..\Engine\IMidiOutGenerator.h"
#include "WinMidiOut.h"
#include <atlctrls.h>
#include "AtlLed.h"
#include "../Monome40h/IMonome40hInputSubscriber.h"


class CLabel;
class IMonome40h;


class CMControlUIView : public CDialogImpl<CMControlUIView>, 
								IMainDisplay, 
								ISwitchDisplay, 
								ITraceDisplay,
								IMidiControlUi,
								IMidiOutGenerator,
								IMonome40hInputSubscriber
{
public:
	CMControlUIView();
	virtual ~CMControlUIView();

	enum { IDD = IDD_MCONTROLUI_FORM };
	typedef CLed				SwitchLed;
	typedef CLabel				SwitchTextDisplay;
	typedef CButton				Switch;

			HWND Create(HWND hWndParent, LPARAM dwInitParam = NULL);
			void Load(const std::string & settingsBasefile);
			void Unload();

			BOOL PreTranslateMessage(MSG* pMsg);
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

public: // IMonome40hInputSubscriber
	virtual void		SwitchPressed(byte row, byte column);
	virtual void		SwitchReleased(byte row, byte column);
	virtual	void		AdcValueChanged(int port, int curValue);

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

	enum {WM_ASYNCTEXTOUT = WM_USER + 1};

	BEGIN_MSG_MAP(CMControlUIView)
		MESSAGE_HANDLER(WM_ASYNCTEXTOUT, AsyncTextOut)
		NOTIFY_CODE_HANDLER(NM_CUSTOMDRAW, OnNotifyCustomDraw)
		COMMAND_HANDLER(0, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(1, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(2, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(3, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(4, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(5, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(6, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(7, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(8, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(9, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(10, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(11, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(12, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(13, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(14, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(15, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(16, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(17, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(18, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(19, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(20, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(21, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(22, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(23, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(24, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(25, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(26, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(27, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(28, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(29, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(30, BN_CLICKED, OnBnPushed)
		COMMAND_HANDLER(31, BN_CLICKED, OnBnPushed)
	END_MSG_MAP()

private:
// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
	LRESULT OnNotifyCustomDraw(int idCtrl, LPNMHDR pNotifyStruct, BOOL& /*bHandled*/);
	LRESULT OnBnPushed(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
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
	CLabel						* mMainDisplay;
	CEdit						* mTraceDisplay;
	std::map<int, SwitchLed*>	mLeds;
	std::map<int, SwitchTextDisplay *>		mSwitchTextDisplays;
	std::map<int, Switch *>		mSwitches;
	std::map<int, bool>			mStupidSwitchStates;
	std::map<int, int>			mSwitchNumberToRowCol;
	std::map<int, int>			mRowColToSwitchNumber;
	CFont						mSwitchButtonFont;
	CFont						mTraceFont;
	int							mPreferredHeight, mPreferredWidth;
	int							mMaxSwitchId;
	UINT						mKeyMessage;
	typedef std::map<unsigned int, WinMidiOut*> MidiOuts;
	MidiOuts					mMidiOuts;

	struct SwitchTextDisplayConfig
	{
		int						mHeight;
		int						mWidth;
		int						mFontHeight;
		std::string				mFontname;
		DWORD					mFgColor;
		DWORD					mBgColor;
		bool					mBold;
	};
	SwitchTextDisplayConfig		mSwitchTextDisplayConfig;

	struct SwitchConfig
	{
		int						mHeight;
		int						mWidth;
		std::string				mFontname;
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
