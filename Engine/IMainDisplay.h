#pragma once

#include <string>


// IMainDisplay
// ----------------------------------------------------------------------------
// use to output text to the main display
//
class IMainDisplay
{
public:
	virtual void TextOut(const std::string & txt) = 0;
	virtual void Clear() = 0;
};
