#ifndef __WIN32_TYPES_H__
#define __WIN32_TYPES_H__

#if _WIN32
// avoid include of windows.h
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
