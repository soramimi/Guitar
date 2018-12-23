#ifndef MISC_H
#define MISC_H

#include <QApplication>
#include <QStringList>
#include <QDateTime>
#include <functional>
#include <vector>
#include <cstdint>
#include <QColor>

class QContextMenuEvent;

class misc {
public:
	static QString getApplicationDir();
	static QStringList splitLines(QByteArray const &text, std::function<QString(char const *ptr, size_t len)> const &tos);
	static QStringList splitLines(QString const &text);
	static void splitLines(char const *begin, char const *end, std::vector<std::string> *out, bool keep_newline);
	static void splitLines(std::string const &text, std::vector<std::string> *out, bool need_crlf);
	static QStringList splitWords(QString const &text);
	static QString getFileName(QString const &path);
	static QString makeDateTimeString(const QDateTime &dt);
	static bool starts_with(std::string const &str, std::string const &with);
	static std::string mid(std::string const &str, int start, int length = -1);
	static QString normalizePathSeparator(QString const &str);
	static QString joinWithSlash(QString const &left, QString const &right);
	static void setFixedSize(QWidget *w);
	static void drawFrame(QPainter *pr, int x, int y, int w, int h, QColor color_topleft, QColor color_bottomright = QColor());
	static void dump(const uint8_t *ptr, size_t len);
	static void dump(QByteArray const *in);
	static bool isText(QString const &mimetype);
	static bool isImage(QString const &mimetype);
	static bool isSVG(QString const &mimetype);
	static bool isPSD(QString const &mimetype);
	static QString abbrevBranchName(QString const &name);
	static QString determinFileType(QString const &filecommand, QString const &path, bool mime, std::function<void(QString const &, QByteArray *)> const &callback);
	static std::string makeProxyServerURL(std::string text);
	static QString makeProxyServerURL(QString text);
	static QPoint contextMenuPos(QWidget *w, QContextMenuEvent *e);
	static bool isExecutable(QString const &cmd);
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
