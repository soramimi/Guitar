#ifndef MISC_H
#define MISC_H

#include <QApplication>
#include <QStringList>
#include <QDateTime>
#include <functional>
#include <vector>
#include <stdint.h>
#include <QColor>

class QContextMenuEvent;

class misc {
public:
	static QString getApplicationDir();
	static QStringList splitLines(const QByteArray &text, std::function<QString(char const *ptr, size_t len)> tos);
	static QStringList splitLines(QString const &text);
	static void splitLines(const char *begin, const char *end, std::vector<std::string> *out, bool keep_crlf);
	static void splitLines(const std::string &text, std::vector<std::string> *out, bool need_crlf);
	static QStringList splitWords(const QString &text);
	static QString getFileName(const QString &path);
	static QString makeDateTimeString(const QDateTime &dt);
	static bool starts_with(std::string const &str, std::string const &with);
	static std::string mid(std::string const &str, int start, int length = -1);
	static QString normalizePathSeparator(const QString &str);
	static QString joinWithSlash(const QString &left, const QString &right);
	static int runCommand(QString const &cmd, QByteArray *out);
	static int runCommand(const QString &cmd, const QByteArray *in, QByteArray *out);
	static void setFixedSize(QWidget *w);
	static void drawFrame(QPainter *pr, int x, int y, int w, int h, QColor color_topleft, QColor color_bottomright = QColor());
	static void dump(const uint8_t *ptr, size_t len);
	static void dump(QByteArray const *in);
	static bool isText(const QString &mimetype);
	static bool isImage(const QString &mimetype);
	static bool isSVG(const QString &mimetype);
	static bool isPSD(const QString &mimetype);
	static QString abbrevBranchName(const QString &name);
	static QString determinFileType(const QString &filecommand, const QString &path, bool mime, std::function<void (const QString &, QByteArray *)> callback);
	static std::string makeProxyServerURL(std::string text);
	static QString makeProxyServerURL(QString text);
	static QPoint contextMenuPos(QWidget *w, QContextMenuEvent *e);
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
