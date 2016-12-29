
#ifndef __MUTEX_H
#define __MUTEX_H


#ifdef WIN32

#include <windows.h>
class Mutex {
public:
	HANDLE _handle;
	Mutex()
	{
		_handle = CreateMutex(0, FALSE, 0);
	}
	~Mutex()
	{
		CloseHandle(_handle);
	}
	void lock()
	{
		WaitForSingleObject(_handle, INFINITE);
	}
	void unlock()
	{
		ReleaseMutex(_handle);
	}
};

#else

#include <pthread.h>
class Mutex {
public:
	pthread_mutex_t _handle;
	Mutex()
	{
		pthread_mutex_init(&_handle, NULL);
	}
	~Mutex()
	{
		pthread_mutex_destroy(&_handle);
	}
	void lock()
	{
		pthread_mutex_lock(&_handle);
	}
	void unlock()
	{
		pthread_mutex_unlock(&_handle);
	}
};

#endif


class AutoLock {
	Mutex *_mutex;
public:
	AutoLock(Mutex *mutex)
	{
		_mutex = mutex;
		_mutex->lock();
	}
	~AutoLock()
	{
		_mutex->unlock();
	}
};



#endif

