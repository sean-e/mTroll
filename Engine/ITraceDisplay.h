#ifndef ITraceDisplay_h__
#define ITraceDisplay_h__

#include <string>


// ITraceDisplay
// ----------------------------------------------------------------------------
// use to output trace statements
//
class ITraceDisplay
{
public:
	virtual void Trace(const std::string & txt) = 0;
};

#endif // ITraceDisplay_h__
