#include "Core.h"
#include "Parallel.h"

#ifdef _WIN32

#include <Windows.h>
#include <process.h> // _beginthread

CMutex::CMutex()
{
	static_assert(CriticalSectionSize >= sizeof(RTL_CRITICAL_SECTION), "Review CriticalSectionSize");
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

CSemaphore::CSemaphore()
{
	data = CreateSemaphore(NULL, 0, 32768, NULL);
}

CSemaphore::~CSemaphore()
{
	CloseHandle(data);
}

void CSemaphore::Signal()
{
	ReleaseSemaphore(data, 1, NULL);
}

void CSemaphore::Wait()
{
	WaitForSingleObject(data, INFINITE);
}

CThread::CThread()
{
	thread = _beginthread(ThreadFunc, 0, this);
}

CThread::~CThread()
{
}

/*static*/ int CThread::CurrentId()
{
	return GetCurrentThreadId();
}

/*static*/ void CThread::Sleep(int milliseconds)
{
	::Sleep(milliseconds);
}

/*static*/ int CThread::GetLogicalCPUCount()
{
	static int MaxThreads = -1;
	if (MaxThreads < 0)
	{
    	SYSTEM_INFO info;
	    GetNativeSystemInfo(&info);
		MaxThreads = info.dwNumberOfProcessors;
	}
    return MaxThreads;
}

#else // linux

#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>

CMutex::CMutex()
{
	static_assert(sizeof(data) >= sizeof(pthread_mutex_t), "Review MutexSize");
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

CSemaphore::CSemaphore()
{
	printf("init_sem(%p)\n", &data);
	static_assert(sizeof(data) >= sizeof(sem_t), "Review SemSize");
	sem_init((sem_t*)&data, 0, 0);
}

CSemaphore::~CSemaphore()
{
	printf("del_sem(%p)\n", &data);
	sem_destroy((sem_t*)data);
}

void CSemaphore::Signal()
{
	printf("sig_sem(%p)\n", &data);
	sem_post((sem_t*)data);
}

void CSemaphore::Wait()
{
	printf("wait_sem(%p)\n", &data);
	sem_wait((sem_t*)data);
}

CThread::CThread()
{
	static_assert(sizeof(thread) >= sizeof(pthread_t), "Review ThreadSize");
	typedef void* (*start_routine_t)(void*);
	pthread_create((pthread_t*)&thread, NULL, (start_routine_t)ThreadFunc, this);
}

CThread::~CThread()
{
}

/*static*/ int CThread::CurrentId()
{
	return (int)pthread_self() >> 6;
}

/*static*/ void CThread::Sleep(int milliseconds)
{
	SDL_Delay(milliseconds); //todo: implement in other way
}

/*static*/ int CThread::GetLogicalCPUCount()
{
	static int MaxThreads = -1;
	if (MaxThreads < 0)
		MaxThreads = sysconf(_SC_NPROCESSORS_ONLN);
    return MaxThreads;
}

#endif // windows / linux

/*static*/ void CThread::ThreadFunc(void* param)
{
	TRY {
		CThread* thread = (CThread*)param;
		thread->Run();
	} CATCH_CRASH {
		appPrintf("Exception in thread %d: ", CurrentId());
		GError.StandardHandler();
		exit(1);
	}
}
