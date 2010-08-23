@echo off

rem root dir files
"c:\program files\7-zip\7z.exe" a -tzip mtrollsrc.zip COPYING mTrollQt.sln zip*.bat

rem recurse dirs
"c:\program files\7-zip\7z.exe" a -tzip mtrollsrc.zip data\* Engine\* midi\* Monome40h\* tinyxml\* winUtil\* -r

rem special handling
"c:\program files\7-zip\7z.exe" a -tzip mtrollsrc.zip mTrollQt\win\* -r -x!*.aps
"c:\program files\7-zip\7z.exe" a -tzip mtrollsrc.zip mTrollQt\* -xr!Debug\ -xr!debug\ -xr!Release\ -xr!release\ -xr!win\ 
"c:\program files\7-zip\7z.exe" d -tzip mtrollsrc.zip -r *.user
