#pragma once

#include <windows.h>
#include <crtdbg.h>


// Library
// ----------------------------------------------------------------------------
// 
//
class Library
{
public:
	Library() : 
		mModule(NULL)
	{ }

	Library(const char * libName) : 
		mModule(::LoadLibraryA(libName))
	{ }

	Library(const wchar_t * libName) : 
		mModule(::LoadLibraryW(libName))
	{ }

	~Library()
	{
		Unload();
	}

	template <class T>
	void GetFunction (const char * funcName, T & funPointer)
	{
		funPointer = reinterpret_cast<T>(GetProc(funcName));
	}

	bool IsLoaded() const {return mModule ? true : false;}

	bool Load(const char * libName)
	{
		Unload();
		mModule = ::LoadLibraryA(libName);
		return IsLoaded();
	}

	bool Load(const wchar_t * libName)
	{
		Unload();
		mModule = ::LoadLibraryW(libName);
		return IsLoaded();
	}

	void Unload()
	{
		if (mModule)
		{
			::FreeLibrary(mModule);
			mModule = NULL;
		}
	}

private:
	HMODULE		mModule;

	FARPROC GetProc(const char * procName)
	{
		_ASSERTE(IsLoaded());
		if (mModule)
			return ::GetProcAddress(mModule, procName);
		return NULL;
	}
};
