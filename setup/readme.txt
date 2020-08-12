mTrollSetup.iss is an installer project file for Inno Setup

Update mTrollVersion.txt before generating an updated installer.
The date in the file will be used both by the installer and to name the produced exe.

Populate the ..\ReleaseDependencies\ directory with the following Qt dlls:
..\ReleaseDependencies\platforms\qwindows.dll
..\ReleaseDependencies\Qt5Core.dll
..\ReleaseDependencies\Qt5Gui.dll
..\ReleaseDependencies\Qt5Widgets.dll

Populate the ..\ReleaseDependencies\ directory with the following VC runtime dlls:
..\ReleaseDependencies\concrt140.dll
..\ReleaseDependencies\msvcp140.dll
..\ReleaseDependencies\vcruntime140.dll
