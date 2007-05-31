#ifndef IMainDisplay_h__
#define IMainDisplay_h__

#include <string>


// IMainDisplay
// ----------------------------------------------------------------------------
// use to output text to the main display
//
class IMainDisplay
{
public:
	virtual void TextOut(const std::string & txt) = 0;
	virtual void ClearDisplay() = 0;
};

#endif // IMainDisplay_h__
