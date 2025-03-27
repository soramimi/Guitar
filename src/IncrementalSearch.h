#ifndef INCREMENTALSEARCH_H
#define INCREMENTALSEARCH_H

#include <string>
#include <optional>
#include <QString>
#include <memory>

class QRect;
class QPainter;
class QStyleOptionViewItem;

class IncrementalSearch {
private:
	struct M;
	M *m;
public:
	IncrementalSearch();
	~IncrementalSearch();
	void init();
	bool open();
	void close();
	std::optional<std::string> queryMigemo(const char *word);

	static bool migemoEnabled();
	static std::string migemoDictDir();
	static std::string migemoDictPath();
	static bool setupMigemoDict();
	static void deleteMigemoDict();

	static IncrementalSearch *instance();
};

struct MigemoFilter {
	QString text;
	std::shared_ptr<QRegularExpression> re_;
	MigemoFilter() = default;
	MigemoFilter(const QString &text);
	bool isEmpty() const;
	void makeFilter(const QString &filtertext);
	bool match(QString text);

	static QString normalizeText(QString s);

	static int u16ncmp(const ushort *s1, const ushort *s2, int n);

	static void fillFilteredBG(QPainter *painter, QRect const &rect);

	static void drawText(QPainter *painter, QStyleOptionViewItem const &opt, QRect r, QString const &text);
	static void drawText_filted(QPainter *painter, QStyleOptionViewItem const &opt, QRect const &rect, MigemoFilter const &filter);
};

#endif // INCREMENTALSEARCH_H
