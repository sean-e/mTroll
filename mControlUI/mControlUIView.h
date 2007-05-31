// mControlUIView.h : interface of the CMControlUIView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "..\Engine\IMainDisplay.h"
#include "..\Engine\IMidiOut.h"
#include "..\Engine\ISwitchDisplay.h"
#include "..\Engine\ITraceDisplay.h"

class MidiControlEngine;


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
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
private:
	MidiControlEngine * mEngine;
};
