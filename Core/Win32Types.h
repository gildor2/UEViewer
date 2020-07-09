#ifndef __WIN32_TYPES_H__
#define __WIN32_TYPES_H__

// This file contains all Windows type definitions which are used in project headers.
// It allows to avoid doing #include <windows.h> in all source files, that header should
// be included only when needed. If some cpp file has #include for both windows.h and this
// file, windows.h should be included first, so definitions in this header will be simply
// ignored. Otherwise type conflicts would appear.

// Good reference: UE4/Engine/Source/Runtime/Core/Public/Windows/MinimalWindowsApi.h

#if _WIN32
#	ifndef APIENTRY
#		define APIENTRY __stdcall
#	endif
#	ifndef WINAPI
#		define WINAPI   __stdcall
#	endif
#	ifndef CALLBACK
#		define CALLBACK __stdcall
#	endif
#	ifndef WINGDIAPI
#		define WINGDIAPI
		typedef void*			HANDLE;
	#ifndef _WIN64
		typedef int				INT_PTR;
		typedef unsigned int	UINT_PTR;
		typedef long			LONG_PTR;
		typedef unsigned long	ULONG_PTR;
	#else
		typedef __int64			INT_PTR;
		typedef unsigned __int64 UINT_PTR;
		typedef __int64			LONG_PTR;
		typedef unsigned __int64 ULONG_PTR;
	#endif
		typedef unsigned		HDC;
		typedef HANDLE			HGLRC;
		typedef const char *	LPCSTR;
		typedef int				BOOL;
		typedef unsigned char	BYTE;
		typedef unsigned short	WORD;
		typedef unsigned int	DWORD;
		typedef unsigned int	UINT;
		typedef HANDLE			HWND;
		typedef HANDLE			HMENU;
		typedef UINT_PTR		WPARAM;
		typedef LONG_PTR		LPARAM;
		typedef int (APIENTRY *PROC)();
		typedef void PIXELFORMATDESCRIPTOR;		// structure
		typedef PIXELFORMATDESCRIPTOR * LPPIXELFORMATDESCRIPTOR;
#	endif // WINGDIAPI
#	ifndef CONST
#		define CONST const
#	endif
#endif // _WIN32

#endif // __WIN32_TYPES_H__
