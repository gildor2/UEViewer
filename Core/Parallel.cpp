#include "Core.h"

#if THREADING

#ifdef _WIN32

#include <Windows.h>
#include <process.h> // _beginthread

#endif // _WIN32

#include "Parallel.h"

bool GEnableThreads = true;

volatile int CThread::NumThreads = 0;

/*-----------------------------------------------------------------------------
	Generic classes
-----------------------------------------------------------------------------*/

#if _WIN32

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

//todo: not called automatically when thread function completed
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
		// Lock other threads - only one will raise the error
		//todo: Note: if multiple threads will crash, they'll corrupt error history with
		//   simultaneous writing to GError.
		//todo: drop appNotify context, it's meaningless in thread
		static volatile int lock;
		if (InterlockedAdd(&lock, 1) != 0)
		{
			while (true) Sleep(1000);
		}

		appPrintf("\nException in thread %d:\n", CurrentId());
		GError.StandardHandler();

		//todo: there's no build number displayed, Core has no access to version string
		exit(1);
	}
}


/*-----------------------------------------------------------------------------
	Thread pool
-----------------------------------------------------------------------------*/

namespace ThreadPool
{

#define MAX_POOL_THREADS 64

// Task queue
namespace Queue
{

struct CTask
{
	ThreadTask	task;
	void*		data;
	CSemaphore* fence;

	void Exec()
	{
		guard(QueueTask::Exec);
		task(data);
		if (fence) fence->Signal();
		unguard;
	}
};


CMutex Mutex;
static CTask Queue[MAX_POOL_THREADS / 2];
static int QueueSize = 0;

// This function returns false if queue size wasn't enough for returning immediately
bool PutToQueue(ThreadTask task, void* data, CSemaphore* fence)
{
	guard(Queue::PutToQueue);

	// Get size of queue
	static int MaxQueueSize = -1;
	if (MaxQueueSize < 0)
	{
		MaxQueueSize = CThread::GetLogicalCPUCount() * 2;
		if (MaxQueueSize > ARRAY_COUNT(Queue))
			MaxQueueSize = ARRAY_COUNT(Queue);
	}

	CTask ToExec;

	{ // Lock scope
		CMutex::ScopedLock Lock(Mutex);

		if (QueueSize < MaxQueueSize)
		{
			// Just put task to queue and return
			CTask& Item = Queue[QueueSize++];
			Item.task = task;
			Item.data = data;
			Item.fence = fence;
			return true;
		}

		// There's not enough space in queue, execute the first item and put new one to the queue's back

		// Grab the first item from queue
		ToExec = Queue[0];
		memmove(&Queue[0], &Queue[1], sizeof(CTask) * (QueueSize - 1));

		// Put new item
		CTask& Item = Queue[QueueSize - 1];
		Item.task = task;
		Item.data = data;
		Item.fence = fence;
	}

	// Execute the first item in current thread
	ToExec.Exec();

	// Indicate that we didn't put task to pool, but executed something
	return false;

	unguard;
}

bool GetFromQueue(CTask& item)
{
	CMutex::ScopedLock Lock(Mutex);

	if (!QueueSize)
		return false;

	item = Queue[0];
	if (--QueueSize)
	{
		memmove(&Queue[0], &Queue[1], sizeof(CTask) * QueueSize);
	}

	return true;
}

} // namespace Queue


// Thread for the pool
class CPoolThread : public CThread
{
public:
	// Flag showing if this thead is doing something or not
	bool bBusy = false;
	bool bShutdown = false;

	void AssignTaskAndWake(ThreadTask inTask, void* inData, CSemaphore* inFence)
	{
		task = inTask;
		taskData = inData;
		fence = inFence;
		sem.Signal();
	}

	void Shutdown()
	{
		bShutdown = true;
		sem.Signal();
	}

protected:
	virtual void Run()
	{
		bBusy = false;

		while (true)
		{
			// Wait for signal to start
			sem.Wait();
			if (bShutdown) break;

			// Execute task
			bBusy = true;
			task(taskData);
			if (fence) fence->Signal();

			// See if there's something in the queue to execute
			Queue::CTask task;
			while (Queue::GetFromQueue(task))
			{
				task.Exec();
			}

			// Return thread to pool
			bBusy = false;
			InterlockedDecrement(&NumWorkingThreads);
		}
		//todo: May be CThread should destroy itself when worker function completed? Just not using CThread anywhere else.
		delete this;
	}

