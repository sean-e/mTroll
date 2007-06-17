#ifndef IMidiControlUi_h__
#define IMidiControlUi_h__

#include <string>


// IMidiControlUi
// ----------------------------------------------------------------------------
// use to load xml based ui settings
//
class IMidiControlUi
{
public:
	virtual void	CreateSwitchLed(int id, int top, int left, int width, int height) = 0;

	virtual void	CreateSwitchFont(const std::string & fontName, int fontHeight, bool bold) = 0;
	virtual void	CreateSwitch(int id, const std::string & label, int top, int left, int width, int height) = 0;

	virtual void	SetSwitchDisplayFontSettings(const std::string & fontName, int fontHeight, bool bold, unsigned int bgColor, unsigned int fgColor) = 0;
	virtual void	CreateSwitchTextDisplay(int id, int top, int left, int width, int height) = 0;

	virtual void	CreateMainDisplay(int top, int left, int width, int height, const std::string & fontName, int fontHeight, bool bold, unsigned int bgColor, unsigned int fgColor) = 0;
	virtual void	CreateTraceDisplay(int top, int left, int width, int height, const std::string & fontName, int fontHeight, bool bold) = 0;
	virtual void	CreateStaticLabel(const std::string & label, int top, int left, int width, int height, const std::string & fontName, int fontHeight, bool bold, unsigned int bgColor, unsigned int fgColor) = 0;

	virtual void	SetMainSize(int width, int height) = 0;
};

#endif // IMidiControlUi_h__
