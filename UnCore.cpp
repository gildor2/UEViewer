#include "Core.h"
#include "UnCore.h"


/*-----------------------------------------------------------------------------
	Simple error/notofication functions
-----------------------------------------------------------------------------*/

void appError(char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	char buf[4096];
	int len = vsnprintf(ARRAY_ARG(buf), fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= sizeof(buf) - 1) exit(1);

//	appNotify("ERROR: %s\n", buf);
	strcpy(GErrorHistory, buf);
	appStrcatn(ARRAY_ARG(GErrorHistory), "\n");
	THROW;
}


const char *GNotifyInfo = "";

void appNotify(char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	char buf[4096];
	int len = vsnprintf(ARRAY_ARG(buf), fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= sizeof(buf) - 1) exit(1);

	if (FILE *f = fopen("notify.log", "a"))
	{
		fprintf(f, "[%s] %s\n", GNotifyInfo, buf);
		fclose(f);
	}
	printf("[%s] %s\n", GNotifyInfo, buf);
}


char GErrorHistory[2048];
static bool WasError = false;

static void LogHistory(const char *part)
{
	appStrcatn(ARRAY_ARG(GErrorHistory), part);
}

void appUnwindPrefix(const char *fmt)
{
	char buf[512];
	appSprintf(ARRAY_ARG(buf), WasError ? " <- %s:" : "%s:", fmt);
	LogHistory(buf);
	WasError = false;
}


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


/*-----------------------------------------------------------------------------
	Memory management
-----------------------------------------------------------------------------*/

void *appMalloc(int size)
{
	assert(size >= 0);
	void *data = malloc(size);
	if (size > 0)
		memset(data, 0, size);
	return data;
}


void appFree(void *ptr)
{
	free(ptr);
}


/*-----------------------------------------------------------------------------
	FArchive methods
-----------------------------------------------------------------------------*/

FArchive& operator<<(FArchive &Ar, FCompactIndex &I)
{
	if (Ar.IsLoading)
	{
		byte b;
		Ar << b;
		int sign  = b & 0x80;
		int shift = 6;
		int r     = b & 0x3F;
		if (b & 0x40)
		{
			do
			{
				Ar << b;
				r |= (b & 0x7F) << shift;
				shift += 7;
			} while (b & 0x80);
		}
		I.Value = sign ? -r : r;
	}
	else
	{
		appError("write AR_INDEX is not implemented");
	}
	return Ar;
}


/*-----------------------------------------------------------------------------
	Miscellaneous
-----------------------------------------------------------------------------*/

int appSprintf(char *dest, int size, const char *fmt, ...)
{
	va_list	argptr;
	va_start(argptr, fmt);
	int len = vsnprintf(dest, size, fmt, argptr);
	va_end(argptr);
	if (len < 0 || len >= size - 1)
		printf("appSprintf: overflow of %d\n", size);

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


void appStrcatn(char *dst, int count, const char *src)
{
	char *p = strchr(dst, 0);
	int maxLen = count - (p - dst);
	if (maxLen > 1)
		appStrncpyz(p, src, maxLen);
}
