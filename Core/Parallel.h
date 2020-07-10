#ifndef __PARALLEL_H__
#define __PARALLEL_H__

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


#ifdef _WIN32

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

#endif // __PARALLEL_H__
