#ifndef __PARALLEL_H__
#define __PARALLEL_H__

#if THREADING

/*-----------------------------------------------------------------------------
	Generic classes
-----------------------------------------------------------------------------*/

//todo: I wasn't able to make this stuff working - profiler GUI crashes with that
//#define TRACY_DEBUG_MUTEX 1

#ifndef TRACY_ENABLE
#undef TRACY_DEBUG_MUTEX
#endif

class CMutex
{
public:
	CMutex();
	~CMutex();

	void Lock();
	bool TryLock();
	void Unlock();

	class ScopedLock
	{
		CMutex& pMutex;
	public:
		ScopedLock(CMutex& mutex)
		: pMutex(mutex)
		{
			mutex.Lock();
		}
		~ScopedLock()
		{
			pMutex.Unlock();
		}
	};

protected:

	bool bInitialized = false;
#ifdef _WIN32
	enum { CriticalSectionSize = sizeof(void*)*4 + sizeof(int)*2 }; // sizeof(RTL_CRITICAL_SECTION)
	char data[CriticalSectionSize];
#else
	enum { MutexSize = 40 }; // __SIZEOF_PTHREAD_MUTEX_T
	size_t data[MutexSize / sizeof(size_t)];
#endif
#if TRACY_DEBUG_MUTEX
	tracy::LockableCtx tracy;
#endif
};

class CSemaphore
{
public:
	CSemaphore();
	~CSemaphore();

	void Signal();
	void Wait();

protected:

#ifdef _WIN32
	void* data;
#else
	enum { SemSize = sizeof(size_t) * 4 };
	size_t data[SemSize / sizeof(size_t)];
#endif
#if TRACY_DEBUG_MUTEX
	tracy::LockableCtx tracy;
#endif
};

// Note: we're using "virtual Run()", which is initialized in derived class. Therefore, CThread creates a
// suspended thread, which must be manually started after derived class is fully set up using "Start()" method.
class CThread
{
public:
	CThread();
	virtual ~CThread();

	// The thread is created suspended to ensure VMT etc are properly set. Use "Start" call to initiate execution.
	void Start();

	virtual void Run() = 0;

	static int CurrentId();

	static void Sleep(int milliseconds);

	static int GetLogicalCPUCount();

	static volatile int NumThreads;

protected:
#ifdef _WIN32
	static unsigned __stdcall ThreadFunc(void* param);

	uintptr_t thread;
#else
	static void ThreadFunc(void* param);

	enum { ThreadSize = sizeof(size_t) }; // actually: typedef unsigned long int pthread_t
	size_t thread[ThreadSize / sizeof(size_t)];
	bool bStarted;
#endif
};

/*-----------------------------------------------------------------------------
	Atimics
-----------------------------------------------------------------------------*/

#ifdef _WIN32

#undef InterlockedIncrement
#undef InterlockedDecrement
#undef InterlockedAdd

// InterlockedIncrement/Decrement functions returns new value

FORCEINLINE int8 InterlockedIncrement(volatile int8* Value)
{
	return (int8)_InterlockedExchangeAdd8((char*)Value, 1) + 1;
}
FORCEINLINE int16 InterlockedIncrement(volatile int16* Value)
{
	return (int16)_InterlockedIncrement16((short*)Value);
}
FORCEINLINE int32 InterlockedIncrement(volatile int32* Value)
{
	return (int32)_InterlockedIncrement((long*)Value);
}

FORCEINLINE int32 InterlockedIncrement(volatile uint32* Value)
{
	return (int32)_InterlockedIncrement((long*)Value);
}

FORCEINLINE int8 InterlockedDecrement(volatile int8* Value)
{
	return (int8)_InterlockedExchangeAdd8((char*)Value, -1) - 1;
}
FORCEINLINE int16 InterlockedDecrement(volatile int16* Value)
{
	return (int16)_InterlockedDecrement16((short*)Value);
}
FORCEINLINE int32 InterlockedDecrement(volatile int32* Value)
{
	return (int32)_InterlockedDecrement((long*)Value);
}

FORCEINLINE int32 InterlockedDecrement(volatile uint32* Value)
{
	return (int32)_InterlockedDecrement((long*)Value);
}

