// mControlUIView.h : interface of the CMControlUIView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "..\Engine\IMainDisplay.h"
#include "..\Engine\IMidiOut.h"
#include "..\Engine\ISwitchDisplay.h"
#include "..\Engine\ITraceDisplay.h"
#include "..\Engine\MidiControlEngine.h"
#include <atlctrls.h>


class CMControlUIView : public CDialogImpl<CMControlUIView>, 
								IMainDisplay, 
								IMidiOut, 
								ISwitchDisplay, 
								ITraceDisplay
{
public:
	CMControlUIView();
	~CMControlUIView();

	enum { IDD = IDD_MCONTROLUI_FORM };

	HWND Create(HWND hWndParent, LPARAM dwInitParam = NULL);
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

	// IMidiOut
	virtual int GetDeviceCount();
	virtual std::string GetDeviceName(int deviceIdx);
	virtual bool OpenMidiOut(int deviceIdx);
	virtual bool MidiOut(const Bytes & bytes);
	virtual void CloseMidiOut();

	BEGIN_MSG_MAP(CMControlUIView)
		MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
// 		COMMAND_HANDLER(IDC_SWITCH1, BN_PUSHED, OnBnPushed1)
// 		COMMAND_HANDLER(IDC_SWITCH2, BN_PUSHED, OnBnPushed2)
// 		COMMAND_HANDLER(IDC_SWITCH3, BN_PUSHED, OnBnPushed3)
// 		COMMAND_HANDLER(IDC_SWITCH4, BN_PUSHED, OnBnPushed4)
// 		COMMAND_HANDLER(IDC_SWITCH5, BN_PUSHED, OnBnPushed5)
// 		COMMAND_HANDLER(IDC_SWITCH6, BN_PUSHED, OnBnPushed6)
// 		COMMAND_HANDLER(IDC_SWITCH7, BN_PUSHED, OnBnPushed7)
// 		COMMAND_HANDLER(IDC_SWITCH8, BN_PUSHED, OnBnPushed8)
// 		COMMAND_HANDLER(IDC_SWITCH9, BN_PUSHED, OnBnPushed9)
// 		COMMAND_HANDLER(IDC_SWITCH10, BN_PUSHED, OnBnPushed10)
// 		COMMAND_HANDLER(IDC_SWITCH11, BN_PUSHED, OnBnPushed11)
// 		COMMAND_HANDLER(IDC_SWITCH12, BN_PUSHED, OnBnPushed12)
// 		COMMAND_HANDLER(IDC_SWITCH13, BN_PUSHED, OnBnPushed13)
// 		COMMAND_HANDLER(IDC_SWITCH14, BN_PUSHED, OnBnPushed14)
// 		COMMAND_HANDLER(IDC_SWITCH15, BN_PUSHED, OnBnPushed15)
// 		COMMAND_HANDLER(IDC_SWITCH16, BN_PUSHED, OnBnPushed16)
// 
// 		COMMAND_HANDLER(IDC_SWITCH1, BN_UNPUSHED, OnBnUnpushed1)
// 		COMMAND_HANDLER(IDC_SWITCH2, BN_UNPUSHED, OnBnUnpushed2)
// 		COMMAND_HANDLER(IDC_SWITCH3, BN_UNPUSHED, OnBnUnpushed3)
// 		COMMAND_HANDLER(IDC_SWITCH4, BN_UNPUSHED, OnBnUnpushed4)
// 		COMMAND_HANDLER(IDC_SWITCH5, BN_UNPUSHED, OnBnUnpushed5)
// 		COMMAND_HANDLER(IDC_SWITCH6, BN_UNPUSHED, OnBnUnpushed6)
// 		COMMAND_HANDLER(IDC_SWITCH7, BN_UNPUSHED, OnBnUnpushed7)
// 		COMMAND_HANDLER(IDC_SWITCH8, BN_UNPUSHED, OnBnUnpushed8)
// 		COMMAND_HANDLER(IDC_SWITCH9, BN_UNPUSHED, OnBnUnpushed9)
// 		COMMAND_HANDLER(IDC_SWITCH10, BN_UNPUSHED, OnBnUnpushed10)
// 		COMMAND_HANDLER(IDC_SWITCH11, BN_UNPUSHED, OnBnUnpushed11)
// 		COMMAND_HANDLER(IDC_SWITCH12, BN_UNPUSHED, OnBnUnpushed12)
// 		COMMAND_HANDLER(IDC_SWITCH13, BN_UNPUSHED, OnBnUnpushed13)
// 		COMMAND_HANDLER(IDC_SWITCH14, BN_UNPUSHED, OnBnUnpushed14)
// 		COMMAND_HANDLER(IDC_SWITCH15, BN_UNPUSHED, OnBnUnpushed15)
// 		COMMAND_HANDLER(IDC_SWITCH16, BN_UNPUSHED, OnBnUnpushed16)
	END_MSG_MAP()

public:
// 	LRESULT OnBnPushed1(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchPressed(0); return 0;}
// 	LRESULT OnBnPushed2(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchPressed(1); return 0;}
// 	LRESULT OnBnPushed3(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchPressed(2); return 0;}
// 	LRESULT OnBnPushed4(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchPressed(3); return 0;}
// 	LRESULT OnBnPushed5(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchPressed(4); return 0;}
// 	LRESULT OnBnPushed6(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchPressed(5); return 0;}
// 	LRESULT OnBnPushed7(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchPressed(6); return 0;}
// 	LRESULT OnBnPushed8(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchPressed(7); return 0;}
// 	LRESULT OnBnPushed9(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchPressed(8); return 0;}
// 	LRESULT OnBnPushed10(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchPressed(9); return 0;}
// 	LRESULT OnBnPushed11(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchPressed(10); return 0;}
// 	LRESULT OnBnPushed12(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchPressed(11); return 0;}
// 	LRESULT OnBnPushed13(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchPressed(12); return 0;}
// 	LRESULT OnBnPushed14(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchPressed(13); return 0;}
// 	LRESULT OnBnPushed15(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchPressed(14); return 0;}
// 	LRESULT OnBnPushed16(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchPressed(15); return 0;}
// 
// 	LRESULT OnBnUnpushed1(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchReleased(0); return 0;}
// 	LRESULT OnBnUnpushed2(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchReleased(1); return 0;}
// 	LRESULT OnBnUnpushed3(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchReleased(2); return 0;}
// 	LRESULT OnBnUnpushed4(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchReleased(3); return 0;}
// 	LRESULT OnBnUnpushed5(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchReleased(4); return 0;}
// 	LRESULT OnBnUnpushed6(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchReleased(5); return 0;}
// 	LRESULT OnBnUnpushed7(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchReleased(6); return 0;}
// 	LRESULT OnBnUnpushed8(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchReleased(7); return 0;}
// 	LRESULT OnBnUnpushed9(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchReleased(8); return 0;}
// 	LRESULT OnBnUnpushed10(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchReleased(9); return 0;}
// 	LRESULT OnBnUnpushed11(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchReleased(10); return 0;}
// 	LRESULT OnBnUnpushed12(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchReleased(11); return 0;}
// 	LRESULT OnBnUnpushed13(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchReleased(12); return 0;}
// 	LRESULT OnBnUnpushed14(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchReleased(13); return 0;}
// 	LRESULT OnBnUnpushed15(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchReleased(14); return 0;}
// 	LRESULT OnBnUnpushed16(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) { mEngine->SwitchReleased(15); return 0;}

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
	LRESULT OnDrawItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
private:
	MidiControlEngine	* mEngine;
	CProgressBarCtrl	mLeds[16];
	CEdit				mSwitchTextDisplays[16];
	CButton				mSwitches[16];
	CEdit				mMainDisplay;
	CEdit				mTraceDisplay;
	bool				mStupidSwitchStates[16];
};
