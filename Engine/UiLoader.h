#ifndef UiLoader_h__
#define UiLoader_h__

#include <string>

class TiXmlElement;
class IMidiControlUi;


class UiLoader
{
public:
	UiLoader(IMidiControlUi * ui, const std::string & settingsFile);

private:
	void					LoadSwitchLeds(TiXmlElement * pElem);
	void					LoadSwitches(TiXmlElement * pElem);
	void					LoadSwitchDisplays(TiXmlElement * pElem);
	void					LoadOtherStuffAndFinalize(TiXmlElement * pElem);

	IMidiControlUi			* mUi;
};

#endif // UiLoader_h__
