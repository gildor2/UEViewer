#include "Core.h"

#if _WIN32
#include <direct.h>					// for mkdir()
#else
#include <sys/stat.h>				// for mkdir()
#endif

#if WIN32_USE_SEH
#define WIN32_LEAN_AND_MEAN			// exclude rarely-used services from windown headers
#include <windows.h>
#include <float.h>					// for _clearfp()
#endif // WIN32_USE_SEH


void appPrintf(const char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	char buf[4096];
	int len = vsnprintf(ARRAY_ARG(buf), fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= sizeof(buf) - 1) exit(1);

	fwrite(buf, len, 1, stdout);
}


/*-----------------------------------------------------------------------------
	Simple error/notofication functions
-----------------------------------------------------------------------------*/

static bool GIsSwError = false;			// software-gererated error

void appError(const char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	char buf[4096];
	int len = vsnprintf(ARRAY_ARG(buf), fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= sizeof(buf) - 1) exit(1);

	GIsSwError = true;

#if DO_GUARD
//	appNotify("ERROR: %s\n", buf);
	strcpy(GErrorHistory, buf);
	appStrcatn(ARRAY_ARG(GErrorHistory), "\n");
	THROW;
#else
	fprintf(stderr, "Fatal Error: %s\n", buf);
	exit(1);
#endif
}


static char NotifyBuf[512];

void appSetNotifyHeader(const char *fmt, ...)
{
	if (!fmt)
	{
		NotifyBuf[0] = 0;
		return;
	}
	va_list	argptr;
	va_start(argptr, fmt);
	vsnprintf(ARRAY_ARG(NotifyBuf), fmt, argptr);
	va_end(argptr);
}


void appNotify(const char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	char buf[4096];
	int len = vsnprintf(ARRAY_ARG(buf), fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= sizeof(buf) - 1) exit(1);

	// print to log file
	if (FILE *f = fopen("notify.log", "a"))
	{
		if (NotifyBuf[0])
			fprintf(f, "\n******** %s ********\n\n", NotifyBuf);
		fprintf(f, "%s\n", buf);
		fclose(f);
	}
	// print to console
	if (NotifyBuf[0])
		fprintf(stderr, "******** %s ********\n", NotifyBuf);
	fprintf(stderr, "*** %s\n", buf);
	// clean notify header
	NotifyBuf[0] = 0;
}


char GErrorHistory[2048];
static bool WasError = false;

static void LogHistory(const char *part)
{
	if (!GErrorHistory[0]) strcpy(GErrorHistory, "General Protection Fault !\n");
	appStrcatn(ARRAY_ARG(GErrorHistory), part);
}

void appUnwindPrefix(const char *fmt)
{
	char buf[512];
	appSprintf(ARRAY_ARG(buf), WasError ? " <- %s:" : "%s:", fmt);
	LogHistory(buf);
	WasError = false;
}

#if DO_GUARD

void appUnwindThrow(const char *fmt, ...)
{
	char buf[512];
	va_list argptr;

	va_start(argptr, fmt);
	if (WasError)
	{
		strcpy(buf, " <- ");
		vsnprintf(buf+4, sizeof(buf)-4, fmt, argptr);
	}
	else
	{
		vsnprintf(buf, sizeof(buf), fmt, argptr);
		WasError = true;
	}
	va_end(argptr);
	LogHistory(buf);

	THROW;
}

#endif // DO_GUARD


/*-----------------------------------------------------------------------------
	Memory management
-----------------------------------------------------------------------------*/

#if PROFILE
int GNumAllocs = 0;
#endif

void *appMalloc(int size)
{
	guard(appMalloc);
	assert(size >= 0 && size < (256<<20));	// upper limit to allocation is 256Mb
	void *data = malloc(size);
	if (!data)
		appError("Failed to allocate %d bytes", size);
	if (size > 0)
		memset(data, 0, size);
#if PROFILE
	GNumAllocs++;
#endif
	return data;
	unguardf(("size=%d", size));
}


void appFree(void *ptr)
{
	guard(appFree);
	free(ptr);
	unguard;
}


/*-----------------------------------------------------------------------------
	Miscellaneous
-----------------------------------------------------------------------------*/

#define VA_GOODSIZE		512
#define VA_BUFSIZE		2048

// name of this function is a short form of "VarArgs"
const char *va(const char *format, ...)
{
//	guardSlow(va);

	static char buf[VA_BUFSIZE];
	static int bufPos = 0;
	// wrap buffer
	if (bufPos >= VA_BUFSIZE - VA_GOODSIZE) bufPos = 0;

	va_list argptr;
	va_start(argptr, format);

	// print
	char *str = buf + bufPos;
	int len = vsnprintf(str, VA_BUFSIZE - bufPos, format, argptr);
	if (len < 0 && bufPos > 0)
	{
		// buffer overflow - try again with printing to buffer start
		bufPos = 0;
		str = buf;
		len = vsnprintf(buf, VA_BUFSIZE, format, argptr);
	}

	va_end(argptr);

	if (len < 0)					// not enough buffer space
	{
		const char suffix[] = " ... (overflow)";		// it is better, than return empty string
		memcpy(buf + VA_BUFSIZE - sizeof(suffix), suffix, sizeof(suffix));
		return str;
	}

	bufPos += len + 1;
	return str;

//	unguardSlow;
}


int appSprintf(char *dest, int size, const char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	int len = vsnprintf(dest, size, fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= size - 1)
		appPrintf("appSprintf: overflow of %d\n", size);

	return len;
}


void appStrncpyz(char *dst, const char *src, int count)
{
	if (count <= 0) return;	// zero-length string

	char c;
	do
	{
		if (!--count)
		{
			// out of dst space -- add zero to the string end
			*dst = 0;
			return;
		}
		c = *src++;
		*dst++ = c;
	} while (c);
}


