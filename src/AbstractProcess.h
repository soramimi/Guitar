#ifndef ABSTRACTPROCESS_H
#define ABSTRACTPROCESS_H

#include <QObject>
#include <QVariant>
#include <functional>

class QString;

class AbstractPtyProcess : public QObject {
	Q_OBJECT
protected:
	QString change_dir_;
	QVariant user_data_;
	std::function<void (bool, const QVariant &)> completed_fn_;
public:
	void setChangeDir(QString const &dir);
	void setCompletedHandler(std::function<void (bool, const QVariant &)> fn, QVariant const &userdata)
	{
		completed_fn_ = fn;
		user_data_ = userdata;
	}
	virtual bool isRunning() const = 0;
	virtual void writeInput(char const *ptr, int len) = 0;
	virtual int readOutput(char *ptr, int len) = 0;
	virtual void start(QString const &cmd, QString const &env) = 0;
	virtual bool wait(unsigned long time = ULONG_MAX) = 0;
	virtual void stop() = 0;
	virtual int getExitCode() const = 0;
	virtual QString getMessage() const = 0;
	virtual void readResult(std::vector<char> *out) = 0;
};

class DryRunPtyProcess : public AbstractPtyProcess {
private:
	QString command_;
public:
	bool isRunning() const
	{
		return false;
	}
	void writeInput(const char *ptr, int len)
	{
	}
	int readOutput(char *ptr, int len)
	{
		return 0;
	}
	void start(const QString &cmd, const QString &env)
	{
		command_ = cmd;
	}
	bool wait(unsigned long time)
	{
		return true;
	}
	void stop()
	{
	}
	int getExitCode() const
	{
		return 0;
	}
	QString getMessage() const
	{
		return QString();
	}
	void readResult(std::vector<char> *out)
	{
		out->clear();
	}
	QString command() const
	{
		return command_;
	}
};

#endif // ABSTRACTPROCESS_H
