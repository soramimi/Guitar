#ifndef MISC_H
#define MISC_H

#include <QApplication>
#include <QStringList>
#include <QDateTime>
#include <functional>
#include <vector>
#include <cstdint>
#include <QColor>

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#define _SkipEmptyParts QString::SkipEmptyParts
#else
#define _SkipEmptyParts Qt::SkipEmptyParts
#endif

class QContextMenuEvent;

class misc {
public:
	static int stricmp(char const *s1, char const *s2)
	{
#ifdef _WIN32
		return ::stricmp(s1, s2);
#else
		return ::strcasecmp(s1, s2);
#endif
	}

	static int strnicmp(char const *s1, char const *s2, size_t n)
	{
#ifdef _WIN32
		return ::strnicmp(s1, s2, n);
#else
		return ::strncasecmp(s1, s2, n);
#endif
	}

	static QString getApplicationDir();
	static QStringList splitLines(QByteArray const &ba, std::function<QString(char const *ptr, size_t len)> const &tos);
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
	static std::string makeProxyServerURL(std::string text);
	static QString makeProxyServerURL(QString text);
	static QPoint contextMenuPos(QWidget *w, QContextMenuEvent *e);
	static bool isExecutable(QString const &cmd);

	static QString collapseWhitespace(QString const &source);
	static bool isValidMailAddress(const QString &email);
	static bool isValidMailAddress(const std::string &email);

	static std::string_view trimmed(std::string_view const &s);
	static std::string bin_to_hex_string(const void *begin, const void *end);
	static std::vector<uint8_t> hex_string_to_bin(std::string_view const &s, const char *sep = nullptr);

	static int compare(uint8_t const *a, size_t n, uint8_t const *b, size_t m);
	static int compare(std::vector<uint8_t> const &a, std::vector<uint8_t> const &b);
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
