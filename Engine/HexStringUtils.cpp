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
ValidateString(std::string inString, 
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
	std::string tmp, retval;
	for (int idx = 0; idx < len; idx++)
	{
		char tmp[6];
		sprintf(tmp, "%02x", inBytes[idx]);
		retval += tmp;

		if (format)
		{
			if ((idx+1) % 8)
				retval += " ";
			else
				retval += "\r\n";
		}
	}

	return retval;
}
