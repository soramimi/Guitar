#ifndef MISC_H
#define MISC_H

#include <QApplication>
#include <QStringList>
#include <QDateTime>
#include <functional>
#include <vector>

class misc {
public:
	static QStringList splitLines(const QByteArray &text, std::function<QString(char const *begin, size_t len)> tos);
	static QStringList splitLines(QString const &text);
	static void splitLines(const char *begin, const char *end, std::vector<std::string> *out);
	static void splitLines(const std::string &text, std::vector<std::string> *out);
	static QStringList splitWords(const QString &text);
	static QString getFileName(const QString &path);
	static QString makeDateTimeString(const QDateTime &dt);
	static bool starts_with(std::string const &str, std::string const &with);
	static std::string mid(std::string const &str, int start, int length = -1);
	static QString normalizePathSeparator(const QString &str);
	static QString joinWithSlash(const QString &left, const QString &right);
};

class OverrideWaitCursor_ {
public:
	OverrideWaitCursor_()
	{
		qApp->setOverrideCursor(Qt::WaitCursor);
	}
	~OverrideWaitCursor_()
	{
		qApp->restoreOverrideCursor();
	}
};
#define OverrideWaitCursor OverrideWaitCursor_ waitcursor_; (void)waitcursor_;

#endif // MISC_H
