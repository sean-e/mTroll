#pragma once

#include <string>


// ITraceDisplay
// ----------------------------------------------------------------------------
// use to output trace statements
//
class ITraceDisplay
{
public:
	virtual void TextOut(const std::string & txt) = 0;
};
