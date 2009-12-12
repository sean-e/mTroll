/*
Original code copyright (c) 2007-2009 Sean Echevarria ( http://www.creepingfog.com/sean/ )

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/

#ifndef HexStringUtils_h__
#define HexStringUtils_h__

#include <string>
#include <vector>

typedef unsigned char byte;
typedef std::vector<byte> Bytes;


byte CharToByte(const char ch);
byte CharsToByte(const char ch1, const char ch2);
byte CharsToByte(const char chrs[2]);
int ValidateString(const std::string & inString, Bytes & outBytes);
std::string GetAsciiHexStr(const Bytes & inBytes, bool format /*= true*/);

#endif // HexStringUtils_h__
