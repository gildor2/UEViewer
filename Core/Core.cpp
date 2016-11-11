#include "Core.h"

#if _WIN32
#include <direct.h>					// for mkdir()
#endif

#include <sys/stat.h>				// for mkdir(), stat()

#if VSTUDIO_INTEGRATION
#define WIN32_LEAN_AND_MEAN			// exclude rarely-used services from windown headers
#define _WIN32_WINDOWS 0x0500		// for IsDebuggerPresent()
#include <windows.h>
#endif // VSTUDIO_INTEGRATION


static FILE *GLogFile = NULL;

void appOpenLogFile(const char *filename)
{
	GLogFile = fopen(filename, "a");
	if (!GLogFile)
		appPrintf("Unable to open log \"%s\"\n", filename);
}


void appPrintf(const char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	char buf[4096];
	int len = vsnprintf(ARRAY_ARG(buf), fmt, argptr);
	va_end(argptr);
	assert(len >= 0 && len < ARRAY_COUNT(buf) - 1);

	fwrite(buf, len, 1, stdout);
	if (GLogFile) fwrite(buf, len, 1, GLogFile);

#if VSTUDIO_INTEGRATION
	if (IsDebuggerPresent())
		OutputDebugString(buf);
#endif
}


/*-----------------------------------------------------------------------------
	Simple error/notofication functions
-----------------------------------------------------------------------------*/

bool GIsSwError = false;			// software-gererated error

void appError(const char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	char buf[4096];
	int len = vsnprintf(ARRAY_ARG(buf), fmt, argptr);
	va_end(argptr);
	assert(len >= 0 && len < ARRAY_COUNT(buf) - 1);

	GIsSwError = true;

#if DO_GUARD
//	appNotify("ERROR: %s\n", buf);
	strcpy(GErrorHistory, buf);
	appStrcatn(ARRAY_ARG(GErrorHistory), "\n");
	THROW;
#else
	fprintf(stderr, "Fatal Error: %s\n", buf);
	if (GLogFile) fprintf(GLogFile, "Fatal Error: %s\n", buf);
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
	assert(len >= 0 && len < ARRAY_COUNT(buf) - 1);

	fflush(stdout);

	// a bit ugly code: printing the same thing into 3 streams

	// print to notify file
	if (FILE *f = fopen("notify.log", "a"))
	{
		if (NotifyBuf[0])
			fprintf(f, "\n******** %s ********\n\n", NotifyBuf);
		fprintf(f, "%s\n", buf);
		fclose(f);
	}
	// print to log file
	if (GLogFile)
	{
		if (NotifyBuf[0])
			fprintf(GLogFile, "******** %s ********\n", NotifyBuf);
		fprintf(GLogFile, "*** %s\n", buf);
		fflush(GLogFile);
	}
	// print to console
	if (NotifyBuf[0])
		fprintf(stderr, "******** %s ********\n", NotifyBuf);
	fprintf(stderr, "*** %s\n", buf);
	fflush(stderr);
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
		vsnprintf(buf+4, ARRAY_COUNT(buf)-4, fmt, argptr);
	}
	else
	{
		vsnprintf(buf, ARRAY_COUNT(buf), fmt, argptr);
		WasError = true;
	}
	va_end(argptr);
	LogHistory(buf);

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

#if 0
// Mask variants:
// 1) *      - any file
// 2) *.*    - any file
// 3) *rest  - name ends with "rest" (for example, ".ext")
// 4) start* - name starts with "start"
// 4) text   - name equals "text"
// Comparision is case-sensitive, when ignoreCase == false (default)
// A few masks can be separated with ','
bool appMatchWildcard(const char *name, const char *mask, bool ignoreCase)
{
	guard(appMatchWildcard);

	if (!name[0] && !mask[0]) return true;		// empty strings matched

	char NameCopy[1024], MaskCopy[1024];
	if (ignoreCase)
	{
		appStrncpylwr(NameCopy, name, ARRAY_COUNT(NameCopy));
		appStrncpylwr(MaskCopy, mask, ARRAY_COUNT(MaskCopy));
	}
	else
	{
		appStrncpyz(NameCopy, name, ARRAY_COUNT(NameCopy));
		appStrncpyz(MaskCopy, mask, ARRAY_COUNT(MaskCopy));
	}

	int namelen = strlen(NameCopy);

	// can use TStringSplitter here
	const char *next;
	for (mask = MaskCopy; mask; mask = next)
	{
		// find next wildcard (comma-separated)
		next = strchr(mask, ',');
		int masklen;
		if (next)
		{
			masklen = next - mask;
			next++;					// skip ','
		}
		else
			masklen = strlen(mask);

		if (!masklen)
		{
			// used something like "mask1,,mask3" (2nd mask is empty)
//??		appPrintf("appMatchWildcard: skip empty mask in \"%s\"\n", mask);
			continue;
		}

		// check for a trivial wildcard
		if (mask[0] == '*')
		{
			if (masklen == 1 || (masklen == 3 && mask[1] == '.' && mask[2] == '*'))
				return true;		// "*" or "*.*" -- any name valid
		}

		// "*text*" mask
		if (masklen >= 3 && mask[0] == '*' && mask[masklen-1] == '*')
		{
			int		i;

			mask++;
			masklen -= 2;
			for (i = 0; i <= namelen - masklen; i++)
				if (!memcmp(&NameCopy[i], mask, masklen)) return true;
		}
		else
		{
			// "*text" or "text*" mask
			const char *suff = strchr(mask, '*');
			if (next && suff >= next) suff = NULL;		// suff can be in next wildcard
			if (suff)
			{
				int preflen = suff - mask;
				int sufflen = masklen - preflen - 1;
				suff++;

				if (namelen < preflen + sufflen)
					continue;		// name is not long enough
				if (preflen && memcmp(NameCopy, mask, preflen))
					continue;		// different prefix
				if (sufflen && memcmp(NameCopy + namelen - sufflen, suff, sufflen))
					continue;		// different suffix

				return true;
			}
			// exact match ("text")
			if (namelen == masklen && !memcmp(NameCopy, mask, namelen))
				return true;
		}
	}

	return false;
	unguard;
}

#else

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

#endif


bool appContainsWildcard(const char *string)
{
	if (strchr(string, '*')) return true;
	if (strchr(string, ',')) return true;
	return false;
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
