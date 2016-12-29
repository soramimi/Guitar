
#ifndef __EVENT_H
#define __EVENT_H

#include "mutex.h"

#ifdef WIN32

class Event {
private:
	HANDLE _handle;
public:
	Event();
	~Event();
	bool wait(int ms = -1);
	void signal();
};

#else

class Event {
private:
	Mutex _mutex;
	pthread_cond_t _cond;
public:
	Event();
	~Event();
	bool wait(int ms = -1);
	void signal();
};

#endif

#endif


