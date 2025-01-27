#ifndef OVERRIDEWAITCURSOR_H
#define OVERRIDEWAITCURSOR_H

#include "ApplicationGlobal.h"

class OverrideWaitCursor_ {
public:
	OverrideWaitCursor_()
	{
		GlobalSetOverrideWaitCursor();
	}
	~OverrideWaitCursor_()
	{
		GlobalRestoreOverrideCursor();
	}
};
#define OverrideWaitCursor OverrideWaitCursor_ waitcursor_; (void)waitcursor_;

#endif // OVERRIDEWAITCURSOR_H
