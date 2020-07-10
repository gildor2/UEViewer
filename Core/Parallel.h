#ifndef __PARALLEL_H__
#define __PARALLEL_H__

/*-----------------------------------------------------------------------------
	Generic classes
-----------------------------------------------------------------------------*/

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

#ifdef _WIN32
	enum { CriticalSectionSize = sizeof(void*)*4 + sizeof(int)*2 }; // sizeof(RTL_CRITICAL_SECTION)
	char data[CriticalSectionSize];
#else
	enum { MutexSize = 40 }; // __SIZEOF_PTHREAD_MUTEX_T
	size_t data[MutexSize / sizeof(size_t)];
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
	enum { SemSize = 16 };
	size_t data[SemSize / sizeof(size_t)];
#endif
};

class CThread
{
public:
	CThread();
	virtual ~CThread();

	virtual void Run() = 0;

	static int CurrentId();

	static void Sleep(int milliseconds);

	static int GetLogicalCPUCount();

protected:
	static void ThreadFunc(void* param);

#ifdef _WIN32
	uintptr_t thread;
#else
	enum { ThreadSize = 8 }; // actually: typedef unsigned long int pthread_t
	size_t thread[ThreadSize / sizeof(size_t)];
#endif
};

/*-----------------------------------------------------------------------------
	Atimics
-----------------------------------------------------------------------------*/

#ifdef _WIN32

#undef InterlockedIncrement
#undef InterlockedDecrement

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

#else

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

#endif

/*-----------------------------------------------------------------------------
	Thread pool
-----------------------------------------------------------------------------*/

namespace ThreadPool
{

typedef void (*ThreadTask)(void*);

// Execute ThreadTask in thread. Return false if there's no free threads
bool ExecuteInThread(ThreadTask task, void* taskData, CSemaphore* fence = NULL);

template<typename F>
FORCEINLINE void TryExecuteInThread(F&& task, CSemaphore* fence = NULL)
{
	guard(TryExecuteInThread);

	struct Worker
	{
		Worker(F&& inTask)
		: task(MoveTemp(inTask))
		{}

		F task;

		static void Proc(void* data)
		{
			Worker* worker = (Worker*)data;
			worker->task();
			delete worker;
		}
	};

	Worker* worker = new Worker(MoveTemp(task));
	if (!ExecuteInThread(Worker::Proc, worker, fence))
	{
		// Execute in current thread
		worker->task();
		if (fence) fence->Signal();
		delete worker;
	}

	unguard;
}

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
	F& Func;

	ParallelForWorker(int InCount, F& InFunc)
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
		guard(ExecuteRange);
		while (idx1 < idx2)
		{
			Func(idx1++);
		}
		unguard;
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
FORCEINLINE void ParallelFor(int Count, F& Func)
{
	ParallelForImpl::ParallelForWorker<F> Worker(Count, MoveTemp(Func));
}


#endif // __PARALLEL_H__
