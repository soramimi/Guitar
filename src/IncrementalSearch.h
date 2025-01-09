#ifndef INCREMENTALSEARCH_H
#define INCREMENTALSEARCH_H

#include <string>
#include <optional>

class IncrementalSearch {
private:
	struct M;
	M *m;
public:
	IncrementalSearch();
	~IncrementalSearch();
	bool open();
	std::string query(const char *word);
};

#endif // INCREMENTALSEARCH_H
