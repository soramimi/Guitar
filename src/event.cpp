
#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sys/time.h>
#endif

#include "event.h"

#ifdef WIN32

Event::Event()
{
	_handle = CreateEvent(0, FALSE, FALSE, 0);
}

Event::~Event()
{
	CloseHandle(_handle);
}

bool Event::wait(int ms)
{
	if (ms < 0) {
		return WaitForSingleObject(_handle, INFINITE) == WAIT_OBJECT_0;
	} else {
		return WaitForSingleObject(_handle, ms) == WAIT_OBJECT_0;
	}
}

void Event::signal()
{
	SetEvent(_handle);
}

#else

Event::Event()
{
	pthread_cond_init(&_cond, 0); 
}

Event::~Event()
{
	pthread_cond_destroy(&_cond);
}

bool Event::wait(int ms)
{
	if (ms < 0) {
		pthread_cond_wait(&_cond, &_mutex._handle);
		return false;
	} else {
		struct timeval tv;
		struct timespec ts;
		gettimeofday(&tv, 0);
		long long t = (long long)tv.tv_sec * 1000000 + tv.tv_usec;
		t = t * 1000 + ms * 1000000LL;
		ts.tv_sec = t / 1000000000;
		ts.tv_nsec = t % 1000000000;
		return pthread_cond_timedwait(&_cond, &_mutex._handle, &ts) == EINTR;
	}
}

void Event::signal()
{
	pthread_cond_signal(&_cond);
}

#endif
