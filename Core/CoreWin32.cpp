#include "Core.h"

#ifdef _WIN32

#if WIN32_USE_SEH
#define WIN32_LEAN_AND_MEAN			// exclude rarely-used services from windown headers
#define _WIN32_WINDOWS 0x0500		// for IsDebuggerPresent()
#include <windows.h>
#include <float.h>					// for _clearfp()
#endif // WIN32_USE_SEH


// Debugging options
//#define USE_DBGHELP				1
//#define EXTRA_UNDECORATE		1		// use different undecorate function, providing better results but not allowing to display static symbols
//#define DUMP_SEH				1		// for debugging SEH frames
#define GET_EXTENDED_INFO		1
//#define UNWIND_EBP_FRAMES		1


// Maximal crash analysis when VSTUDIO_INTEGRATION is set
#if VSTUDIO_INTEGRATION
#include <signal.h>

#undef  USE_DBGHELP
#undef  GET_EXTENDED_INFO
#undef  UNWIND_EBP_FRAMES
#define USE_DBGHELP				1
#define GET_EXTENDED_INFO		1
#define UNWIND_EBP_FRAMES		1

#endif // VSTUDIO_INTEGRATION

#ifdef _WIN64
#undef UNWIND_EBP_FRAMES				//!! should review the code and perhaps adopt to Win64
#endif

#if USE_DBGHELP
// prevent "warning C4091: 'typedef ': ignored on left of '' when no variable is declared" with Win7.1 SDK
#pragma warning(push)
#pragma warning(disable:4091)

#include <dbghelp.h>

#pragma warning(pop)
#endif // USE_DBGHELP


/*-----------------------------------------------------------------------------
	DBGHELP tools
-----------------------------------------------------------------------------*/

#if USE_DBGHELP

#pragma comment(lib, "dbghelp.lib")

static HANDLE hProcess;

static void InitSymbols()
{
	static bool initialized = false;
	if (initialized) return;
	initialized = true;

	// Article about using decorated and undecorated names at the same time:
	// http://www.microsoft.com/library/images/msdn/library/periodic/periodic/msj/hood897.htm

	SymSetOptions(
		SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS
#if EXTRA_UNDECORATE
		| SYMOPT_PUBLICS_ONLY		// this will disallow "private" symbols, and allow undecorated names
#else
		| SYMOPT_UNDNAME			// use the demangler, but function parameter info will be stripped
#endif
	);

	hProcess = GetCurrentProcess();
	SymInitialize(hProcess, NULL, TRUE);
}


#if EXTRA_UNDECORATE

// Strips all occurrences of string 'cut' from 'string'
static void StripPrefix(char* string, const char* cut)
{
	int len1 = strlen(string);
	int len2 = strlen(cut);
	int pos = 0;

	while (pos <= len1 - len2)
	{
		if (memcmp(string, cut, len2) != 0)
		{
			pos++;
			continue;
		}
		strcpy(string + pos, string + pos + len2);
		len1 -= len2;
	}
}

#endif // EXTRA_UNDECORATE


bool appSymbolName(address_t addr, char *buffer, int size)
{
	InitSymbols();

	char SymBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
	PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)SymBuffer;
	pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	pSymbol->MaxNameLen   = MAX_SYM_NAME;

	DWORD64 dwDisplacement = 0;
	if (SymFromAddr(hProcess, addr, &dwDisplacement, pSymbol))
	{
		char OffsetBuffer[32];
		if (dwDisplacement)
			appSprintf(ARRAY_ARG(OffsetBuffer), "+%X", dwDisplacement);
		else
			OffsetBuffer[0] = 0;

#if EXTRA_UNDECORATE
		char undecBuffer[256];
		if (UnDecorateSymbolName(pSymbol->Name, ARRAY_ARG(undecBuffer),
			UNDNAME_NO_LEADING_UNDERSCORES|UNDNAME_NO_LEADING_UNDERSCORES|UNDNAME_NO_ALLOCATION_LANGUAGE|UNDNAME_NO_ACCESS_SPECIFIERS))
		{
			StripPrefix(undecBuffer, "virtual ");
			StripPrefix(undecBuffer, "class ");
			StripPrefix(undecBuffer, "struct ");
			appSprintf(buffer, size, "%s%s", undecBuffer, OffsetBuffer);
		}
		else
		{
			appSprintf(buffer, size, "%s%s", pSymbol->Name, OffsetBuffer);
		}
#else
		appSprintf(buffer, size, "%s%s", pSymbol->Name, OffsetBuffer);
#endif // EXTRA_UNDECORATE
	}
	else
	{
		appSprintf(buffer, size, "%08X", addr);
	}
	return true;
}

