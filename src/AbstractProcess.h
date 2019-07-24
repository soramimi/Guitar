#ifndef ABSTRACTPROCESS_H
#define ABSTRACTPROCESS_H

#include <QObject>
#include <QVariant>
#include <functional>

class QString;

class AbstractPtyProcess : public QObject {
	Q_OBJECT
protected:
	QString change_dir;
	QVariant user_data;
public:
	void setChangeDir(QString const &dir);
	void setVariant(QVariant const &value);
	QVariant const &userVariant() const;
	virtual bool isRunning() const = 0;
	virtual void writeInput(char const *ptr, int len) = 0;
	virtual int readOutput(char *ptr, int len) = 0;
	virtual void start(QString const &cmd, QVariant const &userdata = QVariant()) = 0;
	virtual bool wait(unsigned long time = ULONG_MAX) = 0;
	virtual void stop() = 0;
	virtual int getExitCode() const = 0;
	virtual QString getMessage() const = 0;
	virtual void readResult(std::vector<char> *out) = 0;
signals:
	void completed(bool, QVariant);
};

#endif // ABSTRACTPROCESS_H
