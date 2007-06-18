#pragma once

#include <atlmisc.h>

#define ID_SHAPE_ROUND 3001
#define ID_SHAPE_SQUARE 3002

class CLed : public CWindowImpl<CLed, CStatic>
{
public:
	CLed() :
		m_bBright(false),
		m_nShape(ID_SHAPE_ROUND)
	{
	}

	virtual ~CLed() { }

	void SetShape(int iShape)
	{
		m_nShape = iShape;
	}

	void SetColor(COLORREF on, COLORREF off = RGB(0,0,0))
	{
		m_OnColor = on;
		m_OffColor = off;
	}

	void SetOnOff(bool State)
	{
		m_bBright = State;
		RedrawWindow();
	}

private:
	// This variable is used to store the shape and color
	// set by the user for resetting the led later
	UINT m_nShape;
	BOOL m_bBright;

	COLORREF m_OnColor;
	COLORREF m_OffColor;

protected:
	BEGIN_MSG_MAP(CLed)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
	END_MSG_MAP()

	LRESULT OnPaint(UINT, WPARAM wParam, LPARAM, BOOL& bHandled)
	{
		CPaintDC dc(m_hWnd);

		// Get the rectangle of the window where we are going to draw
		CRect rcClient;
		GetClientRect(&rcClient);

		CPen penBright, penDark;
		CBrush brushBright, brushDark, brushCurrent;

		if (m_bBright==TRUE)
		{
			// If we have to switch on the LED to display the bright colour select
			// the bright pen and brush that we have created above

			penBright.CreatePen(0, 1, m_OnColor);
			brushBright.CreateSolidBrush(m_OnColor);

			dc.SelectPen(penBright);
			dc.SelectBrush(brushBright);

			brushCurrent.m_hBrush = brushBright.m_hBrush;
		}
		else
		{
			// If we have to switch off the LED to display the dark colour select
			// the bright pen and brush that we have created above

			penDark.CreatePen(0, 1, m_OffColor);
			brushDark.CreateSolidBrush(m_OffColor);

			dc.SelectPen(penDark);
			dc.SelectBrush(brushDark);

			brushCurrent.m_hBrush = brushDark.m_hBrush;
		}

		// If the round shape has been selected for the control
		if (m_nShape == ID_SHAPE_ROUND)
		{
			// Draw the actual colour of the LED
			dc.Ellipse(rcClient);

			// Draw a thick dark gray coloured circle
			CPen Pen;
			Pen.CreatePen(0,2,RGB(128,128,128));
			dc.SelectPen(Pen);
			dc.Ellipse(rcClient);

			// Draw a thin light gray coloured circle
			CPen Pen2;
			Pen2.CreatePen(0,1,RGB(192,192,192));
			dc.SelectPen(Pen2);
			dc.Ellipse(rcClient);

			// Draw a white arc at the bottom
			CPen Pen3;
			Pen3.CreatePen(0,1,RGB(255,255,255));
			dc.SelectPen(Pen3);

			// The arc function is just to add a 3D effect for the control
			CPoint ptStart, ptEnd;
			ptStart = CPoint(rcClient.Width()/2, rcClient.bottom);
			ptEnd = CPoint(rcClient.right, rcClient.top);

			dc.MoveTo(ptStart);
			dc.Arc(rcClient, ptStart, ptEnd);

			CBrush Brush;
			Brush.CreateSolidBrush(RGB(255,255,255));
			dc.SelectBrush(Brush);

			// Draw the actual ellipse
			dc.Ellipse(rcClient.left+4, rcClient.top+4, rcClient.left+6, rcClient.top+6);
		}
		else if (m_nShape == ID_SHAPE_SQUARE)
		{
			// If you have decided that your LED is going to look square in shape, then

			// Draw the actual rectangle
			dc.FillRect(rcClient, brushCurrent);

			// The code below gives a 3D look to the control. It does nothing more

			// Draw the dark gray lines
			CPen Pen;
			Pen.CreatePen(0,1,RGB(128,128,128));
			dc.SelectPen(Pen);

			dc.MoveTo(rcClient.left, rcClient.bottom);
			dc.LineTo(rcClient.left, rcClient.top);
			dc.LineTo(rcClient.right, rcClient.top);


			// Draw the light gray lines
			CPen Pen2;
			Pen2.CreatePen(0, 1, RGB(192,192,192));
			dc.SelectPen(Pen2);

			dc.MoveTo(rcClient.right, rcClient.top);
			dc.LineTo(rcClient.right, rcClient.bottom);
			dc.LineTo(rcClient.left, rcClient.bottom);
		}

		return 0;
	}
};
