// mControlUIView.cpp : implementation of the CMControlUIView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "mControlUIView.h"
#include "..\Engine\EngineLoader.h"
#include "..\Engine\MidiControlEngine.h"


CMControlUIView::CMControlUIView()
{
	EngineLoader ldr(this, this, this, this);
	mEngine = ldr.CreateEngine("test.xml");
}

CMControlUIView::~CMControlUIView()
{
	delete mEngine;
}

BOOL CMControlUIView::PreTranslateMessage(MSG* pMsg)
{
	return CWindow::IsDialogMessage(pMsg);
}


// IMainDisplay
void
CMControlUIView::TextOut(const std::string & txt)
{

}

void
CMControlUIView::ClearDisplay()
{

}

// ITraceDisplay
void
CMControlUIView::Trace(const std::string & txt)
{

}

// ISwitchDisplay
void
CMControlUIView::SetSwitchDisplay(int switchNumber, bool isOn)
{

}

bool
CMControlUIView::SupportsSwitchText() const
{
	return true;
}

void
CMControlUIView::SetSwitchText(int switchNumber, const std::string & txt)
{

}

void
CMControlUIView::ClearSwitchText(int switchNumber)
{

}

// IMidiOut
int
CMControlUIView::GetDeviceCount()
{
	return 0;
}

std::string
CMControlUIView::GetDeviceName(int deviceIdx)
{
	return "";
}

bool
CMControlUIView::OpenMidiOut(int deviceIdx)
{
	return false;
}

bool
CMControlUIView::MidiOut(const Bytes & bytes)
{
	return false;
}

void
CMControlUIView::CloseMidiOut()
{

}
