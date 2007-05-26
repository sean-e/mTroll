#pragma once

// IMainDisplay
// ----------------------------------------------------------------------------
// use to output text to the main display
//
class IMainDisplay
{
public:
	virtual void TextOut() = 0;
	virtual void Clear() = 0;
};
