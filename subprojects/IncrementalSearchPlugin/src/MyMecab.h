#ifndef MYMECAB_H
#define MYMECAB_H

#include <stdlib.h>
#include <vector>
#include <string>
#include <string_view>

namespace incrementalsearch {

struct Part {
	bool match = false;
	struct Fragment {
		std::string text;
		size_t pos = 0;
		size_t end = 0;
	};
	Fragment source;
	Fragment kana;
};

struct ResultPart : public Part::Fragment {
	bool match = false;
	ResultPart() = default;
	ResultPart(Part const &t)
		: Part::Fragment(t.source)
		, match(t.match)
	{
	}
};

struct Result {
	bool match = false;
	std::vector<ResultPart> parts;
	explicit operator bool () const
	{
		return match;
	}
};

}

class MyMecab {
private:
	struct Private;
	Private *m;
public:
	struct Part {
		size_t offset;
		size_t length;
		std::string text;
	};
public:
	MyMecab();
	~MyMecab();
	bool open(char const *dicpath);
	void close();
	std::vector<Part> parse(std::string_view const &line);
	operator bool () const;
};

#endif // MYMECAB_H
