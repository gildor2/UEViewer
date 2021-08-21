#include <windows.h>
#include <delayimp.h>

// Reference:
// https://docs.microsoft.com/en-us/cpp/build/reference/understanding-the-helper-function

// Core external functions
void appPrintf(const char*, ...);
void appError(const char*, ...);

static const char* DllPaths[] =
{
#ifndef _WIN64
	"SDL2.dll",
	"libs\\SDL2\\x86\\SDL2.dll"
#else
	"SDL2_64.dll",
	"libs\\SDL2\\x64\\SDL2.dll"
#endif
};

static FARPROC WINAPI delayHook(unsigned dliNotify, PDelayLoadInfo pdli)
{
	if (dliNotify == dliNotePreLoadLibrary)
	{
		if (stricmp(pdli->szDll, "SDL2.dll") == 0)
		{
			// Find SDL2.dll in local folder, then in libs. Use a different file name for Win64 builds.
			for (const char* DllName : DllPaths)
			{
				HANDLE hDll = LoadLibrary(DllName);
				if (hDll)
					return (FARPROC) hDll;
			}
			appError("SDL2.dll was not found, terminating...");
		}
		else
		{
			// Actually can simply return NULL, and let CRT to find a DLL itself
			appError("Unknown DelayLoad: %s", pdli->szDll);
		}
	}

    return NULL;
}

ExternC const PfnDliHook __pfnDliNotifyHook2 = delayHook;