// InterlockedAdd functions returns value before add (i.e. original one)

FORCEINLINE int8 InterlockedAdd(volatile int8* Value, int8 Amount)
{
	return (int8)_InterlockedExchangeAdd8((char*)Value, (char)Amount);
}

FORCEINLINE int16 InterlockedAdd(volatile int16* Value, int16 Amount)
{
	return (int16)_InterlockedExchangeAdd16((short*)Value, (short)Amount);
}

FORCEINLINE int32 InterlockedAdd(volatile int32* Value, int32 Amount)
{
	return (int32)::_InterlockedExchangeAdd((long*)Value, (long)Amount);
}

FORCEINLINE int32 InterlockedAdd(volatile uint32* Value, int32 Amount)
{
	return (int32)::_InterlockedExchangeAdd((long*)Value, (long)Amount);
}

#ifdef _WIN64

FORCEINLINE int64 InterlockedAdd(volatile int64* Value, int64 Amount)
{
	return (int64)::_InterlockedExchangeAdd64((__int64*)Value, (__int64)Amount);
}

FORCEINLINE uint64 InterlockedAdd(volatile uint64* Value, int64 Amount)
{
	return (uint64)::_InterlockedExchangeAdd64((__int64*)Value, (__int64)Amount);
}

#endif // _WIN64

#else // _WIN32

FORCEINLINE int8 InterlockedIncrement(volatile int8* Value)
{
	return __sync_fetch_and_add(Value, 1) + 1;
}
FORCEINLINE int16 InterlockedIncrement(volatile int16* Value)
{
	return __sync_fetch_and_add(Value, 1) + 1;
}
FORCEINLINE int32 InterlockedIncrement(volatile int32* Value)
{
	return __sync_fetch_and_add(Value, 1) + 1;
}

FORCEINLINE int32 InterlockedIncrement(volatile uint32* Value)
{
	return __sync_fetch_and_add(Value, 1) + 1;
}

FORCEINLINE int8 InterlockedDecrement(volatile int8* Value)
{
	return __sync_fetch_and_sub(Value, 1) - 1;
}
FORCEINLINE int16 InterlockedDecrement(volatile int16* Value)
{
	return __sync_fetch_and_sub(Value, 1) - 1;
}
FORCEINLINE int32 InterlockedDecrement(volatile int32* Value)
{
	return __sync_fetch_and_sub(Value, 1) - 1;
}

FORCEINLINE int32 InterlockedDecrement(volatile uint32* Value)
{
	return __sync_fetch_and_sub(Value, 1) - 1;
}

FORCEINLINE int8 InterlockedAdd(volatile int8* Value, int8 Amount)
{
	return __sync_fetch_and_add(Value, Amount);
}

FORCEINLINE int16 InterlockedAdd(volatile int16* Value, int16 Amount)
{
	return __sync_fetch_and_add(Value, Amount);
}

FORCEINLINE int32 InterlockedAdd(volatile int32* Value, int32 Amount)
{
	return __sync_fetch_and_add(Value, Amount);
}

FORCEINLINE int32 InterlockedAdd(volatile uint32* Value, int32 Amount)
{
	return __sync_fetch_and_add(Value, Amount);
}

FORCEINLINE int64 InterlockedAdd(volatile int64* Value, int64 Amount)
{
	return __sync_fetch_and_add(Value, Amount);
}

FORCEINLINE uint64 InterlockedAdd(volatile uint64* Value, int64 Amount)
{
	return __sync_fetch_and_add(Value, Amount);
}

#endif // _WIN32

/*-----------------------------------------------------------------------------
	Thread pool
-----------------------------------------------------------------------------*/

