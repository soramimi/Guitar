#ifndef LOGVIEW_H
#define LOGVIEW_H

#include <QDateTime>
#include <QTableView>

#ifdef _WIN32
typedef uint32_t pid_t;
#endif

struct LogItem {
	QDateTime ts;
	int us = -1;
	QString from;
	pid_t pid = 0;
	QString message;
	QString file;
	int line = 0;
	LogItem() = default;
	LogItem(QString const &message)
		: message(message)
	{
	}
	static LogItem parse(std::string_view const &sv);
};
Q_DECLARE_METATYPE(LogItem)

class LogView : public QTableView {
	Q_OBJECT
private:
	struct Private;
	Private *m;
public:
	explicit LogView(QWidget *parent = nullptr);
	~LogView();
	void add(std::vector<LogItem> &&item);
	void clear();
};

#endif // LOGVIEW_H
