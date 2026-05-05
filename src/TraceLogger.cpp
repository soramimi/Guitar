#include "TraceLogger.h"

#ifdef APP_GUITAR
#include "ApplicationGlobal.h"

void TraceLogger::begin(const QString &name, const QString &cmd)
{
	event_ = {};
	event_.name = name.toStdString();
	event_.phase = TraceEventWriter::PHASE_BEGIN;
	event_.args_comment = cmd.toStdString();
	global->put_trace_event(event_);
}

void TraceLogger::end()
{
	event_.phase = TraceEventWriter::PHASE_END;
	global->put_trace_event(event_);
}
#else
void TraceLogger::begin(const QString &name, const QString &cmd)
{
}

void TraceLogger::end()
{
}
#endif

