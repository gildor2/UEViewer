#include "Core.h"

#if _WIN32
#include <direct.h>					// for mkdir()
#endif

#include <sys/stat.h>				// for mkdir(), stat()

#if !_WIN32
#include <time.h>					// for Linux version of GetTickCount()
#endif

#if VSTUDIO_INTEGRATION
#define WIN32_LEAN_AND_MEAN			// exclude rarely-used services from windown headers
#define _WIN32_WINDOWS 0x0500		// for IsDebuggerPresent()
#include <windows.h>
#endif // VSTUDIO_INTEGRATION

#if THREADING

#include "Parallel.h"

static CMutex GLogMutex;

#endif // THREADING

static FILE *GLogFile = NULL;

void appOpenLogFile(const char *filename)
{
	if (GLogFile) fclose(GLogFile);

	GLogFile = fopen(filename, "a");
	if (!GLogFile)
		appPrintf("Unable to open log \"%s\"\n", filename);
}


void appPrintf(const char *fmt, ...)
{
	guard(appPrintf);

	va_list	argptr;
	va_start(argptr, fmt);
	char buf[4096];
	int len = vsnprintf(ARRAY_ARG(buf), fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= ARRAY_COUNT(buf) - 1) appErrorNoLog("appPrintf: buffer overflow");

#if THREADING
	// Make appPrintf thread-safe
	CMutex::ScopedLock Lock(GLogMutex);
#endif

	fwrite(buf, len, 1, stdout);
	if (GLogFile) fwrite(buf, len, 1, GLogFile);

#if VSTUDIO_INTEGRATION
	if (IsDebuggerPresent())
		OutputDebugString(buf);
#endif

	unguard;
}


/*-----------------------------------------------------------------------------
	Simple error/notification functions
-----------------------------------------------------------------------------*/

CErrorContext GError;

void appError(const char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	char buf[4096];
	int len = vsnprintf(ARRAY_ARG(buf), fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= ARRAY_COUNT(buf) - 1) appErrorNoLog("appError: buffer overflow");

	GError.IsSwError = true;

#if VSTUDIO_INTEGRATION
	if (IsDebuggerPresent())
	{
		OutputDebugString("Fatal Error: ");
		OutputDebugString(buf);
	}
#endif

#if DO_GUARD
//	appNotify("ERROR: %s\n", buf);
	strcpy(GError.History, buf);
	appStrcatn(ARRAY_ARG(GError.History), "\n");
	THROW;
#else
	fprintf(stderr, "Fatal Error: %s\n", buf);
	if (GLogFile) fprintf(GLogFile, "Fatal Error: %s\n", buf);
	exit(1);
#endif
}


static char NotifyHeader[512];

void appSetNotifyHeader(const char *fmt, ...)
{
	if (!fmt)
	{
		NotifyHeader[0] = 0;
		return;
	}
	va_list	argptr;
	va_start(argptr, fmt);
	vsnprintf(ARRAY_ARG(NotifyHeader), fmt, argptr);
	va_end(argptr);
}


void appNotify(const char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	char buf[4096];
	int len = vsnprintf(ARRAY_ARG(buf), fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= ARRAY_COUNT(buf) - 1) appErrorNoLog("appNotify: buffer overflow");

	fflush(stdout);

#if THREADING
	// Make appPrintf thread-safe
	CMutex::ScopedLock Lock(GLogMutex);
#endif

	// a bit ugly code: printing the same thing into 3 streams

	// print to notify file
	if (FILE *f = fopen("notify.log", "a"))
	{
		if (NotifyHeader[0])
			fprintf(f, "\n******** %s ********\n\n", NotifyHeader);
		fprintf(f, "%s\n", buf);
		fclose(f);
	}
	// print to log file
	if (GLogFile)
	{
		if (NotifyHeader[0])
			fprintf(GLogFile, "******** %s ********\n", NotifyHeader);
		fprintf(GLogFile, "*** %s\n", buf);
		fflush(GLogFile);
	}
	// print to console
	if (NotifyHeader[0])
		fprintf(stderr, "******** %s ********\n", NotifyHeader);
	fprintf(stderr, "*** %s\n", buf);
	fflush(stderr);
	// clean notify header
	NotifyHeader[0] = 0;
}

void CErrorContext::StandardHandler()
{
	void (*PrintFunc)(const char*, ...);
	PrintFunc = GError.SuppressLog ? appPrintf : appNotify;
	// appNotify does some markup itself, add explicit marker for pure appPrintf
	const char* Marker = GError.SuppressLog ? "\n*** " : "";

	if (GError.History[0])
	{
//		printf("ERROR: %s\n", GError.History);
		PrintFunc("%sERROR: %s\n", Marker, GError.History);
	}
	else
	{
//		printf("Unknown error\n");
		PrintFunc("%sUnknown error\n", Marker);
	}
}

#if DO_GUARD

void CErrorContext::LogHistory(const char *part)
{
	if (!History[0])
		strcpy(History, "General Protection Fault !\n");
	appStrcatn(ARRAY_ARG(History), part);
}

