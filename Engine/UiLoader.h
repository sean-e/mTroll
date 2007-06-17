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
	bool					LoadAssembyConfig(TiXmlElement * pElem);
	void					LoadSwitchAssemblies(TiXmlElement * pElem);
	void					LoadSwitchAssembly(TiXmlElement * pElem);
	void					LoadOtherStuffAndFinalize(TiXmlElement * pElem);

	IMidiControlUi			* mUi;
};

#endif // UiLoader_h__
