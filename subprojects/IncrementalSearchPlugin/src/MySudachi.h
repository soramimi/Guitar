#ifndef MYSUDACHI_H
#define MYSUDACHI_H

// experimental: Sudachi support

#ifdef USE_SUDACHI

#include <stdlib.h>
#include <vector>
#include <string>
#include <string_view>
#include "IncrementalSearch.h"

class MySudachi : public incrementalsearch::Engine {
private:
	struct Private;
	Private *m;
public:
	MySudachi();
	~MySudachi() override;
	bool open(char const *dicpath) override;
	void close() override;
	std::vector<incrementalsearch::Part> parse(std::string_view const &line) const override;
	operator bool () const override;
};

#endif

#endif // MYSUDACHI_H
