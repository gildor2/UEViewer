#ifndef __CORE_H__
#define __CORE_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>


#if RENDERING
#	include <SDL/SDL.h>		//?? move outside

#if _WIN32
#	ifndef APIENTRY
#		define APIENTRY __stdcall
#	endif
#	ifndef WINGDIAPI
#		define WINGDIAPI
		typedef unsigned		HDC;
		typedef unsigned		HGLRC;
		typedef const char *	LPCSTR;
		typedef int				BOOL;
		typedef unsigned char	BYTE;
		typedef unsigned short	WORD;
		typedef unsigned int	UINT;
		typedef int (APIENTRY *PROC)();
		typedef void PIXELFORMATDESCRIPTOR;		// structure
		typedef PIXELFORMATDESCRIPTOR * LPPIXELFORMATDESCRIPTOR;
#	endif // WINGDIAPI
#	ifndef CONST
#		define CONST const
#	endif
#endif

#	include <GL/gl.h>
#endif


#define VECTOR_ARG(name)	name[0],name[1],name[2]
#define ARRAY_ARG(array)	array, sizeof(array)/sizeof(array[0])
#define ARRAY_COUNT(array)	(sizeof(array)/sizeof(array[0]))

#define FVECTOR_ARG(v)		v.X, v.Y, v.Z
#define FROTATOR_ARG(r)		r.Yaw, r.Pitch, r.Roll

#define BYTES4(a,b,c,d)	((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))


#define assert(x)	\
	if (!(x))		\
	{				\
		appError("assertion failed: %s\n", #x); \
	}

#undef M_PI
#define M_PI				(3.14159265358979323846)


#undef min
#undef max

#define min(a,b)  (((a) < (b)) ? (a) : (b))
#define max(a,b)  (((a) > (b)) ? (a) : (b))
#define bound(a,minval,maxval)  ( ((a) > (minval)) ? ( ((a) < (maxval)) ? (a) : (maxval) ) : (minval) )

#define appFloor(x)		( (int)floor(x) )
#define appCeil(x)		( (int)ceil(x) )
#define appRound(x)		( (int) (x >= 0 ? (x)+0.5f : (x)-0.5f) )


#if _MSC_VER
#	define vsnprintf		_vsnprintf
#	define FORCEINLINE		__forceinline
#	define NORETURN			__declspec(noreturn)
#elif __GNUC__
#	define NORETURN			__attribute__((noreturn))
#	if (__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 2))
	// strange, but there is only way to work (inline+always_inline)
#		define FORCEINLINE	inline __attribute__((always_inline))
#	else
#		define FORCEINLINE	inline
#	endif
#	define stricmp			strcasecmp
#	define strnicmp			strncasecmp
#else
#	error "Unsupported compiler"
#endif

// necessary types
typedef unsigned char		byte;
typedef unsigned short		word;


#define COLOR_ESCAPE	'^'		// may be used for quick location of color-processing code

#define S_BLACK			"^0"
#define S_RED			"^1"
#define S_GREEN			"^2"
#define S_YELLOW		"^3"
#define S_BLUE			"^4"
#define S_MAGENTA		"^5"
#define S_CYAN			"^6"
#define S_WHITE			"^7"


template<class T> inline T OffsetPointer(const T ptr, int offset)
{
	return (T) ((unsigned)ptr + offset);
}


template<class T> inline void Exchange(T& A, T& B)
{
	const T tmp = A;
	A = B;
	B = tmp;
}


void appError(char *fmt, ...);
void* appMalloc(int size);
void appFree(void *ptr);

// log some interesting information
void appSetNotifyHeader(const char *fmt, ...);
void appNotify(char *fmt, ...);

int appSprintf(char *dest, int size, const char *fmt, ...);
void appStrncpyz(char *dst, const char *src, int count);
void appStrcatn(char *dst, int count, const char *src);


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


// C++excpetion-based guard/unguard system
#define guard(func)						\
	{									\
		static const char __FUNC__[] = #func; \
		try {

#define unguard							\
		} catch (...) {					\
			appUnwindThrow(__FUNC__);	\
		}								\
	}

#define unguardf(msg)					\
		} catch (...) {					\
			appUnwindPrefix(__FUNC__);	\
			appUnwindThrow msg;			\
		}								\
	}

#define	THROW_AGAIN		throw
#define THROW			throw 1

void appUnwindPrefix(const char *fmt);		// not vararg (will display function name for unguardf only)
NORETURN void appUnwindThrow(const char *fmt, ...);

extern char GErrorHistory[2048];


#define appMilliseconds()		SDL_GetTicks()


#include "Math3D.h"


#endif // __CORE_H__
