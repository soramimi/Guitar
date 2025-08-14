#ifndef TRACELOGGER_H
#define TRACELOGGER_H

#include "TraceEventWriter.h"
#include <QString>

class TraceLogger {
private:
	TraceEventWriter::Event event_;
public:
	void begin(QString const &name, QString const &cmd);
	void end();
};

#endif // TRACELOGGER_H
