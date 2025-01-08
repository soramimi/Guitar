#ifndef FILETYPE_H
#define FILETYPE_H

#include <string>
#include <vector>

class FileType {
public:
	struct Result {
		std::string mimetype;
		std::string charset;
	};
private:
	void *magic_set = nullptr;
	std::vector<char> mgcdata;
public:
	bool open(const char *mgcptr, size_t mgclen);
	bool open(char const *mgcfile);
	void close();
	static size_t slop_size();
	Result file(int fd) const;
	Result file(char const *filepath) const;
	Result file(const char *data, size_t size, int filemode = 0644, bool pad_slop = true) const;
};

#endif // FILETYPE_H