void appUnwindPrefix(const char *fmt)
{
	char buf[512];
	appSprintf(ARRAY_ARG(buf), GError.FmtNeedArrow ? " <- %s: " : "%s: ", fmt);
	GError.LogHistory(buf);
	GError.FmtNeedArrow = false;
}

void appUnwindThrow(const char *fmt, ...)
{
	char buf[512];
	va_list argptr;

	va_start(argptr, fmt);
	if (GError.FmtNeedArrow)
	{
		strcpy(buf, " <- ");
		vsnprintf(buf+4, ARRAY_COUNT(buf)-4, fmt, argptr);
	}
	else
	{
		vsnprintf(buf, ARRAY_COUNT(buf), fmt, argptr);
		GError.FmtNeedArrow = true;
	}
	va_end(argptr);
	GError.LogHistory(buf);

	THROW;
}

#endif // DO_GUARD


/*-----------------------------------------------------------------------------
	String functions
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
		memcpy(buf + VA_BUFSIZE - ARRAY_COUNT(suffix), suffix, ARRAY_COUNT(suffix));
		return str;
	}

	bufPos += len + 1;
	return str;

//	unguardSlow;
}


char* appStrdup(const char* str)
{
	int len = strlen(str) + 1;
	char* buf = (char*)appMalloc(len);
	memcpy(buf, str, len);
	return buf;
}


int appSprintf(char *dest, int size, const char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	int len = vsnprintf(dest, size, fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= size - 1)
		appPrintf("appSprintf: overflow of size %d (fmt=%s)\n", size, fmt);

	return len;
}


// Unicode appSprintf
int appSprintf(wchar_t *dest, int size, const wchar_t *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	int len = vsnwprintf(dest, size, fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= size - 1)
		appPrintf("appSprintf: overflow of size %d (fmt=%S)\n", size, fmt);

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

void appNormalizeFilename(char *filename)
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
	if (--dst > filename)
	{
		// strip trailing slash, if one
		if (*dst == '/') *dst = 0;
	}
}

/*-----------------------------------------------------------------------------
	Simple wildcard matching
-----------------------------------------------------------------------------*/

// Wildcard matching function from
// http://www.drdobbs.com/architecture-and-design/matching-wildcards-an-empirical-way-to-t/240169123

// This function compares text strings, one of which can have wildcards ('*' or '?').
static bool WildTextCompare(
	const char *pTameText,   // A string without wildcards
	const char *pWildText    // A (potentially) corresponding string with wildcards
)
{
	// These two values are set when we observe a wildcard character.  They
	// represent the locations, in the two strings, from which we start once
	// we've observed it.
	const char *pTameBookmark = NULL;
	const char *pWildBookmark = NULL;

	// Walk the text strings one character at a time.
	while (true)
	{
		// How do you match a unique text string?
		if (*pWildText == '*')
		{
			// Easy: unique up on it!
			while (*(++pWildText) == '*')
			{
			}                          // "xy" matches "x**y"

			if (!*pWildText)
			{
				return true;           // "x" matches "*"
			}

			if (*pWildText != '?')
			{
				// Fast-forward to next possible match.
				while (*pTameText != *pWildText)
				{
					if (!(*(++pTameText)))
						return false;  // "x" doesn't match "*y*"
				}
			}

			pWildBookmark = pWildText;
			pTameBookmark = pTameText;
		}
		else if (*pTameText != *pWildText && *pWildText != '?')
		{
			// Got a non-match.  If we've set our bookmarks, back up to one
			// or both of them and retry.
			//
			if (pWildBookmark)
			{
				if (pWildText != pWildBookmark)
				{
					pWildText = pWildBookmark;

					if (*pTameText != *pWildText)
					{
						// Don't go this far back again.
						pTameText = ++pTameBookmark;
						continue;      // "xy" matches "*y"
					}
					else
					{
						pWildText++;
					}
				}

				if (*pTameText)
				{
					pTameText++;
					continue;          // "mississippi" matches "*sip*"
				}
			}

			return false;              // "xy" doesn't match "x"
		}

		pTameText++;
		pWildText++;

		// How do you match a tame text string?
		if (!*pTameText)
		{
			// The tame way: unique up on it!
			while (*pWildText == '*')
			{
				pWildText++;           // "x" matches "x*"
			}

			if (!*pWildText)
			{
				return true;           // "x" matches "x"
			}

			return false;              // "x" doesn't match "xy"
		}
	}
}

bool appMatchWildcard(const char *name, const char *mask, bool ignoreCase)
{
	guard(appMatchWildcard);

	if (!name[0] && !mask[0]) return true;		// empty strings matched

	if (ignoreCase)
	{
		char NameCopy[1024], MaskCopy[1024];
		appStrncpylwr(NameCopy, name, ARRAY_COUNT(NameCopy));
		appStrncpylwr(MaskCopy, mask, ARRAY_COUNT(MaskCopy));
		return WildTextCompare(NameCopy, MaskCopy);
	}
	else
	{
		return WildTextCompare(name, mask);
	}

	unguard;
}

