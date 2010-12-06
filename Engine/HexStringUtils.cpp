/*
Original code copyright (c) 2007-2009,2010 Sean Echevarria ( http://www.creepingfog.com/sean/ )

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

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <assert.h>
#include "HexStringUtils.h"


byte 
CharToByte(const char ch)
{
	switch (ch) {
	case '0':		return 0;
	case '1':		return 1;
	case '2':		return 2;
	case '3':		return 3;
	case '4':		return 4;
	case '5':		return 5;
	case '6':		return 6;
	case '7':		return 7;
	case '8':		return 8;
	case '9':		return 9;
	case 'a':
	case 'A':
		return 10;
	case 'b':
	case 'B':
		return 11;
	case 'c':
	case 'C':
		return 12;
	case 'd':
	case 'D':
		return 13;
	case 'e':
	case 'E':
		return 14;
	case 'f':
	case 'F':
		return 15;
	default:
		assert(0);
		return 0;
	}
}

byte 
CharsToByte(const char ch1, 
			const char ch2)
{
	byte retval;
	retval = CharToByte(ch1);
	retval *= 16;
	retval = (byte)(retval + CharToByte(ch2));
	return retval;
}

byte
CharsToByte(const char chrs[2])
{
	return CharsToByte(chrs[0], chrs[1]);
}

int 
ValidateString(const std::string & inString, 
			   Bytes & outBytes)
{
	outBytes.clear();
	const int len = inString.length();
	if (!len)
		return 0;

	outBytes.reserve(len / 2);

	for (int idx = 0; idx < len;)
	{
		char ch = inString[idx++];
		if (isspace(ch))
			continue;
		if (!isxdigit(ch) || idx == len)
			return -1;
		
		ch = inString[idx++];
		if (!isxdigit(ch))
			return -1;

		outBytes.push_back(CharsToByte(inString[idx-2], ch));
		
		while (idx < len)
		{
			ch = inString[idx];
			if (isspace(ch))
				idx++;
			else 
			{
				if (isxdigit(ch))
					break;
				else
					return -1;
			}
		}
	}

	return outBytes.size();
}

std::string 
GetAsciiHexStr(const Bytes & inBytes, 
			   bool format /*= true*/)
{
	const int len = inBytes.size();
	return GetAsciiHexStr(&inBytes[0], inBytes.size(), format);
}

std::string
GetAsciiHexStr(const byte * inBytes, size_t sz, bool format /*= true*/)
{
	std::string tmp, retval;
	for (size_t idx = 0; idx < sz; idx++)
	{
		char tmp[6];
		sprintf(tmp, "%02x", inBytes[idx]);
		retval += tmp;

		if (format)
		{
			if ((idx+1) % 16)
				retval += " ";
			else
				retval += "\n";
		}
	}

	return retval;
}

std::string
GetAsciiStr(const byte * inBytes, size_t sz)
{
	std::string tmp, retval;
	for (size_t idx = 0; idx < sz && inBytes[idx]; idx++)
	{
		char tmp[2];
		sprintf(tmp, "%c", inBytes[idx]);
		retval += tmp;
	}

	return retval;
}
