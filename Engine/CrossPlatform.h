#ifndef CrossPlatform_h__
#define CrossPlatform_h__

#ifdef WIN32
#include <windows.h>
#else
#include <cstdio>
#include <strings.h>
#endif

#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>


#ifndef WIN32
// Cross-platform replacements for Windows types and color functions
using DWORD = unsigned int;
using BYTE = unsigned char;

inline BYTE GetRValue(unsigned int rgb) { return (BYTE)(rgb & 0xFF); }
inline BYTE GetGValue(unsigned int rgb) { return (BYTE)((rgb >> 8) & 0xFF); }
inline BYTE GetBValue(unsigned int rgb) { return (BYTE)((rgb >> 16) & 0xFF); }

inline unsigned int RGB(BYTE r, BYTE g, BYTE b) { return (unsigned int)(r | (g << 8) | (b << 16)); }
#endif


namespace xp
{
	// Cross-platform replacement for _itoa_s
	inline int _itoa_s(int value, char* buffer, size_t size, int radix)
	{
#ifdef WIN32
		return ::_itoa_s(value, buffer, size, radix);
#else
		if (radix == 10)
			snprintf(buffer, size, "%d", value);
		else if (radix == 16)
			snprintf(buffer, size, "%x", value);
		else
			return -1; // Unsupported radix
		return 0;
#endif
	}

	// Cross-platform replacement for strcat_s
	inline int strcat_s(char* dest, size_t dest_size, const char* src)
	{
#ifdef WIN32
		return ::strcat_s(dest, dest_size, src);
#else
		size_t dest_len = strlen(dest);
		size_t src_len = strlen(src);

		if (dest_len + src_len + 1 > dest_size)
			return -1; // Buffer too small

		strcat(dest, src);
		return 0;
#endif
	}

	// Cross-platform replacement for Win32 GetTickCount (returns milliseconds)
	// time in ms (used to measure elapsed time between events, origin doesn't matter)
	inline unsigned int CurTime()
	{
		auto now = std::chrono::steady_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
		return static_cast<unsigned int>(duration.count());
	}

	// Cross-platform replacement for Win32 Sleep (takes milliseconds)
	inline void Sleep(unsigned int milliseconds)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
	}

	// Cross-platform replacement for _stricmp
	inline int _stricmp(const char* str1, const char* str2)
	{
#ifdef WIN32
		return ::_stricmp(str1, str2);
#else
		return strcasecmp(str1, str2);
#endif
	}
}

// Cross-platform replacement for _ASSERTE (if not already defined)
#ifndef _ASSERTE
#ifdef NDEBUG
#define _ASSERTE(expr) ((void)0)
#else
#include <cassert>
#define _ASSERTE(expr) assert(expr)
#endif
#endif

#endif // CrossPlatform_h__
