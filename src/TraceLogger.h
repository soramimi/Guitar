
#ifndef TRACELOGGER_H
#define TRACELOGGER_H

#include "TraceEventItem.h"
#include "TraceEventWriter.h"
#include <QString>

class TraceLogger {
private:
	TraceEventItem event_;
public:
	TraceLogger() = default;
	TraceLogger(QString const &name, QString const &cmd)
	{
		begin(name, cmd);
	}
	~TraceLogger()
	{
		if (event_.phase == TraceEventWriter::PHASE_BEGIN) {
			end();
		}
	}
	void begin(QString const &name, QString const &cmd);
	void end();
};

#endif // TRACELOGGER_H