namespace ThreadPool
{

typedef void (*ThreadTask)(void*);

// Execute ThreadTask in thread. Return false if there's no free threads
bool ExecuteInThread(ThreadTask task, void* taskData, CSemaphore* fence = NULL, bool allowQueue = false);

#define TryExecuteInThread(...) TryExecuteInThreadImpl(__FUNCTION__, __VA_ARGS__)

template<typename F>
FORCEINLINE void TryExecuteInThreadImpl(const char* errorContext, F&& task, CSemaphore* fence = NULL, bool allowQueue = false)
{
	guard(TryExecuteInThread);

	struct Worker
	{
		Worker(F&& inTask, const char* inContext)
		: task(MoveTemp(inTask))
		, context(inContext)
		{}

		F task;
		const char* context;

		static void Proc(void* data)
		{
			Worker* worker = (Worker*)data;

			guard(TryExecuteInThread);
			worker->task();
			unguardf("%s", worker->context);

			delete worker;
		}
	};

	Worker* worker = new Worker(MoveTemp(task), errorContext);
	if (!ExecuteInThread(Worker::Proc, worker, fence, allowQueue))
	{
		// Execute in current thread
		worker->task();
		if (fence) fence->Signal();
		delete worker;
	}

	unguard;
}

template<typename F>
FORCEINLINE void TryExecuteInThreadImpl(F& task, CSemaphore* fence = NULL, bool allowQueue = false)
{
	static_assert(sizeof(task) == 0, "TryExecuteInThread can't accept lvalue");
}

void WaitForCompletion();

void Shutdown();

}

/*-----------------------------------------------------------------------------
	ParallelFor
-----------------------------------------------------------------------------*/

//#define DEBUG_PARALLEL_FOR 1

namespace ParallelForImpl
{

class ParallelForBase
{
public:
	CMutex mutex;
	CSemaphore endSignal;
	int8 numActiveThreads;
	bool bAllSentToThreads;
	int currentIndex;
	int lastIndex;
	int step;

	ParallelForBase(int inCount);
	~ParallelForBase();

	void Start(::ThreadPool::ThreadTask worker);

	bool GrabInterval(int& idx1, int& idx2);
};

template<typename F>
class ParallelForWorker : public ParallelForBase
{
public:
	F Func;

	ParallelForWorker(int InCount, F&& InFunc)
	: ParallelForBase(InCount)
	, Func(InFunc)
	{
		guard(ParallelFor);

		Start(PoolThreadWorker);

		// Add processing for main thread
		int idx1, idx2;
		while (GrabInterval(idx1, idx2))
		{
			ExecuteRange(idx1, idx2);
			//todo: may be periodically check if one of threads were released from another work and can be picked up?
		}
		unguard;
	}

	FORCEINLINE void ExecuteRange(int idx1, int idx2)
	{
		int idx = idx1;
		guard(ParallelForWorker::ExecuteRange);
		while (idx < idx2)
		{
			Func(idx++);
		}
		unguardf("%d [%d,%d]/%d", idx, idx1, idx2, lastIndex);
	}

	static void PoolThreadWorker(void* data)
	{
		guard(PoolThreadWorker);

	#if DEBUG_PARALLEL_FOR
		printf("...... %d: started\n", CThread::CurrentId()&255);
	#endif

		ParallelForWorker& w = *(ParallelForWorker*)data;

		int idx1, idx2;
		while (w.GrabInterval(idx1, idx2))
		{
			w.ExecuteRange(idx1, idx2);
		}

		// End the thread, send signal if this was the last allocated thread
		if (InterlockedDecrement(&w.numActiveThreads) == 0)
			w.endSignal.Signal();

	#if DEBUG_PARALLEL_FOR
		printf("...... %d: finished\n", CThread::CurrentId()&255);
	#endif

		unguard;
	}
};

} // namespace ParallelForImpl

template<typename F>
FORCEINLINE void ParallelFor(int Count, F&& Func)
{
	ParallelForImpl::ParallelForWorker<F> Worker(Count, MoveTemp(Func));
}


#else // THREADING

// Non-threaded versions of functions

template<typename T>
FORCEINLINE T InterlockedIncrement(T* Value)
{
	return ++(*Value);
}

template<typename T>
FORCEINLINE T InterlockedDecrement(T* Value)
{
	return --(*Value);
}

template<typename T, typename T2>
FORCEINLINE T InterlockedAdd(T* Value, T2 Amount)
{
	T Result = *Value;
	*Value = Result + Amount;;
	return Result;
}

template<typename F>
FORCEINLINE void ParallelFor(int Count, F&& Func)
{
	for (int i = 0; i < Count; i++)
		Func(i);
}

#endif // THREADING

#endif // __PARALLEL_H__
