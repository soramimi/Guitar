#ifndef INCREMENTALSEARCH_H
#define INCREMENTALSEARCH_H

#include <string>
#include <optional>
#include <QString>

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

#endif // INCREMENTALSEARCH_H
