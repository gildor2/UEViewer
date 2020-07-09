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

protected:

#ifdef _WIN32
	enum { CriticalSectionSize = sizeof(void*)*4 + sizeof(int)*2 }; // sizeof(RTL_CRITICAL_SECTION)
	char data[CriticalSectionSize];
#else
	enum { MutexSize = 40 }; // __SIZEOF_PTHREAD_MUTEX_T
	char data[MutexSize];
#endif
};

class CThread
{
public:
	CThread();
	virtual ~CThread();

	virtual void Run() = 0;

	static void Sleep(int milliseconds);

protected:
	static void ThreadFunc(void* param);

#ifdef _WIN32
	uintptr_t thread;
#else
	enum { ThreadSize = 8 }; // actually: typedef unsigned long int pthread_t
	char thread[ThreadSize];
#endif
};

#endif // __PARALLEL_H__
