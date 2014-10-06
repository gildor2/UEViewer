#ifndef __WIN32_TYPES_H__
#define __WIN32_TYPES_H__

// This file contains all Windows type definitions which are used in project headers.
// It allows to avoid doing #include <windows.h> in all source files, that header should
// be included only when needed. If some cpp file has #include for both windows.h and this
// file, windows.h should be included first, so definitions in this header will be simply
// ignored. Otherwise type conflicts would appear.

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
#		ifdef _WIN64
#			error Review these types!
#		endif
		typedef unsigned		HDC;
		typedef unsigned		HGLRC;
		typedef const char *	LPCSTR;
		typedef int				BOOL;
		typedef unsigned char	BYTE;
		typedef unsigned short	WORD;
		typedef unsigned int	DWORD;
		typedef unsigned int	UINT;
		typedef void*			HWND;
		typedef void*			HMENU;
		typedef size_t			WPARAM;
		typedef long			LPARAM;
		typedef int				INT_PTR;
		typedef int (APIENTRY *PROC)();
		typedef void PIXELFORMATDESCRIPTOR;		// structure
		typedef PIXELFORMATDESCRIPTOR * LPPIXELFORMATDESCRIPTOR;
#	endif // WINGDIAPI
#	ifndef CONST
#		define CONST const
#	endif
#endif // _WIN32

#endif // __WIN32_TYPES_H__
