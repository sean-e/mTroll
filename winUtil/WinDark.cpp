#include "WinDark.h"
#include <dwmapi.h>

#pragma comment (lib, "Dwmapi.lib")

namespace WinDark
{
	// This is block from GitHub user: https://github.com/statiolake
	// via commit: https://github.com/statiolake/neovim-qt/commit/da8eaba7f0e38b6b51f3bacd02a8cc2d1f7a34d8

	enum PreferredAppMode
	{
		Default,
		AllowDark,
		ForceDark,
		ForceLight,
		Max
	};

	enum WINDOWCOMPOSITIONATTRIB
	{
		WCA_UNDEFINED = 0,
		WCA_NCRENDERING_ENABLED = 1,
		WCA_NCRENDERING_POLICY = 2,
		WCA_TRANSITIONS_FORCEDISABLED = 3,
		WCA_ALLOW_NCPAINT = 4,
		WCA_CAPTION_BUTTON_BOUNDS = 5,
		WCA_NONCLIENT_RTL_LAYOUT = 6,
		WCA_FORCE_ICONIC_REPRESENTATION = 7,
		WCA_EXTENDED_FRAME_BOUNDS = 8,
		WCA_HAS_ICONIC_BITMAP = 9,
		WCA_THEME_ATTRIBUTES = 10,
		WCA_NCRENDERING_EXILED = 11,
		WCA_NCADORNMENTINFO = 12,
		WCA_EXCLUDED_FROM_LIVEPREVIEW = 13,
		WCA_VIDEO_OVERLAY_ACTIVE = 14,
		WCA_FORCE_ACTIVEWINDOW_APPEARANCE = 15,
		WCA_DISALLOW_PEEK = 16,
		WCA_CLOAK = 17,
		WCA_CLOAKED = 18,
		WCA_ACCENT_POLICY = 19,
		WCA_FREEZE_REPRESENTATION = 20,
		WCA_EVER_UNCLOAKED = 21,
		WCA_VISUAL_OWNER = 22,
		WCA_HOLOGRAPHIC = 23,
		WCA_EXCLUDED_FROM_DDA = 24,
		WCA_PASSIVEUPDATEMODE = 25,
		WCA_USEDARKMODECOLORS = 26,
		WCA_LAST = 27
	};

	struct WINDOWCOMPOSITIONATTRIBDATA
	{
		WINDOWCOMPOSITIONATTRIB Attrib;
		PVOID pvData;
		SIZE_T cbData;
	};

	using fnAllowDarkModeForWindow = BOOL(WINAPI *)(HWND hWnd, BOOL allow);
	using fnSetPreferredAppMode = PreferredAppMode(WINAPI *)(PreferredAppMode appMode);
	using fnSetWindowCompositionAttribute = BOOL(WINAPI *)(HWND hwnd, WINDOWCOMPOSITIONATTRIBDATA *);

	void setDarkTitlebar(HWND hwnd, bool enableDark)
	{
		HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
		if (!hUxtheme)
			return;

		HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
		if (!hUser32)
			return;

		fnAllowDarkModeForWindow AllowDarkModeForWindow = reinterpret_cast<fnAllowDarkModeForWindow>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133)));
		fnSetPreferredAppMode SetPreferredAppMode = reinterpret_cast<fnSetPreferredAppMode>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135)));
		fnSetWindowCompositionAttribute SetWindowCompositionAttribute = reinterpret_cast<fnSetWindowCompositionAttribute>(GetProcAddress(hUser32, "SetWindowCompositionAttribute"));
		if (!AllowDarkModeForWindow || !SetPreferredAppMode || !SetWindowCompositionAttribute)
			return;

		SetPreferredAppMode(enableDark ? ForceDark : ForceLight);
		BOOL dark = enableDark ? TRUE : FALSE;
		AllowDarkModeForWindow(hwnd, dark);
		WINDOWCOMPOSITIONATTRIBDATA data =
		{
			WCA_USEDARKMODECOLORS,
			&dark,
			sizeof(dark)
		};
		SetWindowCompositionAttribute(hwnd, &data);
	}
}
