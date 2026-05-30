// Jagger -- C++ implementation of Pattern-based Japanese Morphological Analyzer
//  $Id: jagger.cc 2154 2026-05-25 05:13:44Z ynaga $
// Copyright (c) 2022 Naoki Yoshinaga <ynaga@iis.u-tokyo.ac.jp>
#include <jagger.h>
#include <cstdio>
#include <string_view>

namespace jagger {
struct tagger {
private:
	// simple_reader reader;
public:
	tagger(char const *in)
	{
	}
	~tagger()
	{
		for (size_t i = 0; i < _mmapped.size(); ++i)
			::munmap(_mmapped[i].first, _mmapped[i].second);
	}
	void read_model(const std::string &m)
	{ // read patterns
		_da.set_array(_read_array(m + ".da"));
		_cp2i = static_cast<uint16_t *>(_read_array(m + ".cp2i"));
		_fs = static_cast<char *>(_read_array(m + ".fs"));
	}
	__attribute__((flatten)) void run(std::string_view in)
	{
		auto Write = [&](char const *s, size_t len) {
			fwrite(s, 1, len, stderr);
		};
		auto write_feature = [&](const pattern_value_t &s) {
			s.concat() ? // as unknown words
				Write(&_fs[s.fs_offset()], s.core_fs_len()),
				Write(UNK_LEMMA "\0", 7)
					   : Write(&_fs[s.fs_offset()], s.fs_len());
		};
		pattern_value_t s, s_prev(0, _cp2i[CP_MAX + 1]); // BOS
		while (!in.empty()) {
			if (in.front() == '\n') { // EOS
				if (s_prev.ti() != _cp2i[CP_MAX + 1])
					write_feature(s_prev);
				// Write("EOS\n", 4);
				// if (TTY) writer.flush(); // line buffering
				s_prev.ti(_cp2i[CP_MAX + 1]);
				s.shift(1);
			} else {
				s.r = _da.longestPatternSearch(in.data(), s_prev.ti(), _cp2i);
				// s.shift(s.shift() ? s.shift() : u8_len(reader.ptr())); // could be faster
				s.shift(s.shift());
				s.concat(0);
				if (s_prev.ti() != _cp2i[CP_MAX + 1]) {
					const bool concat = (s_prev.ctype() & s.ctype()) && // char type mismatch
						(s_prev.ctype() != KANA || s_prev.shift() + s.shift() < 18);
					s.concat(concat);
					if (!concat) // word that may concat with the future context
						write_feature(s_prev);
				}
				const int sh = s.shift();
				s_prev = s;
#define W(n)                                             \
	if (n <= 6 ? __builtin_expect(sh == n, 1) : sh == n) \
		Write(in.data(), n);                   \
	else // faster aligned copy for common short tokens
				W(3)
				W(6)
				W(9) W(12) W(15) Write(in.data(), sh);
#undef W
			}
			in.remove_prefix(s.shift());
			// if (!TTY && !writer.writable((1 << FEAT_BITS) + (1 << PAT_BITS))) writer.flush();
			// if (TTY ? !*reader.ptr() : __builtin_expect(!reader.readable(1 << PAT_BITS), 0)) reader.read();
		}
		if (s_prev.ti() != _cp2i[CP_MAX + 1]) // force EOS for premature input
			write_feature(s_prev);
		// , Write("EOS\n", 4);
	}

private:
	ccedar::da_ _da; // there may be cache friendly alignment
	const uint16_t *_cp2i; // UTF8 char and BOS -> id
	const char *_fs; // feature strings
	std::vector<std::pair<void *, size_t>> _mmapped;
	void *_read_array(const std::string &fn)
	{
		int fd = ::open(fn.c_str(), O_RDONLY);
		ERR_IF(fd == -1, "no such file: %s", fn.c_str());
		const size_t size = ::lseek(fd, 0, SEEK_END); // get size
		void *data = ::mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
		::close(fd);
		_mmapped.push_back(std::make_pair(data, size));
		return data;
	}
};
}

#if 0
int main(int argc, char **argv)
{
	std::string m(JAGGER_DEFAULT_MODEL "/patterns");
	char const *s = "ヤマハは5月28日、ネットワーク機器の値上げを発表した。7月1日以降、ルーターや無線アクセスポイント、ネットワークスイッチなどを3000円～18万円程度値上げする。";
	jagger::tagger jagger(s);
	jagger.read_model(m);
	jagger.run(s);
	return 0;
}
#endif
