#ifndef MYMECAB_H
#define MYMECAB_H

#include <stdlib.h>
#include <vector>
#include <string>
#include <string_view>

class LibMecab {
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
	LibMecab();
	~LibMecab();
	bool open(char const *dicpath);
	void close();
	std::vector<Part> parse(std::string_view const &line);
	std::string convert_roman_to_katakana(std::string_view const &romaji);
};

#endif // MYMECAB_H