#endif // USE_DBGHELP

const char *appSymbolName(address_t addr)
{
	static char	buf[256];

#if USE_DBGHELP
	if (appSymbolName(addr, ARRAY_ARG(buf)))
		return buf;
#endif

#if GET_EXTENDED_INFO
	HMODULE hModule = NULL;
	char moduleName[256];
	char *s;

	MEMORY_BASIC_INFORMATION mbi;
	if (!VirtualQuery((void*)addr, &mbi, sizeof(mbi)))
		goto simple;
	if (!(hModule = (HMODULE)mbi.AllocationBase))
		goto simple;
	if (!GetModuleFileName(hModule, ARRAY_ARG(moduleName)))
		goto simple;

//	if (s = strrchr(moduleName, '.'))	// cut extension
//		*s = 0;
	if (s = strrchr(moduleName, '\\'))
		strcpy(moduleName, s+1);		// remove "path\" part
	appSprintf(ARRAY_ARG(buf), "%s+0x%X", moduleName, (int)(addr - (size_t)hModule));
	return buf;
#endif // GET_EXTENDED_INFO

simple:
	appSprintf(ARRAY_ARG(buf), "%08X", addr);
	return buf;
}



/*-----------------------------------------------------------------------------
	Stack trace functions
-----------------------------------------------------------------------------*/

int appCaptureStackTrace(address_t* buffer, int maxDepth, int framesToSkip)
{
	return RtlCaptureStackBackTrace(framesToSkip, maxDepth, (void**)buffer, NULL);
}


void appDumpStackTrace(const address_t* buffer, int depth)
{
	for (int i = 0; i < depth; i++)
	{
		if (!buffer[i]) break;
		const char *symbol = appSymbolName(buffer[i]);
		appPrintf("    %s\n", symbol);
	}
}


/*-----------------------------------------------------------------------------
	Win32 exception handler (SEH)
-----------------------------------------------------------------------------*/

#if VSTUDIO_INTEGRATION
bool GUseDebugger = false;
#endif

#if WIN32_USE_SEH && DO_GUARD

/*
	SEH internals:

	http://www.microsoft.com/msj/0197/exception/exception.aspx
	russian version: http://www.wasm.ru/print.php?article=Win32SEHPietrek1
	http://www.codeproject.com/KB/cpp/exceptionhandler.aspx
	http://www.howzatt.demon.co.uk/articles/oct04.html
	http://www.rsdn.ru/article/baseserv/except.xml
	http://www.insidepro.com/kk/014/014r.shtml
	http://www.securitylab.ru/contest/212085.php?sphrase_id=862525
*/

#if DUMP_SEH

struct EXCEPTION_REGISTRATION
{
	EXCEPTION_REGISTRATION	*prev;
	void					*handler;
};


// Data structure(s) pointed to by Visual C++ extended exception frame
struct scopetable_entry
{
	DWORD					previousTryLevel;
	void					*lpfnFilter;
	void					*lpfnHandler;
};

// The extended exception frame used by Visual C++
struct VC_EXCEPTION_REGISTRATION : EXCEPTION_REGISTRATION
{
	scopetable_entry		*scopetable;
	int						trylevel;
	int						_ebp;
};

extern "C" void _except_handler3();

// Display the information in one exception frame, along with its scopetable
static void ShowSEHFrame(VC_EXCEPTION_REGISTRATION * pVCExcRec)
{
	// note: handler may be inside kernel, and it will not use VC_EXCEPTION_REGISTRATION structures!
	bool isCpp = pVCExcRec->handler == _except_handler3;
	printf("Frame: %08X  Handler: %s  Prev: %08X", pVCExcRec, appSymbolName((address_t)pVCExcRec->handler), pVCExcRec->prev);
	if (isCpp) printf(" Scopetable: %08X [%d]", pVCExcRec->scopetable, pVCExcRec->trylevel);
	printf("\n");
	if (!isCpp) return;

	scopetable_entry *pScopeTableEntry = pVCExcRec->scopetable;
	for (int i = 0; i <= pVCExcRec->trylevel; i++, pScopeTableEntry++)
	{
		char filter[256], handler[256];
		strcpy(filter, appSymbolName((address_t)pScopeTableEntry->lpfnFilter));
		strcpy(handler, appSymbolName((address_t)pScopeTableEntry->lpfnHandler));
		printf("    scopetable[%i] PrevTryLevel: %08X  filter: %s  __except: %s\n", i, pScopeTableEntry->previousTryLevel, filter, handler);
	}
}