bool appContainsWildcard(const char *string)
{
	if (strchr(string, '*')) return true;
	if (strchr(string, ',')) return true;
	if (strchr(string, '?')) return true;
	return false;
}


/*-----------------------------------------------------------------------------
	Command line helpers
-----------------------------------------------------------------------------*/

void appParseResponseFile(const char* filename, int& outArgc, const char**& outArgv)
{
	guard(appParseResponseFile);

	FILE* f = fopen(filename, "r");
	if (!f)
	{
		appErrorNoLog("Unable to find command line file \"%s\"", filename);
	}
	// Determine file size
	fseek(f, 0, SEEK_END);
	size_t len = ftell(f);
	fseek(f, 0, SEEK_SET);
	// Allocate buffer, we'll never release it
	char* buffer = (char*)appMalloc(len+1);
	// Read contents. Note that on Windows, fread will skip 'r' characters in text mode.
	len = fread(buffer, 1, len, f);
	if (len == 0)
	{
		appErrorNoLog("Unable to read command line file \"%s\"", filename);
	}
	fclose(f);
	buffer[len] = 0;

	// Parse in 2 passes: count number of arguments, then store result
	for (int pass = 0; pass < 2; pass++)
	{
		char* s = buffer;
		int argc = 1; // reserve argv[0] for executable name
		while (*s)
		{
			// Skip whitespace
			while (isspace(*s))
			{
				s++;
			}
			if (*s == 0) break;
			// Skip comments
			if (*s == '#' || *s == ';')
			{
				s++;
				while (*s != 0 && *s != '\n')
				{
					s++;
				}
				continue;
			}
			// Parameter
			if (*s == '"')
			{
				s++; // skip quote
				// Process quoted strings
				if (pass) outArgv[argc] = s;
				while (*s != '"' && *s != 0 && *s != '\n')
				{
					s++;
				}
				if (pass) *s = 0;
				s++; // skip quote
				argc++;
			}
			else
			{
				// Regular string
				if (pass) outArgv[argc] = s;
				while (!isspace(*s) && *s != 0)
				{
					if (*s == '"')
					{
						// Quotes in the middle of parameter (-path="..." etc) - include spaces
						s++;
						while (*s != '"'&& *s != '\n' && *s != 0)
						{
							s++;
						}
						if (*s == '"')
						{
							// Skip closing quote so it won't be erased
							s++;
						}
					}
					else
					{
						s++;
					}
				}
				if (pass) *s = 0;
				s++; // skip space
				argc++;
			}
		}

		if (pass == 0)
		{
			// Allocate argv[] array (will never release it)
			outArgv = new const char*[argc+1];
			outArgv[0] = "";			// placeholder for executable name
			outArgv[argc] = NULL;		// next-after-last is NULL
			outArgc = argc;
		}
	}

	unguard;
}


/*-----------------------------------------------------------------------------
	File helpers
-----------------------------------------------------------------------------*/

void appMakeDirectory(const char *dirname)
{
	if (!dirname[0]) return;
	// Win32 and Unix: there is no API to create directory chains
	// so - we will create "a", then "a/b", then "a/b/c"
	char Name[256];
	appStrncpyz(Name, dirname, ARRAY_COUNT(Name));
	appNormalizeFilename(Name);

	for (char *s = Name; /* empty */ ; s++)
	{
		char c = *s;
		if (c != '/' && c != 0)
			continue;
		*s = 0;						// temporarily cut rest of path
		// here: path delimiter or end of string
		if (Name[0] != '.' || Name[1] != 0)		// do not create "."
#if _WIN32
			_mkdir(Name);
#else
			mkdir(Name, S_IRWXU);
#endif
		if (!c) break;				// end of string
		*s = '/';					// restore string (c == '/')
	}
}

void appMakeDirectoryForFile(const char *filename)
{
	char Name[256];
	appStrncpyz(Name, filename, ARRAY_COUNT(Name));
	appNormalizeFilename(Name);

	char *s = strrchr(Name, '/');
	if (s)
	{
		*s = 0;
		appMakeDirectory(Name);
	}
}

#ifndef S_ISDIR
// no such declarations in windows headers, but exists in mingw32 ...
#define	S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#define	S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#define stat _stati64
#endif

unsigned appGetFileType(const char *filename)
{
	char Name[256];
	appStrncpyz(Name, filename, ARRAY_COUNT(Name));
	appNormalizeFilename(Name);

	struct stat buf;
	if (stat(filename, &buf) == -1)
		return 0;					// no such file/dir
	if (S_ISDIR(buf.st_mode))
		return FS_DIR;
	else if (S_ISREG(buf.st_mode))
		return FS_FILE;
	return 0;						// just in case ... (may be, win32 have other file types?)
}

#if !_WIN32

// POSIX version of GetTickCount()
unsigned long GetTickCount()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64)(ts.tv_nsec / 1000000) + ((uint64)ts.tv_sec * 1000ull);
}

#endif // _WIN32
