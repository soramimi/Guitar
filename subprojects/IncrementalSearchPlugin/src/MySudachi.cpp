
// experimental: Sudachi support

#ifdef USE_SUDACHI

#include "MySudachi.h"
#include "cstring"
#include <mecab.h>
#include <string>
#include <vector>
#include "gzip.h"
#include "zs.h"


#include "/home/soramimi/develop/sudachi-example/cpp/sudachi_capi.h"

struct MySudachi::Private {
	SudachiTokenizer *tokenizer = nullptr;
};

MySudachi::MySudachi()
	: m(new Private)
{
	
}

MySudachi::~MySudachi()
{
	close();
	delete m;
}

bool MySudachi::open(const char *dicpath)
{
#ifdef _WIN32
	dicpath = "Z:\\sudachi-dictionary\\system_small.dic";
#else
	dicpath = "/home/soramimi/develop/sudachi-example/sudachi-dictionary-20260428/system_small.dic";
#endif
	m->tokenizer = sudachi_tokenizer_new(dicpath);
	
	return (bool)m->tokenizer;
}

void MySudachi::close()
{
	if (m->tokenizer) {
		sudachi_tokenizer_free(m->tokenizer);
		m->tokenizer = nullptr;
	}
}

MySudachi::operator bool() const
{
	return true;
}

static std::vector<std::string_view> split(std::string_view text, char sep)
{
	std::vector<std::string_view> result;
	size_t pos = 0;
	while (pos < text.size()) {
		size_t end = text.find(sep, pos);
		if (end == std::string_view::npos) {
			end = text.size();
		}
		result.emplace_back(text.substr(pos, end - pos));
		pos = end + 1;
	}
	return result;
}

std::vector<std::string_view> split_lines(std::string_view text)
{
	return split(text, '\n');
}

std::vector<std::string_view> split_words_by_tab(std::string_view text)
{
	return split(text, '\t');
}

std::vector<incrementalsearch::Part> MySudachi::parse(const std::string_view &line) const
{
	std::vector<incrementalsearch::Part> ret;
	
	char *result = sudachi_tokenize_to_string(m->tokenizer, std::string(line).c_str(), SUDACHI_MODE_A);
	if (result) {
		std::vector<std::string_view> lines = split_lines(result);
		size_t offset = 0;
		for (std::string_view line : lines) {
			std::vector<std::string_view> words = split_words_by_tab(line);
			if (words.size() >= 2) {
				std::string_view surface = words[0];
				std::string_view reading = words[1];
				// fprintf(stderr, "surface: %.*s, reading: %.*s\n", (int)surface.size(), surface.data(), (int)reading.size(), reading.data());
				incrementalsearch::Part part;
				part.text = std::string(reading.empty() ? surface : reading);
				part.offset = offset;
				part.length = surface.size();
				if (line.substr(0, surface.size()) == surface) {
					ret.push_back(part);
				} else {
					// fprintf(stderr, "Warning: surface does not match line: %.*s vs %.*s\n", (int)surface.size(), surface.data(), (int)line.size(), line.data());
				}
				offset += surface.size();
			}
		}
		sudachi_string_free(result);
	}
	return ret;	
}

#endif

