/*
#if !defined(AFX_DYNAMICLED_H__7AA00BEC_B6E4_48A7_9719_3A15C0AB217A__INCLUDED_)
#define AFX_DYNAMICLED_H__7AA00BEC_B6E4_48A7_9719_3A15C0AB217A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DynamicLED.h : header file
//
#define ID_LED_RED 2001
#define ID_LED_GREEN 2002
#define ID_LED_BLUE 2003

/////////////////////////////////////////////////////////////////////////////
// CDynamicLED window

class CDynamicLED : public CStatic
{
// Construction
public:
	CDynamicLED();

	// The constants needed to define the colours of the LED
	#define ID_LED_RED		2001
	#define ID_LED_GREEN	2002
	#define ID_LED_BLUE		2003
	#define ID_LED_YELLOW	2004

	// The constants needed to define the shape of the LED
	#define ID_SHAPE_ROUND	3001
	#define ID_SHAPE_SQUARE	3002
	
// Attributes
protected:
	
	// The pens and brushes needed to do the drawing
	CPen m_PenBright,m_PenDark;
	CBrush m_BrushBright,m_BrushDark,m_BrushCurrent;
	
	// This variable is used to store the shape and color
	// set by the user for resetting the led later
	UINT m_nID;
	UINT m_nShape;

	BOOL m_bBright;

	CWnd *m_pWnd;

	int m_nTimerInterval,m_nPrevTimerInterval;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDynamicLED)
	//}}AFX_VIRTUAL

// Implementation
public:
	void SwitchOn();
	void SwitchOff();

	void SetLED(CWnd *pWnd,UINT nIDColor, UINT nIDShape, int nTimerInterval);
	
	virtual ~CDynamicLED();

private:
	// This function cannot be called directly from your dialog application.
	// Its used internally by this class to reset the LED again.

	void ResetLED(UINT nIDColor, UINT nIDShape, int nTimerInterval);

	// Generated message map functions
protected:
	//{{AFX_MSG(CDynamicLED)
	afx_msg void OnPaint();
	afx_msg void OnTimer(UINT nIDEvent);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DYNAMICLED_H__7AA00BEC_B6E4_48A7_9719_3A15C0AB217A__INCLUDED_)

*/



#if !defined(AFX_DYNAMICLED_H__7AA00BEC_B6E4_48A7_9719_3A15C0AB217A__INCLUDED_)
#define AFX_DYNAMICLED_H__7AA00BEC_B6E4_48A7_9719_3A15C0AB217A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DynamicLED.h : header file
//
#include "stdafx.h"

#define ID_SHAPE_ROUND 3001
#define ID_SHAPE_SQUARE 3002

/////////////////////////////////////////////////////////////////////////////
// CDynamicLED window
class CDynamicLED : public CStatic
{
// Construction
public:
CDynamicLED();
void SetColor(COLORREF on,COLORREF off = RGB(0,0,0));
void SetBlink(int iTime_in_ms); // 0 means: blink off
void SetOnOff(bool State);
virtual ~CDynamicLED();
void SetShape(int iShape);

private:

// The pens and brushes needed to do the drawing
CPen m_PenBright,m_PenDark;
CBrush m_BrushBright,m_BrushDark,m_BrushCurrent;

// This variable is used to store the shape and color
// set by the user for resetting the led later
UINT m_nShape;
BOOL m_bBright;

// Operations
public:

// Overrides
// ClassWizard generated virtual function overrides
//{{AFX_VIRTUAL(CDynamicLED)
//}}AFX_VIRTUAL



private:
unsigned int m_TimerHandle;
COLORREF m_OnColor;
COLORREF m_OffColor;
// Generated message map functions
protected:
//{{AFX_MSG(CDynamicLED)
afx_msg void OnPaint();
afx_msg void OnTimer(UINT nIDEvent);
//}}AFX_MSG

DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DYNAMICLED_H__7AA00BEC_B6E4_48A7_9719_3A15C0AB217A__INCLUDED_)

