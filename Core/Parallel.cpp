#include "Core.h"
#include "Parallel.h"

bool GEnableThreads = true;

int CThread::NumThreads = 0;

/*-----------------------------------------------------------------------------
	Generic classes
-----------------------------------------------------------------------------*/

#ifdef _WIN32

#include <Windows.h>
#include <process.h> // _beginthread

// Windows.h has InterlockedIncrement/Decrement defines, hide then
#undef InterlockedIncrement
#undef InterlockedDecrement

#if TRACY_DEBUG_MUTEX
static tracy::SourceLocationData DummyStrLoc({ nullptr, "CMutex Var", __FILE__, __LINE__, 0 });
#endif

CMutex::CMutex()
#if TRACY_DEBUG_MUTEX
: tracy(&DummyStrLoc)
#endif
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
#if TRACY_DEBUG_MUTEX
	bool b = tracy.BeforeLock();
#endif
	EnterCriticalSection((LPCRITICAL_SECTION)&data);
#if TRACY_DEBUG_MUTEX
	if (b) tracy.AfterLock();
#endif
}

bool CMutex::TryLock()
{
	bool bAcquired = TryEnterCriticalSection((LPCRITICAL_SECTION)&data) == 0;
#if TRACY_DEBUG_MUTEX
	tracy.AfterTryLock(bAcquired);
#endif
	return bAcquired;
}

void CMutex::Unlock()
{
	LeaveCriticalSection((LPCRITICAL_SECTION)&data);
#if TRACY_DEBUG_MUTEX
	tracy.AfterUnlock();
#endif
}

CSemaphore::CSemaphore()
#if TRACY_DEBUG_MUTEX
: tracy(&DummyStrLoc)
#endif
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
#if TRACY_DEBUG_MUTEX
	tracy.AfterUnlock();
#endif
}

void CSemaphore::Wait()
{
#if TRACY_DEBUG_MUTEX
	bool b = tracy.BeforeLock();
#endif
	WaitForSingleObject(data, INFINITE);
#if TRACY_DEBUG_MUTEX
	if (b) tracy.AfterLock();
#endif
}

CThread::CThread()
{
	InterlockedIncrement(&NumThreads);
	thread = _beginthread(ThreadFunc, 0, this);
}

CThread::~CThread()
{
	InterlockedDecrement(&NumThreads);
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
	static_assert(sizeof(data) >= sizeof(sem_t), "Review SemSize");
	sem_init((sem_t*)&data, 0, 0);
}

CSemaphore::~CSemaphore()
{
	sem_destroy((sem_t*)data);
}

void CSemaphore::Signal()
{
	sem_post((sem_t*)data);
}

void CSemaphore::Wait()
{
	sem_wait((sem_t*)data);
}

CThread::CThread()
{
	InterlockedIncrement(&NumThreads);
	static_assert(sizeof(thread) >= sizeof(pthread_t), "Review ThreadSize");
	typedef void* (*start_routine_t)(void*);
	pthread_create((pthread_t*)&thread, NULL, (start_routine_t)ThreadFunc, this);
}