static void DumpSEH()
{
	printf("\n");
	VC_EXCEPTION_REGISTRATION *pVCExcRec;
	__asm
	{
		mov		eax, fs:[0]
		mov		pVCExcRec, eax
	}
	while ((unsigned)pVCExcRec != 0xFFFFFFFF)
    {
		ShowSEHFrame(pVCExcRec);
		pVCExcRec = (VC_EXCEPTION_REGISTRATION*)(pVCExcRec->prev);
	}
	printf("\n");
}

#endif // DUMP_SEH


static void DropSEHFrames()
{
#ifndef _WIN64
	__asm
	{
		push	edx
		push	ebx
		// get current frame
		mov		eax, fs:[0]				// points to frame in this function (this function has TRY/CATCH block)
		mov		eax, [eax]				// frame in Win32 exception handler
		mov		edx, eax				// use this frame later
		// find outermost frame
	_loop:
		mov		ebx, eax				// pointer to last valid frame
		mov		eax, [eax]
		cmp		eax, -1					// "last frame" marker
		jne		_loop
		mov		[edx], ebx				// this will skip all intermediate frames
		pop		ebx
		pop		edx
	}
#endif // _WIN64
}


#if UNWIND_EBP_FRAMES
void UnwindEbpFrame(const address_t *data)
{
	void *pStackVar = _alloca(1);		// Use _alloca() to get the current stack pointer
	MEMORY_BASIC_INFORMATION mbi;
	if (!VirtualQuery(pStackVar, &mbi, sizeof(mbi)))
		return;

	address_t stackStart = (address_t)mbi.BaseAddress;	// AllocationBase has wrong value here
	address_t stackEnd   = stackStart + mbi.RegionSize;
//	printf("MBI: BaseAddress=%08X AllocationBase=%08X RegionSize=%08X\n", mbi.BaseAddress, mbi.AllocationBase, mbi.RegionSize);
//	printf("data=%08X var=%08X stack = [%08X .. %08X]\n", data, pStackVar, stackStart, stackEnd);

	int level = 0;
	while (true)
	{
		if ((address_t)data < stackStart || (address_t)data >= stackEnd)
			break;						// not a stack pointer

		address_t pNext = data[0];
		address_t pFunc = data[1];

		if (IsBadCodePtr((FARPROC)pFunc))
			break;						// not points to code
		const char *symbol = appSymbolName(pFunc);
		if (!level) appPrintf("\nCall stack:\n");
		appPrintf("    %s\n", symbol);
		if (pNext <= (address_t)data)	// next frame is shifted in a wrong direction
			break;
		data = (address_t*)pNext;
		level++;
	}
	if (level) appPrintf("\n\n");
}
#endif // UNWIND_EBP_FRAMES