void appStrncpylwr(char *dst, const char *src, int count)
{
	if (count <= 0) return;

	char c;
	do
	{
		if (!--count)
		{
			// out of dst space -- add zero to the string end
			*dst = 0;
			return;
		}
		c = tolower(*src++);
		*dst++ = c;
	} while (c);
}


void appStrcatn(char *dst, int count, const char *src)
{
	char *p = strchr(dst, 0);
	int maxLen = count - (p - dst);
	if (maxLen > 1)
		appStrncpyz(p, src, maxLen);
}


const char *appStristr(const char *s1, const char *s2)
{
	char buf1[1024], buf2[1024];
	appStrncpylwr(buf1, s1, ARRAY_COUNT(buf1));
	appStrncpylwr(buf2, s2, ARRAY_COUNT(buf2));
	char *s = strstr(buf1, buf2);
	if (!s) return NULL;
	return s1 + (s - buf1);
}

static void NormalizeFilename(char *filename)
{
	char *src = filename;
	char *dst = filename;
	char prev = 0;
	while (true)
	{
		char c = *src++;
		if (c == '\\') c = '/';
		if (c == '/' && prev == '/') continue; // squeeze multiple slashes
		*dst++ = prev = c;
		if (!c) break;
	}
}


#if _WIN32

void appMakeDirectory(const char *dirname)
{
	if (!dirname[0]) return;
	// mkdir() and win32 CreateDirectory() does not support "a/b/c" creation,
	// so - we will create "a", then "a/b", then "a/b/c"
	char Name[256];
	appStrncpyz(Name, dirname, ARRAY_COUNT(Name));
	NormalizeFilename(Name);

	for (char *s = Name; /* empty */ ; s++)
	{
		char c = *s;
		if (c != '/' && c != 0)
			continue;
		*s = 0;						// temporarily cut rest of path
		// here: path delimiter or end of string
		if (Name[0] != '.' || Name[1] != 0)		// do not create "."
			_mkdir(Name);
		if (!c) break;				// end of string
		*s = '/';					// restore string (c == '/')
	}
}

#else  // _WIN32

void appMakeDirectory(const char *dirname)
{
	if (!dirname[0]) return;
	// mkdir() does not support "a/b/c" creation,
	// so - we will create "a", then "a/b", then "a/b/c"
	char Name[256];
	appStrncpyz(Name, dirname, ARRAY_COUNT(Name));
	NormalizeFilename(Name);

	for (char *s = Name; /* empty */ ; s++)
	{
		char c = *s;
		if (c != '/' && c != 0)
			continue;
		*s = 0;						// temporarily cut rest of path
		// here: path delimiter or end of string
		if (Name[0] != '.' || Name[1] != 0)		// do not create "."
			mkdir(Name, S_IRWXU);
		if (!c) break;				// end of string
		*s = '/';					// restore string (c == '/')
	}
}

#endif // _WIN32

void appMakeDirectoryForFile(const char *filename)
{
	char Name[256];
	appStrncpyz(Name, filename, ARRAY_COUNT(Name));
	NormalizeFilename(Name);

	char *s = strrchr(Name, '/');
	if (s)
	{
		*s = 0;
		appMakeDirectory(Name);
	}
}


/*-----------------------------------------------------------------------------
	Win32 exception handler (SEH)
-----------------------------------------------------------------------------*/

#if WIN32_USE_SEH

long WINAPI win32ExceptFilter(struct _EXCEPTION_POINTERS *info)
{
	if (GIsSwError) return EXCEPTION_EXECUTE_HANDLER;		// no interest to thread context when software-generated errors

	static bool dumped = false;
	if (dumped) return EXCEPTION_EXECUTE_HANDLER;			// error will be handled only once
	// NOTE: side effect of line above: we will not able to catch GPF-like recursive errors

	// WARNING: recursive error will not be found
	// If we will disable line above, will be dumped context for each appUnwind() entry
	dumped = true;

	// if FPU exception occured, _clearfp() is required (otherwise, exception will be re-raised again)
	_clearfp();


	TRY {
		const char *excName = "Exception";
		switch (info->ExceptionRecord->ExceptionCode)
		{
		case EXCEPTION_ACCESS_VIOLATION:
			excName = "Access violation";
			break;
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
			excName = "Float zero divide";
			break;
		case EXCEPTION_FLT_DENORMAL_OPERAND:
			excName = "Float denormal operand";
			break;
		case EXCEPTION_FLT_INVALID_OPERATION:
		case EXCEPTION_FLT_INEXACT_RESULT:
		case EXCEPTION_FLT_OVERFLOW:
		case EXCEPTION_FLT_STACK_CHECK:
		case EXCEPTION_FLT_UNDERFLOW:
			excName = "FPU exception";
			break;
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			excName = "Integer zero divide";
			break;
		case EXCEPTION_PRIV_INSTRUCTION:
			excName = "Priveleged instruction";
			break;
		case EXCEPTION_ILLEGAL_INSTRUCTION:
			excName = "Illegal opcode";
			break;
		case EXCEPTION_STACK_OVERFLOW:
			excName = "Stack overflow";
			break;
		}

		// log error
		CONTEXT* ctx = info->ContextRecord;
		appSprintf(ARRAY_ARG(GErrorHistory), "%s (%08X) at %08X\n",
			excName, info->ExceptionRecord->ExceptionCode, ctx->Eip
		);
	} CATCH {
		// do nothing
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

__declspec(naked) unsigned win32ExceptFilter2()
{
	__asm {
		push	[ebp-0x14]
		call	win32ExceptFilter
		retn			// return value from win32ExceptFilter()
	}
}

#endif // WIN32_USE_SEH
