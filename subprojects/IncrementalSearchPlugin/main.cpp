#include "MeCaSearch.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>

std::string readfile(char const *path)
{
	int fd = open(path, O_RDONLY);
	if (fd != -1) {
		struct stat st;
		std::vector<char> buf;
		if (fstat(fd, &st) == 0 && st.st_size > 0) {
			buf.resize(st.st_size + 1);
			read(fd, buf.data(), st.st_size);
		}
		close(fd);
		return buf.data();
	}
	return {};
}

std::vector<std::string_view> split_lines(char const *text)
{
	std::vector<std::string_view> result;
	char const *p = text;
	while (*p) {
		char const *start = p;
		while (*p && *p != '\n') ++p;
		result.emplace_back(start, p - start);
		if (*p) ++p; // skip '\n'
	}
	return result;
}



int main2(int argc, char **argv)
{
	std::string src = readfile("wagahaiwa_nekodearu.txt");
	if (src.empty()) return 1;
	char const *input = src.c_str();

	MeCaSearch meca;
	if (!meca.open("/home/soramimi/develop/MeCaSearch/mecab/mecab-ipadic")) {
		fprintf(stderr, "MeCaSearch::open() failed\n");
		return 1;
	}

	struct Line {
		std::string text;
		std::string roma;
	};

	std::string query = meca.convert_roman_to_katakana("gakkou");

	if (1) {
		std::vector<std::string_view> lines = split_lines(input);
		for (auto const &line : lines) {
			Line item;
			item.text = line;
			std::vector<MeCaSearch::Part> parts = meca.parse(line);
			for (auto const &part : parts) {
				item.roma.append(part.text);
			}
			if (strstr(item.roma.c_str(), query.c_str())) {
				puts((std::string(item.text)).c_str());
				puts((std::string(item.roma)).c_str());
				putchar('\n');
			}
		}
	}


	return 0;
}
int main3(int argc, char **argv)
{
	MeCaSearch meca;
	if (!meca.open("/dummy")) {
		fprintf(stderr, "MeCaSearch::open() failed\n");
		return 1;
	}

	struct Line {
		std::string text;
		std::string roma;
	};

	std::string query = meca.convert_roman_to_katakana("gakkou");

	if (1) {
		Line item;
		std::string line = "リポジトリ";
		// std::string line = "吾輩は猫である";
		std::vector<MeCaSearch::Part> parts = meca.parse(line);
		for (auto const &part : parts) {
			puts(part.text.c_str());
		}
		putchar('\n');
	}


	return 0;
}

int main1(int argc, char **argv)
{
	return main3(argc, argv);
}

#include <mecab.h>

int main(int argc, char **argv)
{
	return mecab_do(argc, argv, nullptr);
}
