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
#include "WinMidiOut.h"
#include <atlctrls.h>


class CLabel;


class CMControlUIView : public CDialogImpl<CMControlUIView>, 
								IMainDisplay, 
								ISwitchDisplay, 
								ITraceDisplay,
								IMidiControlUi
{
public:
	CMControlUIView();
	~CMControlUIView();

	enum { IDD = IDD_MCONTROLUI_FORM };
	typedef CProgressBarCtrl	SwitchLed;
	typedef CLabel				SwitchTextDisplay;
	typedef CButton				Switch;

	HWND Create(HWND hWndParent, LPARAM dwInitParam = NULL);
	void LoadUi(const std::string & uiSettingsFile);
	void LoadMidiSettings(const std::string & file);

	BOOL PreTranslateMessage(MSG* pMsg);

	// IMainDisplay
	virtual void TextOut(const std::string & txt);
	virtual void ClearDisplay();

	// ITraceDisplay
	virtual void Trace(const std::string & txt);

	// ISwitchDisplay
	virtual void SetSwitchDisplay(int switchNumber, bool isOn);
	virtual bool SupportsSwitchText() const;
	virtual void SetSwitchText(int switchNumber, const std::string & txt);
	virtual void ClearSwitchText(int switchNumber);
	virtual void SetSwitchDisplayPos(int switchNumber, int pos, int range);

private:
	void Unload();

	// IMidiControlUi
	virtual void	SetSwitchLedConfig(int width, int height, unsigned int onColor, unsigned int offColor);
	virtual void	CreateSwitchLed(int id, int top, int left);
	virtual void	SetSwitchConfig(int width, int height, const std::string & fontName, int fontHeight, bool bold);
	virtual void	CreateSwitch(int id, const std::string & label, int top, int left);
	virtual void	SetSwitchTextDisplayConfig(int width, int height, const std::string & fontName, int fontHeight, bool bold, unsigned int bgColor, unsigned int fgColor);
	virtual void	CreateSwitchTextDisplay(int id, int top, int left);
	virtual void	CreateMainDisplay(int top, int left, int width, int height, const std::string & fontName, int fontHeight, bool bold, unsigned int bgColor, unsigned int fgColor);
	virtual void	CreateTraceDisplay(int top, int left, int width, int height, const std::string & fontName, int fontHeight, bool bold);
	virtual void	CreateStaticLabel(const std::string & label, int top, int left, int width, int height, const std::string & fontName, int fontHeight, bool bold, unsigned int bgColor, unsigned int fgColor);
	virtual void	SetMainSize(int width, int height);

	BEGIN_MSG_MAP(CMControlUIView)
		NOTIFY_CODE_HANDLER(NM_CUSTOMDRAW, OnNotifyCustomDraw)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
	LRESULT OnNotifyCustomDraw(int idCtrl, LPNMHDR pNotifyStruct, BOOL& /*bHandled*/);

private:
	MidiControlEngine			* mEngine;
	CLabel						* mMainDisplay;
	CEdit						* mTraceDisplay;
	std::map<int, SwitchLed*>	mLeds;
	std::map<int, SwitchTextDisplay *>		mSwitchTextDisplays;
	std::map<int, Switch *>		mSwitches;
	std::map<int, bool>			mStupidSwitchStates;
	CFont						mSwitchButtonFont;
	CFont						mTraceFont;
	int							mPreferredHeight, mPreferredWidth;
	int							mMaxSwitchId;
	WinMidiOut					mMidiOut;

	struct SwitchTextDisplayConfig
	{
		std::string				mFontname;
		int						mFontHeight;
		DWORD					mFgColor;
		DWORD					mBgColor;
		bool					mBold;
		int						mHeight;
		int						mWidth;
	};
	SwitchTextDisplayConfig		mSwitchTextDisplayConfig;

	struct SwitchConfig
	{
		std::string				mFontname;
		int						mFontHeight;
		bool					mBold;
		int						mHeight;
		int						mWidth;
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