	// Executed code
	ThreadTask task;
	// Pointer to task context
	void* taskData;
	// Object used to signal execution end
	CSemaphore* fence;
	// Semaphore used to wake up the thread
	CSemaphore sem;

public:
	// Statics
	// Number of awake threads
	static volatile int NumWorkingThreads;
};

volatile int CPoolThread::NumWorkingThreads = 0;

// Thread pool itself
namespace Pool
{

static CPoolThread* Pool[MAX_POOL_THREADS];
static int NumPoolThreads = 0;
static CMutex Mutex;

CPoolThread* AllocateFreeThread()
{
	CMutex::ScopedLock lock(Mutex);

	// Find idle thread
	for (int i = 0; i < NumPoolThreads; i++)
	{
		CPoolThread* Thread = Pool[i];
		if (!Thread->bBusy)
		{
			Thread->bBusy = true;
			InterlockedIncrement(&CPoolThread::NumWorkingThreads);
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

		// Put Shutdown function to 'atexit' sequence
		atexit(ThreadPool::Shutdown);
	}

	// Create new thread, if can
	if (NumPoolThreads < MaxThreads)
	{
		CPoolThread* NewThread = new CPoolThread();
		NewThread->bBusy = true;
		Pool[NumPoolThreads++] = NewThread;
		InterlockedIncrement(&CPoolThread::NumWorkingThreads);
		return NewThread;
	}

	// No free threads, and can't allocate a new one
	return NULL;
}

} // namespace Pool

bool ExecuteInThread(ThreadTask task, void* taskData, CSemaphore* fence, bool allowQueue)
{
	CPoolThread* thread = Pool::AllocateFreeThread();
	if (thread)
	{
		thread->AssignTaskAndWake(task, taskData, fence);
		return true;
	}
	else if (allowQueue)
	{
		Queue::PutToQueue(task, taskData, fence);
		//todo: it is possible that after AllocateFreeThread() call buf before PutToQueue completion, one
		//todo: of the worker threads will be returned to pool
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

	// Execute tasks from the queue if any
	Queue::CTask task;
	while (Queue::GetFromQueue(task))
	{
		task.Exec();
	}

	// Wait for all threads to go to the idle state
	while (CPoolThread::NumWorkingThreads > 0)
	{
		CThread::Sleep(20);
	}
	unguard;
}

void Shutdown()
{
	guard(ThreadPool::Shutdown);

	if (GError.HasError())
	{
		// Do not bother the proper shutdown of threading system in a case of error
		return;
	}

	WaitForCompletion();
	int NumThreadsAfterShutdown = CThread::NumThreads - Pool::NumPoolThreads;

	// Signal to all threads to shutdown
	for (int i = 0; i < Pool::NumPoolThreads; i++)
	{
		CPoolThread* Thread = Pool::Pool[i];
		Thread->Shutdown();
	}

	// Wait them to terminate
	int NumAttempts = 200;
	while (CThread::NumThreads > NumThreadsAfterShutdown && NumAttempts-- > 0)
	{
		CThread::Sleep(20);
	}
	if (NumAttempts == 0)
	{
		appPrintf("Warning: unable to shutdown %d threads\n", CThread::NumThreads);
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
, bAllSentToThreads(false)
, currentIndex(0)
, lastIndex(inCount)
{}

ParallelForBase::~ParallelForBase()
{
	// Wait for completion: the last thread which will complete the job will
	// signal 'endSignal' about completion.
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

	// Divide index count by 'step' with rounding up
	int numThreads = (lastIndex + step - 1) / step;

	// Recompute step to avoid having tiny last step
	step = (lastIndex + numThreads - 1) / numThreads;
	assert(step > 0);

	// Clamp numThreads by available thread count
	if (numThreads > maxThreads)
		numThreads = maxThreads;

#if DEBUG_PARALLEL_FOR
	printf("ParallelFor: %d, %d, step %d (thread %d)\n", currentIndex, lastIndex, step, CThread::CurrentId()&255);
#endif

	// Allocate threads, exclude 1 thread for the thread executing ParallelFor

	// Reserve 1 "active thread", so if the 1st spawned thread will finish the job before
	// we'll be able to spawn anything else - it won't send "completed" signal.
	InterlockedIncrement(&numActiveThreads);

	// Spawn as many threads as planned. Stop allocating new threads if job will be completed
	// faster than planned (verify bAllSentToThreads as a loop condition).
	for (int i = 0; i < numThreads - 1 && !bAllSentToThreads; i++)
	{
		// Just in case, increment thread count before starting a thread, so we'll avoid
		// the situation when worker thread will execute everything before we'll return
		// to the invoker (main) thread and increment count.
		InterlockedIncrement(&numActiveThreads);
		if (!ThreadPool::ExecuteInThread(worker, this))
		{
			// All threads has been allocated, can't spawn more.
			// Decrement the thread count as we've failed to use new thread.
			InterlockedDecrement(&numActiveThreads);
			break;
		}
	}

	// Remove thread "reservation". If at this moment everything has been completed, the
	// value goes to zero, and we'll not perform waiting for completion in destructor. If
	// something is still works, then we'll wait, and the worker thread will send a signal
	// when count finally decremented to 0.
	InterlockedDecrement(&numActiveThreads);
}

bool ParallelForBase::GrabInterval(int& idx1, int& idx2)
{
	CMutex::ScopedLock lock(mutex);
	idx1 = currentIndex;
	if (idx1 >= lastIndex)
	{
		// All items were sent to threads
		bAllSentToThreads = true;
		return false;
	}
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

#endif // THREADING
