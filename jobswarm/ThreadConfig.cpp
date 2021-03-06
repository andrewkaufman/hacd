#include <cassert>
#include "ThreadConfig.h"
#include "PlatformConfigHACD.h"

/*!
**
** Copyright (c) 20011 by John W. Ratcliff mailto:jratcliffscarab@gmail.com
**
**
** The MIT license:
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is furnished
** to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in all
** copies or substantial portions of the Software.

** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
** WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
** CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#if defined(WIN32)

#define _WIN32_WINNT 0x400
#include <windows.h>

#pragma comment(lib,"winmm.lib")

//	#ifndef _WIN32_WINNT

//	#endif
//	#include <windows.h>
//#include <winbase.h>
#endif

#if defined(_XBOX)
	#include "NxXBOX.h"
#endif

#if defined(__linux__)
	//#include <sys/time.h>
	#include <time.h>
	#include <unistd.h>
	#include <errno.h>
	#define __stdcall
#endif

#if defined(__APPLE__) || defined(__linux__)
	#include <pthread.h>
#endif


#ifdef	NDEBUG
#define VERIFY( x ) (x)
#else
#define VERIFY( x ) assert((x))
#endif

namespace THREAD_CONFIG
{

unsigned int tc_timeGetTime(void)
{
   #if defined(__linux__)
      struct timespec ts;
      clock_gettime(CLOCK_REALTIME, &ts);
      return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
   #else
      return timeGetTime();
   #endif
}

void   tc_sleep(unsigned int ms)
{
   #if defined(__linux__)
      usleep(ms * 1000);
   #else
      Sleep(ms);
   #endif
}

class MyThreadMutex : public ThreadMutex
{
public:
  MyThreadMutex(void)
  {
    #if defined(WIN32) || defined(_XBOX)
  	InitializeCriticalSection(&m_Mutex);
    #elif defined(__APPLE__) || defined(__linux__)
  	pthread_mutexattr_t mutexAttr;  // Mutex Attribute
  	VERIFY( pthread_mutexattr_init(&mutexAttr) == 0 );
  	VERIFY( pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE_NP) == 0 );
  	VERIFY( pthread_mutex_init(&m_Mutex, &mutexAttr) == 0 );
  	VERIFY( pthread_mutexattr_destroy(&mutexAttr) == 0 );
    #endif
  }

  ~MyThreadMutex(void)
  {
    #if defined(WIN32) || defined(_XBOX)
  	DeleteCriticalSection(&m_Mutex);
    #elif defined(__APPLE__) || defined(__linux__)
  	VERIFY( pthread_mutex_destroy(&m_Mutex) == 0 );
    #endif
  }

  void lock(void)
  {
    #if defined(WIN32) || defined(_XBOX)
  	EnterCriticalSection(&m_Mutex);
    #elif defined(__APPLE__) || defined(__linux__)
  	VERIFY( pthread_mutex_lock(&m_Mutex) == 0 );
    #endif
  }

  bool tryLock(void)
  {
    #if defined(WIN32) || defined(_XBOX)
  	bool bRet = false;
  	//assert(("TryEnterCriticalSection seems to not work on XP???", 0));
  	bRet = TryEnterCriticalSection(&m_Mutex) ? true : false;
  	return bRet;
    #elif defined(__APPLE__) || defined(__linux__)
  	int result = pthread_mutex_trylock(&m_Mutex);
  	return (result == 0);
    #endif
  }

  void unlock(void)
  {
    #if defined(WIN32) || defined(_XBOX)
  	LeaveCriticalSection(&m_Mutex);
    #elif defined(__APPLE__) || defined(__linux__)
  	VERIFY( pthread_mutex_unlock(&m_Mutex) == 0 );
    #endif
  }

private:
  #if defined(WIN32) || defined(_XBOX)
	CRITICAL_SECTION m_Mutex;
	#elif defined(__APPLE__) || defined(__linux__)
	pthread_mutex_t  m_Mutex;
	#endif
};

ThreadMutex * tc_createThreadMutex(void)
{
  MyThreadMutex *m = new MyThreadMutex;
  return static_cast< ThreadMutex *>(m);
}

void          tc_releaseThreadMutex(ThreadMutex *tm)
{
  MyThreadMutex *m = static_cast< MyThreadMutex *>(tm);
  delete m;
}

#if defined(WIN32) || defined(_XBOX)
static unsigned long __stdcall _ThreadWorkerFunc(LPVOID arg);
#elif defined(__APPLE__) || defined(__linux__)
static void* _ThreadWorkerFunc(void* arg);
#endif

class MyThread : public Thread
{
public:
  MyThread(ThreadInterface *iface)
  {
    mInterface = iface;
	#if defined(WIN32) || defined(_XBOX)
   	  mThread     = CreateThread(0, 0, _ThreadWorkerFunc, this, CREATE_SUSPENDED, NULL );
    #elif defined(__APPLE__) || defined(__linux__)
	  VERIFY( pthread_create(&mThread, NULL, _ThreadWorkerFunc, this) == 0 );
	#endif
  }

  ~MyThread(void)
  {
	#if defined(WIN32) || defined(_XBOX)
      if ( mThread )
      {
        CloseHandle(mThread);
        mThread = 0;
      }
	#endif
  }

  void onJobExecute(void)
  {
    mInterface->threadMain();
  }

  virtual void Suspend(void)
  {
#ifdef HACD_WINDOWS
	SuspendThread(mThread);
#endif
  }

  virtual void Resume(void) 
  {
#ifdef HACD_WINDOWS
	  ResumeThread(mThread);
#endif
  }

private:
  ThreadInterface *mInterface;
  #if defined(WIN32) || defined(_XBOX)
    HANDLE           mThread;
  #elif defined(__APPLE__) || defined(__linux__)
    pthread_t mThread;
  #endif
};


Thread      * tc_createThread(ThreadInterface *tinterface)
{
  MyThread *m = new MyThread(tinterface);
  return static_cast< Thread *>(m);
}

void          tc_releaseThread(Thread *t)
{
  MyThread *m = static_cast<MyThread *>(t);
  delete m;
}

#if defined(WIN32) || defined(_XBOX)
static unsigned long __stdcall _ThreadWorkerFunc(LPVOID arg)
#elif defined(__APPLE__) || defined(__linux__)
static void* _ThreadWorkerFunc(void* arg)
#endif
{
  MyThread *worker = (MyThread *) arg;
	worker->onJobExecute();
  return 0;
}


class MyThreadEvent : public ThreadEvent
{
public:
  MyThreadEvent(void)
  {
	#if defined(WIN32) || defined(_XBOX)
      mEvent = ::CreateEventA(NULL,TRUE,TRUE,"ThreadEvent");
	#elif defined(__APPLE__) || defined(__linux__)
	  pthread_mutexattr_t mutexAttr;  // Mutex Attribute
	  VERIFY( pthread_mutexattr_init(&mutexAttr) == 0 );
	  VERIFY( pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE_NP) == 0 );
	  VERIFY( pthread_mutex_init(&mEventMutex, &mutexAttr) == 0 );
	  VERIFY( pthread_mutexattr_destroy(&mutexAttr) == 0 );
	  VERIFY( pthread_cond_init(&mEvent, NULL) == 0 );
	#endif
  }

  ~MyThreadEvent(void)
  {
	#if defined(WIN32) || defined(_XBOX)
    if ( mEvent )
    {
      ::CloseHandle(mEvent);
    }
	#elif defined(__APPLE__) || defined(__linux__)
	  VERIFY( pthread_cond_destroy(&mEvent) == 0 );
	  VERIFY( pthread_mutex_destroy(&mEventMutex) == 0 );
	#endif
  }

  virtual void setEvent(void)  // signal the event
  {
	#if defined(WIN32) || defined(_XBOX)
    if ( mEvent )
    {
      ::SetEvent(mEvent);
    }
	#elif defined(__APPLE__) || defined(__linux__)
	  VERIFY( pthread_mutex_lock(&mEventMutex) == 0 );
	  VERIFY( pthread_cond_signal(&mEvent) == 0 );
	  VERIFY( pthread_mutex_unlock(&mEventMutex) == 0 );
	#endif
  }

  void resetEvent(void)
  {
	#if defined(WIN32) || defined(_XBOX)
    if ( mEvent )
    {
      ::ResetEvent(mEvent);
    }
	#endif
  }

	virtual void waitForSingleObject(unsigned int ms)
	{
		#if defined(WIN32) || defined(_XBOX)
		if ( mEvent )
		{
			DWORD result = ::WaitForSingleObject(mEvent,ms);
			HACD_FORCE_PARAMETER_REFERENCE(result);
			HACD_ASSERT( result == WAIT_OBJECT_0 );
		}
		#elif defined(__APPLE__) || defined(__linux__)
		VERIFY( pthread_mutex_lock(&mEventMutex) == 0 );
		if (ms == 0xffffffff)
		{
			VERIFY( pthread_cond_wait(&mEvent, &mEventMutex) == 0 );
		}
		else
		{
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_nsec += ms * 1000000;
			ts.tv_sec += ts.tv_nsec / 1000000000;
			ts.tv_nsec %= 1000000000;
			int result = pthread_cond_timedwait(&mEvent, &mEventMutex, &ts);
			assert(result == 0 || result == ETIMEDOUT);
		}
		VERIFY( pthread_mutex_unlock(&mEventMutex) == 0 );
		#endif
	}

private:
  #if defined(WIN32) || defined(_XBOX)
    HANDLE mEvent;
  #elif defined(__APPLE__) || defined(__linux__)
    pthread_mutex_t mEventMutex;
    pthread_cond_t mEvent;
  #endif
};

int		tc_atomicAdd(int *addend,int amount)
{
#if defined(HACD_WINDOWS) || defined(HACD_MINGW)
	return InterlockedExchangeAdd((long*) addend, long (amount));
#endif

#if defined(HACD_LINUX)
	return __sync_fetch_and_add ((int32_t*)addend, amount );
#endif

#if defined(HACD_APPLE)
	int count = OSAtomicAdd32 (amount, (int32_t*)addend);
	return count - *addend;
#endif

}

ThreadEvent * tc_createThreadEvent(void)
{
  MyThreadEvent *m = new MyThreadEvent;
  return static_cast<ThreadEvent *>(m);
}

void  tc_releaseThreadEvent(ThreadEvent *t)
{
  MyThreadEvent *m = static_cast< MyThreadEvent *>(t);
  delete m;
}

}; // end of namespace
