#include "Core.h"
#include "Parallel.h"

#ifdef _WIN32

#include <Windows.h>
#include <process.h> // _beginthread

CMutex::CMutex()
{
	static_assert(CriticalSectionSize == sizeof(RTL_CRITICAL_SECTION), "Review CriticalSectionSize");
	InitializeCriticalSection((LPCRITICAL_SECTION)&data);
}

CMutex::~CMutex()
{
	DeleteCriticalSection((LPCRITICAL_SECTION)&data);
}

void CMutex::Lock()
{
	EnterCriticalSection((LPCRITICAL_SECTION)&data);
}

bool CMutex::TryLock()
{
	return TryEnterCriticalSection((LPCRITICAL_SECTION)&data) == 0;
}

void CMutex::Unlock()
{
	LeaveCriticalSection((LPCRITICAL_SECTION)&data);
}

CThread::CThread()
{
	thread = _beginthread(ThreadFunc, 0, this);
}

CThread::~CThread()
{
}

/*static*/ void CThread::Sleep(int milliseconds)
{
	::Sleep(milliseconds);
}

#else // linux

#include <pthread.h>

CMutex::CMutex()
{
	static_assert(MutexSize >= sizeof(pthread_mutex_t));
	// Make a recursive mutex
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init((pthread_mutex_t*)&data, &attr);
	pthread_mutexattr_destroy(&attr);
}

CMutex::~CMutex()
{
	pthread_mutex_destroy((pthread_mutex_t*)&data);
}

void CMutex::Lock()
{
	pthread_mutex_lock((pthread_mutex_t*)&data);
}

bool CMutex::TryLock()
{
	return pthread_mutex_trylock((pthread_mutex_t*)&data) == 0;
}

void CMutex::Unlock()
{
	pthread_mutex_unlock((pthread_mutex_t*)&data);
}

CThread::CThread()
{
	static_assert(ThreadSize >= sizeof(pthread_t));
	typedef void* (*start_routine_t)(void*);
	pthread_create((pthread_t*)&thread, NULL, (start_routine_t)ThreadFunc, this);
}

CThread::~CThread()
{
}

/*static*/ void CThread::Sleep(int milliseconds)
{
	SDL_Delay(milliseconds); //todo: implement in other way
}

#endif // windows / linux

/*static*/ void CThread::ThreadFunc(void* param)
{
	guard(CThread::ThreadFunc);
	CThread* thread = (CThread*)param;
	thread->Run();
	unguard;
}
