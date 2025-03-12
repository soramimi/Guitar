#ifndef INCREMENTALSEARCH_H
#define INCREMENTALSEARCH_H

#include <string>
#include <optional>
#include <QString>
#include <memory>


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
	bool match(const QString &name);

	static QString normalizeText(QString s);


	static int u16ncmp(const ushort *s1, const ushort *s2, int n);
};

#endif // INCREMENTALSEARCH_H
