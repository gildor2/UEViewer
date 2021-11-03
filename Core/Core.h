#ifndef __CORE_H__
#define __CORE_H__

#if _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#if _MSC_VER
#	include <intrin.h>
#	include <excpt.h>
#endif

#if __GNUC__
#	include <wchar.h>
#	include <stdint.h>
#endif

#ifdef TRACY_ENABLE
#	undef max                     // defined somewhere in C headers, we'll redefine it below anyway
#	define __PLACEMENT_NEW_INLINE // prevent inclusion of VC "operator new"
#	include <tracy/Tracy.hpp>     // Include Tracy.hpp header
#endif

#include "Build.h"

#if RENDERING
#	define SDL_MAIN_HANDLED			// prevent overriding of 'main' function on Windows
#	include <SDL2/SDL.h>			//?? move outside (here for SDL_GetTicks() only?)
#endif

#define VECTOR_ARG(v)			(v).X, (v).Y, (v).Z
#define QUAT_ARG(v)				(v).X, (v).Y, (v).Z, (v).W
#define ARRAY_ARG(array)		array, sizeof(array)/sizeof(array[0])
#define ARRAY_COUNT(array)		(sizeof(array)/sizeof(array[0]))

// use "STR(any_value)" to convert it to string (may be float value)
#define STR2(s) #s
#define STR(s) STR2(s)

#define BYTES4(a,b,c,d)	((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))


#define DO_ASSERT				1


#if MAX_DEBUG

// override some settings with MAX_DEBUG option
#undef  DO_ASSERT
#define DO_ASSERT				1
#undef  DO_GUARD
#define DO_GUARD				1
#undef  DO_GUARD_MAX
#define DO_GUARD_MAX			1
#undef  DEBUG_MEMORY
#define DEBUG_MEMORY			1
#undef  VSTUDIO_INTEGRATION
#define VSTUDIO_INTEGRATION		1

#if _MSC_VER
#pragma optimize("", off)
#endif

#endif // MAX_DEBUG


#undef assert

