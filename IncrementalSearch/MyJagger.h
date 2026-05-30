#ifndef MYJAGGER_H
#define MYJAGGER_H

#include <stdlib.h>
#include <string>
#include <vector>

#include "MyMecab.h"

// experimental: Jaggerを用いた形態素解析の実験コード

class LibJagger {
private:
	struct Private;
	Private *m;
public:
	using Part = LibMecab::Part;
public:
	LibJagger();
	~LibJagger();
	bool open(char const *dicpath);
	void close();
	std::vector<Part> parse(std::string_view const &line);
};

#endif // MYJAGGER_H
