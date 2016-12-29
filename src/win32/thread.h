
#ifndef __THREAD_H
#define __THREAD_H

#include <stdarg.h>

#ifdef WIN32
	#include <windows.h>
	#include <process.h>
#else
	#include <pthread.h>
#endif



class Thread {
protected:
public:
	volatile bool _interrupted;
	bool _running;
#ifdef WIN32
	HANDLE _thread_handle;
	static unsigned int __stdcall run_(void *arg);
#else
	pthread_t _thread_handle;
	static void *run_(void *arg);
#endif

public:
	Thread()
	{
		_interrupted = false;
		_running = false;
		_thread_handle = 0;
	}
	virtual ~Thread()
	{
		detach();
	}
protected:
	virtual void run() = 0;
	virtual bool interrupted() const
	{
		return _interrupted;
	}
public:
	virtual void start();
	virtual void stop();
	virtual void join();
	virtual void terminate();
	virtual void detach();
	bool running() const
	{
		return _running;
	}
};



#endif
