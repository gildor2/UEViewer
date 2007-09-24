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

	printf("ERROR: %s\n", buf);
	appNotify("ERROR: %s\n", buf);
	exit(1);
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
