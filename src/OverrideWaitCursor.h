#ifndef OVERRIDEWAITCURSOR_H
#define OVERRIDEWAITCURSOR_H

#include <QApplication>

class OverrideWaitCursor_ {
public:
	OverrideWaitCursor_()
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);
	}
	~OverrideWaitCursor_()
	{
		QApplication::restoreOverrideCursor();
	}
};
#define OverrideWaitCursor OverrideWaitCursor_ waitcursor_; (void)waitcursor_;

#endif // OVERRIDEWAITCURSOR_H
