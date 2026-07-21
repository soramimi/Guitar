
#ifndef MAIN_H
#define MAIN_H

#include "GitCloneData.h"
#include "GitCommitItem.h"
#include "MainWindowTypes.h"
#include "RepositoryInfo.h"
#include <QElapsedTimer>
#include <QMetaType>
#include <QVariant>

class ProcessStatus;
struct LogData {
	QByteArray data;
	LogData() = default;
	LogData(QByteArray const &ba)
		: data(ba)
	{
	}
	LogData(QString const &s)
		: data(s.toUtf8())
	{
	}
	LogData(char const *p, int n)
		: data(p, n)
	{
	}
	LogData(std::string_view const &s)
		: data(s.data(), s.size())
	{
	}
};
Q_DECLARE_METATYPE(LogData)

struct CloneParams {
	GitCloneData clonedata;
	RepositoryInfo repodata;
};
Q_DECLARE_METATYPE(CloneParams)

class PtyProcessCompleted {
public:
	std::function<void(ProcessStatus *, QVariant const &)> callback;
	std::shared_ptr<ProcessStatus> status;
	QVariant userdata;
	QString process_name;
	QElapsedTimer elapsed;
	PtyProcessCompleted()
		: status(std::make_shared<ProcessStatus>())
	{
	}
};
Q_DECLARE_METATYPE(PtyProcessCompleted)

struct OpenRepositoryOption {
	bool new_session = true;

	bool validate = false;
	bool wait_cursor = true;
	bool keep_selection = false;

	bool clear_log = true;
	bool do_fetch = true;

	bool suppress_uncommit_changes = false;
};

#endif // MAIN_H
