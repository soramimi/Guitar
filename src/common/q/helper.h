#ifndef QHELPER_H
#define QHELPER_H

#include <QString>
#include <string_view>
#include <vector>


class QS : public QString {
public:
	explicit QS(const QChar *unicode, int size = -1) : QString(unicode, size) {}
	QS() = default;
	QS(QString &&s) : QString(s) {}
	QS(QString const &s) : QString(s) {}
	QS(QChar c) : QString(c) {}
	QS(char const *p, int n = -1) : QString(QLatin1String(p, n)) {}
	QS(std::string_view const &s) : QS(s.data(), s.size()) {}
	QS(std::string const &s) : QString(QString::fromStdString(s)) {}
	QS(std::vector<char> const &s) : QS(s.data(), s.size()) {}
	operator std::string () const { return toStdString(); }
	std::string ss() const { return toStdString(); }
	QS &operator += (std::string const &s) { append(fromStdString(s)); return *this; }
};

class QBA : public QByteArray {
public:
	QBA() = default;
	QBA(QByteArray const &ba) : QByteArray(ba) {}
	QBA(char const *p, int n = -1) : QByteArray(p, n) {}
	QBA(int n, char c) : QByteArray(n, c) {}
	QBA(std::string const &s) : QBA(s.data(), s.size()) {}
	QBA(std::string_view const &s) : QBA(s.data(), s.size()) {}
	QBA(std::vector<char> const &s) : QBA(s.data(), s.size()) {}
	std::vector<char> vc() const { return std::vector<char>(begin(), end()); }
	std::string_view sv() const { return std::string_view(data(), size()); }
	operator std::string_view () const { return sv(); }
};

class QSL : public QStringList {
public:
	QSL() = default;
	QSL(QStringList const &sl) : QStringList(sl) {}
	QSL(std::vector<std::string> const &v)
	{
		for (std::string const &s : v) {
			push_back(QString::fromStdString(s));
		}
	}
	std::vector<std::string> vs() const
	{
		std::vector<std::string> v;
		for (const QString &s : *this) {
			v.push_back(s.toStdString());
		}
		return v;
	}
	operator std::vector<std::string> () const
	{
		return vs();
	}
};

#endif // QHELPER_H
