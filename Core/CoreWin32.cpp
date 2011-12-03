#include "Core.h"

#ifdef _WIN32

#if WIN32_USE_SEH
#define WIN32_LEAN_AND_MEAN			// exclude rarely-used services from windown headers
#define _WIN32_WINDOWS 0x0500		// for IsDebuggerPresent()
#include <windows.h>
#include <float.h>					// for _clearfp()
#endif // WIN32_USE_SEH


//#define USE_DBGHELP			1
//#define DUMP_SEH			1		// for debugging SEH frames

// use dbghelp tools by default when VSTUDIO_INTEGRATION is set
#if !defined(USE_DBGHELP) && defined(VSTUDIO_INTEGRATION)
#define USE_DBGHELP			VSTUDIO_INTEGRATION
#endif

#if USE_DBGHELP
#include <dbghelp.h>
#endif


/*-----------------------------------------------------------------------------
	DBGHELP tools
-----------------------------------------------------------------------------*/

typedef size_t		address_t;

#if USE_DBGHELP

#pragma comment(lib, "dbghelp.lib")

static HANDLE hProcess;

static void InitSymbols()
{
	static bool initialized = false;
	if (initialized) return;
	initialized = true;

	SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);

	hProcess = GetCurrentProcess();
	SymInitialize(hProcess, NULL, TRUE);
}


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

		appSprintf(buffer, size, "%s%s", pSymbol->Name, OffsetBuffer);
	}
	else
	{
		appSprintf(buffer, size, "%08X", addr);
	}
	return true;
}

#endif

const char *appSymbolName(address_t addr)
{
	static char	buf[256];

#if USE_DBGHELP
	if (!appSymbolName(addr, ARRAY_ARG(buf)))
#endif
		appSprintf(ARRAY_ARG(buf), "%08X", addr);
	return buf;
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
}


long WINAPI win32ExceptFilter(struct _EXCEPTION_POINTERS *info)
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
	if (GUseDebugger)
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

	if (GIsSwError) return EXCEPTION_EXECUTE_HANDLER;		// no interest to thread context when software-generated errors

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
		appSprintf(ARRAY_ARG(GErrorHistory), "%s (%08X) at %s\n",
			excName, info->ExceptionRecord->ExceptionCode, appSymbolName(ctx->Eip)
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

#endif // _WIN32