#if DO_ASSERT
#define assert(x)	\
	if (!(x))		\
	{				\
		appError("assertion failed: %s\n", #x); \
	}
#else
#define assert(x)
#endif // DO_ASSERT


#undef M_PI
#define M_PI					(3.14159265358979323846)


#undef min
#undef max

#define min(a,b)				( ((a) < (b)) ? (a) : (b) )
#define max(a,b)				( ((a) > (b)) ? (a) : (b) )
#define bound(a,minval,maxval)	( ((a) > (minval)) ? ( ((a) < (maxval)) ? (a) : (maxval) ) : (minval) )

#define appFloor(x)				( (int)floor(x) )
#define appCeil(x)				( (int)ceil(x)  )
#define appRound(x)				( (int) (x >= 0 ? (x)+0.5f : (x)-0.5f) )


#if _MSC_VER

#	define vsnprintf			_vsnprintf
#	define vsnwprintf			_vsnwprintf
#	define FORCEINLINE			__forceinline
#	define NORETURN				__declspec(noreturn)
#	define stricmp				_stricmp
#	define strnicmp				_strnicmp
#	define GCC_PACK							// VC uses #pragma pack()
#	if _MSC_VER >= 1400
#		define IS_POD(T)		__is_pod(T)
#	endif
#	define FORMAT_SIZE(fmt)		"%I" fmt
//#	pragma warning(disable : 4291)			// no matched operator delete found
#	pragma warning(disable : 4100)			// unreferenced formal parameter
#	pragma warning(disable : 4127)			// conditional expression is constant
#	pragma warning(disable : 4509)			// nonstandard extension used: '..' uses SEH and '..' has destructor
#	pragma warning(disable : 4714)			// function '...' marked as __forceinline not inlined
	// this functions are smaller, when in intrinsic form (and, of course, faster):
#	pragma intrinsic(memcpy, memset, memcmp, abs, fabs, _rotl8, _rotl, _rotr8, _rotr)
	// allow nested inline expansions
#	pragma inline_depth(8)
#	define WIN32_USE_SEH		1
#	define ROL8(val,shift)		_rotl8(val,shift)
#	define ROR8(val,shift)		_rotr8(val,shift)
#	define ROL16(val,shift)		_rotl16(val,shift)
#	define ROR16(val,shift)		_rotr16(val,shift)
#	define ROL32(val,shift)		_rotl(val,shift)
#	define ROR32(val,shift)		_rotr(val,shift)

#	define appDebugBreak		__debugbreak

typedef __int64					int64;
typedef unsigned __int64		uint64;

#elif __GNUC__

#	define vsnwprintf			swprintf
#	define __FUNCSIG__			__PRETTY_FUNCTION__
#	define NORETURN				__attribute__((noreturn))
#	if (__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 2))
	// strange, but there is only way to work (inline+always_inline)
#		define FORCEINLINE		inline __attribute__((always_inline))
#	else
#		define FORCEINLINE		inline
#	endif
#	if (__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 4))
#		define IS_POD(T)		__is_pod(T)
#	endif
#	define stricmp				strcasecmp
#	define strnicmp				strncasecmp
#	define GCC_PACK				__attribute__((__packed__))
#	define FORMAT_SIZE(fmt)		"%z" fmt
#	undef VSTUDIO_INTEGRATION
#	undef WIN32_USE_SEH

// Previous definitions:
//   typedef signed long long		int64;
//   typedef unsigned long long		uint64;
// However this fails conversion from size_t to uint64 in gcc and clang, so we're new using standard types from 'stdint.h'
typedef int64_t					int64;
typedef uint64_t				uint64;

#else

#	error "Unsupported compiler"

#endif

// necessary types
typedef unsigned char			byte;

// integer types of particular size (just for easier code understanding in some places)
typedef signed char				int8;
typedef unsigned char			uint8;			// byte
typedef signed short			int16;
typedef unsigned short			uint16;			// word
typedef signed int				int32;
typedef unsigned int			uint32;

typedef size_t					address_t;


#ifndef IS_POD
#	define IS_POD(T)			false
#endif

// cyclical shift operations
#ifndef ROL8
#define ROL8(val,shift)			( ((val) << (shift)) | ((val) >> (8-(shift))) )
#endif

#ifndef ROR8
#define ROR8(val,shift)			( ((val) >> (shift)) | ((val) << (8-(shift))) )
#endif

#ifndef ROL16
#define ROL16(val,shift)		( ((val) << (shift)) | ((val) >> (16-(shift))) )
#endif

#ifndef ROR16
#define ROR16(val,shift)		( ((val) >> (shift)) | ((val) << (16-(shift))) )
#endif

#ifndef ROL32
#define ROL32(val,shift)		( (unsigned(val) << (shift)) | (unsigned(val) >> (32-(shift))) )
#endif

#ifndef ROR32
#define ROR32(val,shift)		( (unsigned(val) >> (shift)) | (unsigned(val) << (32-(shift))) )
#endif


// Using size_t typecasts - that's platform integer type
template<class T> inline T OffsetPointer(const T ptr, int offset)
{
	return (T) ((size_t)ptr + offset);
}

// Align integer or pointer of any type
template<class T> inline T Align(const T ptr, int alignment)
{
	return (T) (((size_t)ptr + alignment - 1) & ~(alignment - 1));
}

template<class T> inline void Exchange(T& A, T& B)
{
	const T tmp = A;
	A = B;
	B = tmp;
}

// Stuff for rvalue support. This is a complicated stuff which is not possible to be
// implemented in a different way, so this is a copy-paste of UE4 code.

template<typename T> struct TRemoveReference      { typedef T Type; };
template<typename T> struct TRemoveReference<T&>  { typedef T Type; };
template<typename T> struct TRemoveReference<T&&> { typedef T Type; };

template<typename T> struct TIsLValueReferenceType     { enum { Value = false }; };
template<typename T> struct TIsLValueReferenceType<T&> { enum { Value = true  }; };

template<typename T1, typename T2>
struct TAreTypesEqual
{
	enum { Value = 0 };
};

template<typename T>
struct TAreTypesEqual<T,T>
{
	enum { Value = 1 };
};

template<typename T>
FORCEINLINE typename TRemoveReference<T>::Type&& MoveTemp(T&& Obj)
{
	typedef typename TRemoveReference<T>::Type CastType;

	// Validate that we're not being passed an rvalue or a const object - the former is redundant, the latter is almost certainly a mistake
	static_assert(TIsLValueReferenceType<T>::Value, "MoveTemp called on an rvalue");
	static_assert(!TAreTypesEqual<CastType&, const CastType&>::Value, "MoveTemp called on a const object");

	return (CastType&&)Obj;
}

// Sorting helpers

template<class T> FORCEINLINE void QSort(T* array, int count, int (*cmpFunc)(const T*, const T*))
{
	qsort(array, count, sizeof(T), (int (*)(const void*, const void*)) cmpFunc);
}

template<class T> FORCEINLINE void QSort(T* array, int count, int (*cmpFunc)(const T&, const T&))
{
	qsort(array, count, sizeof(T), (int (*)(const void*, const void*)) cmpFunc);
}

// special version for 'const char*' arrays (for easier comparator declaration)
//!! todo: add default comparator function with stricmp()
inline void QSort(const char** array, int count, int (*cmpFunc)(const char**, const char**))
{
	qsort(array, count, sizeof(char*), (int (*)(const void*, const void*)) cmpFunc);
}

void appOpenLogFile(const char *filename);
void appPrintf(const char *fmt, ...);

NORETURN void appError(const char *fmt, ...);
#define appErrorNoLog(...) { GError.SuppressLog = true; appError(__VA_ARGS__); }


// Log some information

void appSetNotifyHeader(const char *fmt, ...);
void appNotify(const char *fmt, ...);


// String functions

const char *va(const char *format, ...);
int appSprintf(char *dest, int size, const char *fmt, ...);
int appSprintf(wchar_t *dest, int size, const wchar_t *fmt, ...);
// Allocate a copy of string. Analog of strdup(), but allocation is made with appMalloc.
char* appStrdup(const char* str);
// Copy string to dst with ensuring that string will not exceed 'count' capacity, including trailing zero character.
// The resulting string is always null-terminated.
void appStrncpyz(char *dst, const char *src, int count);
// The same as appStrncpyz(), but will lowercase characters during copying.
void appStrncpylwr(char *dst, const char *src, int count);
// Append src string to dst. Resulting string will never exceed count characters including trailing zero.
// Result is always null-terminated.
void appStrcatn(char *dst, int count, const char *src);
// Finds a substring s2 inside s1 with ignoring character case.
const char *appStristr(const char *s1, const char *s2);

// Returns 'true' if name matches wildcard 'mask'.
bool appMatchWildcard(const char *name, const char *mask, bool ignoreCase = false);
// Returns true is string contains wildcard characters.
bool appContainsWildcard(const char *string);

void appNormalizeFilename(char *filename);
void appMakeDirectory(const char *dirname);
void appMakeDirectoryForFile(const char *filename);

// Parsing response file (file with command line arguments). Throws an error if problems reading file.
void appParseResponseFile(const char* filename, int& outArgc, const char**& outArgv);

#define FS_FILE				1
#define FS_DIR				2

// Check file name type. Returns 0 if not exists, FS_FILE if this is a file,
// and FS_DIR if this is a directory
unsigned appGetFileType(const char *filename);


// Memory management

void* appMalloc(int size, int alignment = 8, bool noInit = false);
void* appRealloc(void *ptr, int newSize);

FORCEINLINE void* appMallocNoInit(int size, int alignment = 8)
{
	return appMalloc(size, alignment, true);
}

void appFree(void *ptr);

#ifndef __APPLE__

// C++ specs doesn't allow inlining of operator new/delete:  https://en.cppreference.com/w/cpp/memory/new/operator_new
// All compilers are fine with that, except clang on macos. For this case we're providing "static" declaration deparately.

FORCEINLINE void* operator new(size_t size)
{
	return appMalloc(size);
}

FORCEINLINE void* operator new[](size_t size)
{
	return appMalloc(size);
}

FORCEINLINE void operator delete(void* ptr)
{
	appFree(ptr);
}

FORCEINLINE void operator delete[](void* ptr)
{
	appFree(ptr);
}

#endif // __APPLE__


// C++17 (delete with alignment)
FORCEINLINE void operator delete(void* ptr, size_t)
{
	appFree(ptr);
}

// inplace new
FORCEINLINE void* operator new(size_t /*size*/, void* ptr)
{
	return ptr;
}


#define DEFAULT_ALIGNMENT		8
#define MEM_CHUNK_SIZE			16384

class CMemoryChain
{
public:
	void* Alloc(size_t size, int alignment = DEFAULT_ALIGNMENT);
	// creating chain
	void* operator new(size_t size, int dataSize = MEM_CHUNK_SIZE);
	// deleting chain
	void operator delete(void* ptr);
	// stats
	int GetSize() const;

private:
	CMemoryChain*	next;
	int				size;
	byte*			data;
	byte*			end;
};


#if PROFILE
// number of dynamic allocations
extern int GNumAllocs;
#endif

// static allocation stats
extern size_t GTotalAllocationSize;
extern int    GTotalAllocationCount;

void appDumpMemoryAllocations();


// "Guard" macros

#if DO_GUARD

// NOTE: using "char __FUNC__[]" instead of "char *__FUNC__" here: in 2nd case compiler
// will generate static string and static pointer variable, but in the 1st case - only
// static string.

#if !WIN32_USE_SEH

// C++exception-based guard/unguard system
#define guard(func)						\
	{									\
		static const char *__FUNC__ = #func; \
		try {

#if DO_GUARD_MAX
#define guardfunc						\
	{									\
		static const char *__FUNC__ = __FUNCSIG__; \
		try {
#else
#define guardfunc						\
	{									\
		static const char *__FUNC__ = __FUNCTION__; \
		try {
#endif

#define unguard							\
		} catch (...) {					\
			CErrorContext::UnwindThrow(__FUNC__); \
		}								\
	}

#define unguardf(...)					\
		} catch (...) {					\
			CErrorContext::SetPrefix(__FUNC__);	\
			CErrorContext::UnwindThrow(__VA_ARGS__);\
		}								\
	}

#define TRY				try
#define CATCH			catch (...)
#define CATCH_CRASH		catch (...)
#define	THROW_AGAIN		throw
#define THROW			throw 1				// throw something (required for GCC in order to get working appError etc)

#else

long win32ExceptFilter(struct _EXCEPTION_POINTERS *info);
#define EXCEPT_FILTER	win32ExceptFilter(GetExceptionInformation())

#define guard(func)						\
	{									\
		static const char __FUNC__[] = #func; \
		__try {

#if DO_GUARD_MAX
#define guardfunc						\
	{									\
		static const char __FUNC__[] = __FUNCSIG__; \
		__try {
#else
#define guardfunc						\
	{									\
		static const char __FUNC__[] = __FUNCTION__; \
		__try {
#endif

#define unguard							\
		} __except (EXCEPT_FILTER) {	\
			CErrorContext::UnwindThrow(__FUNC__); \
		}								\
	}

#define unguardf(...)					\
		} __except (EXCEPT_FILTER) {	\
			CErrorContext::SetPrefix(__FUNC__);	\
			CErrorContext::UnwindThrow(__VA_ARGS__);\
		}								\
	}

#define TRY				__try
#define CATCH			__except(1)			// 1==EXCEPTION_EXECUTE_HANDLER
#define CATCH_CRASH		__except(EXCEPT_FILTER)
#define THROW_AGAIN		throw
#define THROW			throw

#endif

// The structure holding full error information, with reset capability.
struct CErrorContext
{
	typedef void (*ErrorHandlerType)();

	// Determines if this is an exception or appError throwed
	bool IsSwError;
	// Suppress logging error message to a file (in a case of user mistake)
	bool SuppressLog;
	bool IsErrorLogged;

protected:
	// Used for error history formatting
	bool FmtNeedArrow;

	ErrorHandlerType ErrorHandler;

public:
#if THREADING
	int ErrorThreadId;
	bool ShouldLogThisThread();
#endif

	// Call stack
	char History[2048];

	inline CErrorContext()
	{
		Reset();
	}

	inline bool HasError() const
	{
		return History[0] != 0;
	}

	// Reset the error context. Note: this will reset ErrorHandler as well.
	inline void Reset()
	{
		memset(this, 0, sizeof(*this));
	}

	// Set custom error handler (executed with HandleError() call)
	inline void SetErrorHandler(ErrorHandlerType Handler)
	{
		ErrorHandler = Handler;
	}

	static NORETURN void UnwindThrow(const char* fmt, ...);

	// Not vararg (will display function name for unguardf only)
	static void SetPrefix(const char* prefix);

	// Display the error message
	void HandleError();

	// Log error message to console and notify.log, do NOT exit
	void StandardHandler();

	void LogHistory(const char *part);
};

extern CErrorContext GError;

#else  // DO_GUARD

#define guard(func)		{
#define guardfunc		{
#define unguard			}
#define unguardf(...)	}

#define TRY				if (1)
#define CATCH			else
#define CATCH_CRASH		else
#define THROW_AGAIN		throw
#define THROW			throw

#endif // DO_GUARD

#ifdef TRACY_ENABLE

// Use guard macros to instrument code
#undef guard
#undef guardfunc
#undef unguard
#undef unguardf

//#define guard(func)		{ ZoneScopedN(#func);
#define guard(func)		{ ZoneNamedN(___tracy_scoped_zone, #func, bEnableProfiler);
#define guardfunc		{ ZoneScoped;
#define unguard			}
#define unguardf(...)	}

// Stuff for conditional profile samples
namespace ProfilerInternal
{
	enum { bEnableProfiler = 1 };
};
using namespace ProfilerInternal;

#define PROFILE_IF(cond) bool bEnableProfiler = cond;

// Labelling the profile sample
#define PROFILE_LABEL(text)			ZoneText(text, strlen(text))

// Profiling memory allocations
#define PROFILE_ALLOC(ptr, size)	TracyAllocS(ptr, size, 32)
#define PROFILE_FREE(ptr)			TracyFree(ptr)

#else

#define PROFILE_IF(cond)
#define PROFILE_LABEL(text)
#define PROFILE_ALLOC(ptr, size)
#define PROFILE_FREE(ptr)

#endif // TRACY_ENABLE

#if VSTUDIO_INTEGRATION
extern bool GUseDebugger;
#endif


#ifdef _WIN32
#	if !defined(WINAPI) 	// detect <windows.h>
	extern "C" {
		__declspec(dllimport) unsigned long __stdcall GetTickCount();
	}
#	endif
#else
	// Local implementation of GetTickCount() for non-Windows platforms
	unsigned long GetTickCount();
#endif
#define appMilliseconds()		GetTickCount()

// Allow operation of enum class as with regular integer
#define BITFIELD_ENUM(Enum) \
	inline Enum& operator|=(Enum& Lhs, Enum Rhs) { return Lhs = (Enum)((__underlying_type(Enum))Lhs | (__underlying_type(Enum))Rhs); } \
	inline Enum& operator&=(Enum& Lhs, Enum Rhs) { return Lhs = (Enum)((__underlying_type(Enum))Lhs & (__underlying_type(Enum))Rhs); } \
	inline Enum& operator^=(Enum& Lhs, Enum Rhs) { return Lhs = (Enum)((__underlying_type(Enum))Lhs ^ (__underlying_type(Enum))Rhs); } \
	inline Enum  operator| (Enum  Lhs, Enum Rhs) { return (Enum)((__underlying_type(Enum))Lhs | (__underlying_type(Enum))Rhs); } \
	inline Enum  operator& (Enum  Lhs, Enum Rhs) { return (Enum)((__underlying_type(Enum))Lhs & (__underlying_type(Enum))Rhs); } \
	inline Enum  operator^ (Enum  Lhs, Enum Rhs) { return (Enum)((__underlying_type(Enum))Lhs ^ (__underlying_type(Enum))Rhs); } \
	inline bool  operator! (Enum  E)             { return !(__underlying_type(Enum))E; } \
	inline Enum  operator~ (Enum  E)             { return (Enum)~(__underlying_type(Enum))E; }

template<typename Enum>
constexpr bool EnumHasAnyFlags(Enum Flags, Enum Contains)
{
	return ( ((__underlying_type(Enum))Flags) & (__underlying_type(Enum))Contains ) != 0;
}

#if _WIN32

void appInitPlatform();

void appCopyTextToClipboard(const char* text);

int appCaptureStackTrace(address_t* buffer, int maxDepth, int framesToSkip);
void appDumpStackTrace(const address_t* buffer, int depth);

#else

inline void appInitPlatform() {}

inline int appCaptureStackTrace(address_t* buffer, int maxDepth, int framesToSkip) { return 0; }
inline void appDumpStackTrace(const address_t* buffer, int depth) {}

#endif // _WIN32


#include "Math3D.h"


#endif // __CORE_H__
