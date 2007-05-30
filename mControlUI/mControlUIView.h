// mControlUIView.h : interface of the CMControlUIView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

class CMControlUIView : public CDialogImpl<CMControlUIView>
{
public:
	enum { IDD = IDD_MCONTROLUI_FORM };

	BOOL PreTranslateMessage(MSG* pMsg);

	BEGIN_MSG_MAP(CMControlUIView)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
};
