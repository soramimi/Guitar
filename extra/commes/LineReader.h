#ifndef LINEREADER_H
#define LINEREADER_H

#include <string>
#include <vector>

class LineReader {
private:
	struct File {
		std::string path;
		int fd = -1;
		char buffer[1024];
		size_t offset = 0;
		size_t length = 0;
		char last_char = 0;
		bool eof = false;
		int line = 0;
	};
	std::vector<File> files_;
	File *current();
	void close_all();
	void close_one();
	bool internal_getline(std::string *out);
public:
	LineReader();
	~LineReader();
	std::string const &file() const;
	int line() const;
	bool open(std::string const &path);
	bool getline(std::string *out);
};

#endif // LINEREADER_H
