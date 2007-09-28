#ifndef __CORE_H__
#define __CORE_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>


#define VECTOR_ARG(name)	name[0],name[1],name[2]
#define ARRAY_ARG(array)	array, sizeof(array)/sizeof(array[0])
#define ARRAY_COUNT(array)	(sizeof(array)/sizeof(array[0]))

#define FVECTOR_ARG(v)		v.X, v.Y, v.Z
#define ROTATOR_ARG(r)		r.Yaw, r.Pitch, r.Roll

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


#if _MSC_VER
#	define vsnprintf		_vsnprintf
#	define FORCEINLINE		__forceinline
#endif


void appError(char *fmt, ...);
void* appMalloc(int size);
void appFree(void *ptr);

// log some interesting information
extern const char *GNotifyInfo;
void appNotify(char *fmt, ...);

int appSprintf(char *dest, int size, const char *fmt, ...);
void appStrncpyz(char *dst, const char *src, int count);


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


#include "Math3D.h"


#endif // __CORE_H__