CThread::~CThread()
{
	InterlockedDecrement(&NumThreads);
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


/*-----------------------------------------------------------------------------
	Thread pool
-----------------------------------------------------------------------------*/

namespace ThreadPool
{

static int GNumWorkingThreads = 0;

class CPoolThread : public CThread
{
public:
	// Flag showing if this thead is doing something or not
	bool bBusy = false;

	void AssignTaskAndWake(ThreadTask inTask, void* inData, CSemaphore* inFence)
	{
		task = inTask;
		taskData = inData;
		fence = inFence;
		sem.Signal();
	}

protected:
	virtual void Run()
	{
		bBusy = false;

		while (true) //todo: there's no "exit" condition
		{
			// Wait for signal to start
			sem.Wait();
			// Execute task
			bBusy = true;
			task(taskData);
			if (fence) fence->Signal();
			// Return thread to pool
			bBusy = false;
			InterlockedDecrement(&GNumWorkingThreads);
			//todo: Signal that task was completed?
		}
	}

	// Executed code
	ThreadTask task;
	// Pointer to task context
	void* taskData;
	// Object used to signal execution end
	CSemaphore* fence;
	// Semaphore used to wake up the thread
	CSemaphore sem;
};

#define MAX_POOL_THREADS 64

static CPoolThread* GThreadPool[MAX_POOL_THREADS];
static int GNumAllocatedThreads = 0;

static CMutex GPoolMutex;

CPoolThread* AllocateFreeThread()
{
	CMutex::ScopedLock lock(GPoolMutex);

	// Find idle thread
	for (int i = 0; i < GNumAllocatedThreads; i++)
	{
		CPoolThread* Thread = GThreadPool[i];
		if (!Thread->bBusy)
		{
			Thread->bBusy = true;
			InterlockedIncrement(&GNumWorkingThreads);
			return Thread;
		}
	}

	static int MaxThreads = -1;
	if (MaxThreads < 0)
	{
		MaxThreads = CThread::GetLogicalCPUCount();
		MaxThreads = min(MaxThreads, MAX_POOL_THREADS);
		--MaxThreads; // exclude main thread
		if (!GEnableThreads) MaxThreads = 0;
	}

	// Create new thread, if can
	if (GNumAllocatedThreads < MaxThreads)
	{
		CPoolThread* NewThread = new CPoolThread();
		NewThread->bBusy = true;
		GThreadPool[GNumAllocatedThreads++] = NewThread;
		InterlockedIncrement(&GNumWorkingThreads);
		return NewThread;
	}

	// No free threads, and can't allocate a new one
	return NULL;
}

bool ExecuteInThread(ThreadTask task, void* taskData, CSemaphore* fence)
{
	CPoolThread* thread = AllocateFreeThread();
	if (thread)
	{
		thread->AssignTaskAndWake(task, taskData, fence);
		return true;
	}
	else
	{
		return false;
	}
}

void WaitForCompletion()
{
	guard(ThreadPool::WaitForCompletion);
	while (GNumWorkingThreads > 0)
	{
		CThread::Sleep(20);
	}
	unguard;
}

} // namespace ThreadPool


/*-----------------------------------------------------------------------------
	ParallelFor
-----------------------------------------------------------------------------*/

namespace ParallelForImpl
{

ParallelForBase::ParallelForBase(int inCount)
: numActiveThreads(0)
, currentIndex(0)
, lastIndex(inCount)
{}

ParallelForBase::~ParallelForBase()
{
	// Wait for completion
	if (numActiveThreads)
	{
		guard(ParallelForWait);
		endSignal.Wait();
		unguard;
	}
}

void ParallelForBase::Start(ThreadPool::ThreadTask worker)
{
	if (lastIndex == 0)
		return;

	//todo: review code
	int maxThreads = CThread::GetLogicalCPUCount();
	int stepDivisor = maxThreads * 20;		// assume each thread will request for data 20 times
	step = (lastIndex + stepDivisor - 1) / stepDivisor;
	if (step < 20) step = 20; //?? should override for slow tasks, e.g. processing 10 items 1 second each

	int numThreads = (lastIndex + step - 1) / step;

	// Recompute step to avoid having tiny last step
	step = (lastIndex + numThreads - 1) / numThreads;
	assert(step > 0);

	// Clamp numThreads
	if (numThreads > maxThreads)
		numThreads = maxThreads;

#if DEBUG_PARALLEL_FOR
	printf("ParallelFor: %d, %d, step %d (thread %d)\n", currentIndex, lastIndex, step, CThread::CurrentId()&255);
#endif

	// Allocate threads, exclude 1 thread for the thread executing ParallelFor
	for (int i = 0; i < numThreads - 1; i++)
	{
		if (!ThreadPool::ExecuteInThread(worker, this))
		{
			break; // all threads were allocated
		}
		InterlockedIncrement(&numActiveThreads);
	}
}

bool ParallelForBase::GrabInterval(int& idx1, int& idx2)
{
	CMutex::ScopedLock lock(mutex);
	idx1 = currentIndex;
	if (idx1 >= lastIndex)
		return false; // all done
	idx2 = idx1 + step;
	if (idx2 > lastIndex)
		idx2 = lastIndex;
	currentIndex = idx2;
#if DEBUG_PARALLEL_FOR
	printf("thread %d: GET %d .. %d [%d]\n", CThread::CurrentId()&255, idx1, idx2, idx2 - idx1);
#endif
	return true;
}

} // namespace ParallelForImpl