long win32ExceptFilter(struct _EXCEPTION_POINTERS *info)
{
#if VSTUDIO_INTEGRATION
	static bool skipAllHandlers = false;
	if (skipAllHandlers) return EXCEPTION_CONTINUE_SEARCH;	// drop to outermost handler
#endif

	static int dumped = false;
	if (dumped) return EXCEPTION_EXECUTE_HANDLER;			// error will be handled only once
	// NOTE: side effect of line above: we will not able to catch GPF-like recursive errors

#if DUMP_SEH
	DumpSEH();
#endif

	// WARNING: recursive error will not be found
	// If we will disable line above, will be dumped context for each appUnwind() entry
	dumped = true;

#if VSTUDIO_INTEGRATION
	if (GUseDebugger || IsDebuggerPresent())
	{
		SetErrorMode(0);					// without this crash will not be reported
		SetUnhandledExceptionFilter(NULL);	// just in case
		UnhandledExceptionFilter(info);		// invoke debugger
		if (IsDebuggerPresent())
		{
			// System has loaded a debugger.
			// Here we are removing almost all __try/__except frames from the SEH chain.
			// We are doing so to ensure correct handling of error in debugger which will
			// be attached after process crash.
			skipAllHandlers = true;
			DropSEHFrames();
	#if DUMP_SEH
			DumpSEH();
	#endif
			return EXCEPTION_CONTINUE_SEARCH;
		}
		// the debugger was not loaded
		// continue guard chain to unroll call stack
		return EXCEPTION_EXECUTE_HANDLER;
	}
#endif // VSTUDIO_INTEGRATION

	if (GError.IsSwError) return EXCEPTION_EXECUTE_HANDLER;		// no interest to thread context when software-generated errors

	// if FPU exception occurred, _clearfp() is required (otherwise, exception will be re-raised again)
	_clearfp();


	__try {
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
			excName = "Privileged instruction";
			break;
		case EXCEPTION_ILLEGAL_INSTRUCTION:
			excName = "Illegal opcode";
			break;
		case EXCEPTION_STACK_OVERFLOW:
			excName = "Stack overflow";
			break;
		case EXCEPTION_BREAKPOINT:
			excName = "Breakpoint";
			break;
		}

		// log error
		CONTEXT* ctx = info->ContextRecord;
#ifndef _WIN64
		appSprintf(ARRAY_ARG(GError.History), "%s (%08X) at %s\n",
			excName, info->ExceptionRecord->ExceptionCode, appSymbolName(ctx->Eip)
		);
#else
		appSprintf(ARRAY_ARG(GError.History), "%s (%08X) at %s\n",
			excName, info->ExceptionRecord->ExceptionCode, appSymbolName(ctx->Rip)
		);
#endif // _WIN64
#if UNWIND_EBP_FRAMES
		UnwindEbpFrame((address_t*) ctx->Ebp);
#elif VSTUDIO_INTEGRATION
		address_t stackTrace[64];
		appCaptureStackTrace(ARRAY_ARG(stackTrace), 7);
		appPrintf("\nCall stack:\n");
		appDumpStackTrace(ARRAY_ARG(stackTrace));
#endif // UNWIND_EBP_FRAMES
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		// do nothing
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

#endif // WIN32_USE_SEH

#if VSTUDIO_INTEGRATION

static void AbortHandler(int signal)
{
	appPrintf("abort() called");
	DebugBreak();
}

#endif // VSTUDIO_INTEGRATION

void appInitPlatform()
{
#if VSTUDIO_INTEGRATION
	// Win32 UI code doesn't allow us to use SEH, and any assert() will call abort() from CxxThrowException().
	// To catch such exceptions, hook abort() function.
	signal(SIGABRT, AbortHandler);
#endif // VSTUDIO_INTEGRATION
	// Increase standard 512 open file limit. Note: it seems it can't be increased more than 2048 (stackoverflow says).
	//_setmaxstdio(1024);
}

void appCopyTextToClipboard(const char* text)
{
#if HAS_UI
	if (!OpenClipboard(0)) return;

	// We should insert CR character before each LF in order to allow this text to be copied
	// to any Windows application without problem. Count number of lines first.
	int len = strlen(text);
	int i, numLines = 0;
	for (i = 0; i < len; i++)
		if (text[i] == '\n') numLines++;

	EmptyClipboard();
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len + numLines + 1);
	char* data = (char*)GlobalLock(hMem);

	// copy string with CRLF expansion
	for (i = 0; i <= len; i++)
	{
		char c = text[i];
		if (c == '\n')
		{
			*data++ = '\r';
		}
		*data++ = c;
	}

	GlobalUnlock(hMem);
	SetClipboardData(CF_TEXT, hMem);
	CloseClipboard();
#endif // HAS_UI
}


#if defined(OLDCRT) && (_MSC_VER  >= 1900)

// Support OLDCRT with VC2015 or newer. VC2015 switched to another CRT model called "Universal CRT".
// It has some incompatibilities in header files.

// Access stdin/stdout/stderr.
// UCRT uses __acrt_iob_func(). Older CRT used __iob_func. Also, older CRT used "_iobuf" structure with
// alias "FILE", however "FILE" in UCRT is just a pointer to something internal structure.

// Define __iob_func locally because it is missing in UCRT
extern "C" __declspec(dllimport) FILE* __cdecl __iob_func();

// Size of FILE structure for VS2013 and older
enum { CRT_FILE_SIZE = (sizeof(char*)*3 + sizeof(int)*5) };

// Note that originally this function has dll linkage. We're removing it with _ACRTIMP_ALT="" define
// in project settings. It is supposed to work correctly as stated in include/.../ucrt/corecrt.h
// Without that define, both compiler and linker will issue warnings about inconsistent dll linkage.
// Code would work, however compiler will generate call to __acrt_iob_func via function pointer, and
// it will add this function to executable exports.

extern "C" FILE* __cdecl __acrt_iob_func(unsigned Index)
{
	return (FILE*)((char*)__iob_func() + Index * Align(CRT_FILE_SIZE, sizeof(char*)));
}

// Required for Oodle, the CRT implementation just forwards the call to terminate()
void terminate();

extern "C" void __std_terminate()
{
	terminate();
}

#endif // OLDCRT

#endif // _WIN32
