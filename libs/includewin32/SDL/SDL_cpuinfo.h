/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2011 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/

/**
 *  \file SDL_cpuinfo.h
 *  
 *  CPU feature detection for SDL.
 */

#ifndef _SDL_cpuinfo_h
#define _SDL_cpuinfo_h

#include "SDL_stdinc.h"

/* Need to do this here because intrin.h has C++ code in it */
/* Visual Studio 2005 has a bug where intrin.h conflicts with winnt.h */
#if defined(_MSC_VER) && (_MSC_VER >= 1500) && !defined(_WIN32_WCE)
#include <intrin.h>
#ifndef _WIN64
#define __MMX__
#define __3dNOW__
#endif
#define __SSE__
#define __SSE2__
#elif defined(__MINGW64_VERSION_MAJOR)
#include <intrin.h>
#else
#ifdef __ALTIVEC__
#if HAVE_ALTIVEC_H && !defined(__APPLE_ALTIVEC__)
#include <altivec.h>
#undef pixel
#endif
#endif
#ifdef __MMX__
#include <mmintrin.h>
#endif
#ifdef __3dNOW__
#include <mm3dnow.h>
#endif
#ifdef __SSE__
#include <xmmintrin.h>
#endif
#ifdef __SSE2__
#include <emmintrin.h>
#endif
#endif

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* This is a guess for the cacheline size used for padding.
 * Most x86 processors have a 64 byte cache line.
 * The 64-bit PowerPC processors have a 128 byte cache line.
 * We'll use the larger value to be generally safe.
 */
#define SDL_CACHELINE_SIZE  128

/**
 *  This function returns the number of CPU cores available.
 */
extern DECLSPEC int SDLCALL SDL_GetCPUCount(void);

/**
 *  This function returns the L1 cache line size of the CPU
 *
 *  This is useful for determining multi-threaded structure padding
 *  or SIMD prefetch sizes.
 */
extern DECLSPEC int SDLCALL SDL_GetCPUCacheLineSize(void);

/**
 *  This function returns true if the CPU has the RDTSC instruction.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasRDTSC(void);

/**
 *  This function returns true if the CPU has AltiVec features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasAltiVec(void);

/**
 *  This function returns true if the CPU has MMX features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasMMX(void);

/**
 *  This function returns true if the CPU has 3DNow! features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_Has3DNow(void);

/**
 *  This function returns true if the CPU has SSE features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasSSE(void);

/**
 *  This function returns true if the CPU has SSE2 features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasSSE2(void);

/**
 *  This function returns true if the CPU has SSE3 features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasSSE3(void);

/**
 *  This function returns true if the CPU has SSE4.1 features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasSSE41(void);

/**
 *  This function returns true if the CPU has SSE4.2 features.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasSSE42(void);


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#include "close_code.h"

#endif /* _SDL_cpuinfo_h */

/* vi: set ts=4 sw=4 expandtab: */
