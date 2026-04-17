#ifndef DIR_H
#define DIR_H

#include <string>

class Dir {
private:
	std::string path_;
public:
	Dir() = default;
	Dir(std::string const &path);
	std::string path() const;
	static Dir current();
	static bool setCurrent(std::string const &path);
};

#endif // DIR_H
