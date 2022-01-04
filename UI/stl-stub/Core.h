// Simple UI library.
// Copyright (C) 2022 Konstantin Nosov
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

#ifndef __CORE_H__
#define __CORE_H__

//todo: should be defined before any include like windows.h etc - see BaseDialog.cpp
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>


#undef min
#undef max

#define min(a,b)				( ((a) < (b)) ? (a) : (b) )
#define max(a,b)				( ((a) > (b)) ? (a) : (b) )
#define bound(a,minval,maxval)	( ((a) > (minval)) ? ( ((a) < (maxval)) ? (a) : (maxval) ) : (minval) )

#undef ARRAY_COUNT

#define ARRAY_ARG(array)		array, sizeof(array)/sizeof(array[0])
#define ARRAY_COUNT(array)		(sizeof(array)/sizeof(array[0]))

#if _MSC_VER
#	define FORCEINLINE			__forceinline
#	define stricmp				_stricmp
#	define strnicmp				_strnicmp
#else
#	error Unsupported compiler
#endif

#define guard(x)
#define unguard
#define unguardf(...)

#if defined(_WIN32) && defined(_DEBUG)

extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent();
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char* lpOutputString);

#endif // _WIN32 && _DEBUG


static void appPrintf(const char* Format, ...)
{
	va_list	argptr;
	va_start(argptr, Format);
	char buffer[1024];
	_vsnprintf_s(buffer, ARRAY_COUNT(buffer), Format, argptr);
	buffer[ARRAY_COUNT(buffer)-1] = 0;
	va_end(argptr);

	printf("%s", buffer);
#ifdef _DEBUG
	if (IsDebuggerPresent())
		OutputDebugStringA(buffer);
#endif
}

static void appError(const char* Format, ...)
{
	va_list	argptr;
	va_start(argptr, Format);
	char buffer[1024];
	_vsnprintf_s(buffer, ARRAY_COUNT(buffer), Format, argptr);
	va_end(argptr);

	appPrintf("%s", buffer);
#ifdef _DEBUG
	if (IsDebuggerPresent())
		__debugbreak();
#endif
	exit(1);
}

static int appSprintf(char *dest, int size, const char *Format, ...)
{
	va_list	argptr;
	va_start(argptr, Format);
	int len = _vsnprintf_s(dest, size, size, Format, argptr);
	va_end(argptr);
	return len;
}

static void appStrncpyz(char *dst, const char *src, int count)
{
	int len = (int)strlen(src);
	if (len >= count)
		len = count-1;
	strncpy(dst, src, len);
	dst[len] = 0;
}

#ifndef assert
#define assert(x)				if (!(x)) { appError("Assert: %s", #x); }
#endif

#define appNotify(...)			appPrintf("*** " __VA_ARGS__)

#endif // __CORE_H__
