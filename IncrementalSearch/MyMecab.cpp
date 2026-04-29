// mecabのソースにlibmecab.cppがあり、LibMecab.cppとすると、
// Windowsでファイル名が競合するため、MyMecab.cppとしています。

#include "MyMecab.h"
#include "cstring"
#include <mecab.h>
#include <string>
#include <vector>
#include "gzip.h"

struct Resource {
	unsigned char *data;
	unsigned int size;
};

extern "C" unsigned char xxd_dicrc_gz[];
extern "C" unsigned int xxd_dicrc_gz_len;
extern "C" unsigned char xxd_unkdic_gz[];
extern "C" unsigned int xxd_unkdic_gz_len;
extern "C" unsigned char xxd_charbin_gz[];
extern "C" unsigned int xxd_charbin_gz_len;
extern "C" unsigned char xxd_sysdic_gz[];
extern "C" unsigned int xxd_sysdic_gz_len;
extern "C" unsigned char xxd_matrixbin_gz[];
extern "C" unsigned int xxd_matrixbin_gz_len;


Resource dicrc_gz = {
	xxd_dicrc_gz,
	xxd_dicrc_gz_len
};

Resource unkdic_gz = {
	xxd_unkdic_gz,
	xxd_unkdic_gz_len
};

Resource charbin_gz = {
	xxd_charbin_gz,
	xxd_charbin_gz_len
};

Resource sysdic_gz = {
	xxd_sysdic_gz,
	xxd_sysdic_gz_len
};

Resource matrixbin_gz = {
	xxd_matrixbin_gz,
	xxd_matrixbin_gz_len
};

struct KanaRoma {
	char const *kana;
	char const *roma;
};

class ResourceReader : public AbstractSimpleReader {
private:
	char const *data = nullptr;
	size_t size = 0;
	size_t offset = 0;
public:
	ResourceReader(char const *data, size_t size)
		: data(data)
		, size(size)
	{
	}
	int read(void *ptr, size_t len)
	{
		if (offset >= size) return 0;
		size_t to_read = std::min(len, size - offset);
		memcpy(ptr, data + offset, to_read);
		offset += to_read;
		return static_cast<int>(to_read);
	}
	int64_t pos() const
	{
		return static_cast<int64_t>(offset);
	}
	void seek(int64_t pos)
	{
		offset = static_cast<size_t>(pos);
	}
};

class ResourceWriter : public AbstractSimpleWriter {
public:
	std::vector<char> *data;
private:
	size_t offset = 0;
public:
	ResourceWriter(std::vector<char> *data)
		: data(data)
	{
		this->data->clear();
		this->data->reserve(65536);
	}
	int write(const void *ptr, size_t len)
	{
		size_t old_size = data->size();
		data->resize(old_size + len);
		memcpy(data->data() + old_size, ptr, len);
		offset += len;
		return static_cast<int>(len);
	}
	int64_t pos() const
	{
		return static_cast<int64_t>(offset);
	}
	void seek(int64_t pos)
	{
		offset = static_cast<size_t>(pos);
	}

	// AbstractSimpleWriter interface
public:
};

static std::string filename(char const *name)
{
	char const *p = name;
	char const *left = name;
	while (*p) {
		if (*p == '/' || *p == '\\') {
			left = p + 1;
		}
		p++;
	}
	return std::string(left);
}

bool x_get_resource_i8(char const *path, std::vector<char> *buf)
{
	Resource *res = nullptr;
	std::string name = filename(path);
	if (name == "dicrc") {
		res = &dicrc_gz;
	} else if (name == "unk.dic") {
		res = &unkdic_gz;
	} else if (name == "char.bin") {
		res = &charbin_gz;
	} else if (name == "sys.dic") {
		res = &sysdic_gz;
	}
	if (res) {
		ResourceReader reader((char const *)res->data, res->size);
		ResourceWriter writer(buf);
		gzip gz;
		if (gz.decompress(&reader, &writer)) {
			return true;
		}
	}
	return false;
}

bool x_get_resource_i16(char const *path, std::vector<short int> *buf)
{
	Resource *res = nullptr;
	std::string name = filename(path);
	if (name == "matrix.bin") {
		res = &matrixbin_gz;
	}
	if (res) {
		std::vector<char> tmp;
		ResourceReader reader((char const *)res->data, res->size);
		ResourceWriter writer(&tmp);
		gzip gz;
		if (gz.decompress(&reader, &writer)) {
			size_t n = tmp.size() / 2;
			if (n > 0) {
				buf->resize(n);
				memcpy(buf->data(), tmp.data(), n * 2);
			}
			return true;
		}
	}
	return false;
}



static std::vector<KanaRoma> kana_roma_table = {
	{"ァ", "xa"},
	{"ア", "a"},
	{"ィ", "xi"},
	{"イ", "i"},
	{"ゥ", "xu"},
	{"ウ", "u"},
	{"ウィ", "wi"},
	{"ウェ", "we"},
	{"ェ", "xe"},
	{"エ", "e"},
	{"ォ", "xo"},
	{"オ", "o"},
	{"カ", "ka"},
	{"ガ", "ga"},
	{"キ", "ki"},
	{"キィ", "kyi"},
	{"キェ", "kye"},
	{"キャ", "kya"},
	{"キュ", "kyu"},
	{"キョ", "kyo"},
	{"ギ", "gi"},
	{"ギィ", "gyi"},
	{"ギェ", "gye"},
	{"ギャ", "gya"},
	{"ギュ", "gyu"},
	{"ギョ", "gyo"},
	{"ク", "ku"},
	{"クァ", "kwa"},
	{"クィ", "kwi"},
	{"クェ", "kwe"},
	{"クォ", "kwo"},
	{"グ", "gu"},
	{"グァ", "gwa"},
	{"グィ", "gwi"},
	{"グェ", "gwe"},
	{"グォ", "gwo"},
	{"ケ", "ke"},
	{"ゲ", "ge"},
	{"コ", "ko"},
	{"ゴ", "go"},
	{"サ", "sa"},
	{"ザ", "za"},
	{"シ", "shi"},
	{"シ", "si"},
	{"シェ", "she"},
	{"シェ", "sye"},
	{"シャ", "sha"},
	{"シャ", "sya"},
	{"シュ", "shu"},
	{"シュ", "syu"},
	{"ショ", "sho"},
	{"ショ", "syo"},
	{"ジ", "ji"},
	{"ジ", "zi"},
	{"ジィ", "jyi"},
	{"ジィ", "zyi"},
	{"ジェ", "je"},
	{"ジェ", "je"},
	{"ジェ", "jye"},
	{"ジェ", "zye"},
	{"ジャ", "ja"},
	{"ジャ", "jya"},
	{"ジャ", "zya"},
	{"ジュ", "ju"},
	{"ジュ", "jyu"},
	{"ジュ", "zyu"},
	{"ジョ", "jo"},
	{"ジョ", "jyo"},
	{"ジョ", "zyo"},
	{"ス", "su"},
	{"ズ", "zu"},
	{"セ", "se"},
	{"ゼ", "ze"},
	{"ソ", "so"},
	{"ゾ", "zo"},
	{"タ", "ta"},
	{"ダ", "da"},
	{"チ", "chi"},
	{"チ", "ti"},
	{"チェ", "che"},
	{"チェ", "che"},
	{"チェ", "tye"},
	{"チャ", "cha"},
	{"チャ", "tya"},
	{"チュ", "chu"},
	{"チュ", "tyu"},
	{"チョ", "cho"},
	{"チョ", "tyo"},
	{"ヂ", "di"},
	{"ヂェ", "dhe"},
	{"ヂェ", "dye"},
	{"ヂャ", "dha"},
	{"ヂャ", "dya"},
	{"ヂュ", "dhu"},
	{"ヂュ", "dyu"},
	{"ヂョ", "dho"},
	{"ヂョ", "dyo"},
	{"ッ", "xtu"},
	{"ツ", "tsu"},
	{"ツ", "tu"},
	{"ツァ", "tsa"},
	{"ツィ", "tsi"},
	{"ツェ", "tse"},
	{"ツォ", "tso"},
	{"ヅ", "du"},
	{"テ", "te"},
	{"ティ", "thi"},
	{"デ", "de"},
	{"ディ", "dhi"},
	{"ディ", "dhi"},
	{"ディ", "dyi"},
	{"ト", "to"},
	{"トゥ", "thu"},
	{"ド", "do"},
	{"ドゥ", "dhu"},
	{"ナ", "na"},
	{"ニ", "ni"},
	{"ニャ", "nya"},
	{"ニュ", "nyu"},
	{"ニョ", "nyo"},
	{"ヌ", "nu"},
	{"ネ", "ne"},
	{"ノ", "no"},
	{"ハ", "ha"},
	{"バ", "ba"},
	{"パ", "pa"},
	{"ヒ", "hi"},
	{"ヒィ", "hyi"},
	{"ヒェ", "hye"},
	{"ヒャ", "hya"},
	{"ヒュ", "hyu"},
	{"ヒョ", "hyo"},
	{"ビ", "bi"},
	{"ビィ", "byi"},
	{"ビェ", "bye"},
	{"ビャ", "bya"},
	{"ビュ", "byu"},
	{"ビョ", "byo"},
	{"ピ", "pi"},
	{"ピィ", "pyi"},
	{"ピェ", "pye"},
	{"ピャ", "pya"},
	{"ピュ", "pyu"},
	{"ピョ", "pyo"},
	{"フ", "fu"},
	{"フ", "hu"},
	{"ファ", "fa"},
	{"フィ", "fi"},
	{"フェ", "fe"},
	{"フォ", "fo"},
	{"フャ", "fya"},
	{"フュ", "fyu"},
	{"フョ", "fyo"},
	{"ブ", "bu"},
	{"プ", "pu"},
	{"ヘ", "he"},
	{"ベ", "be"},
	{"ペ", "pe"},
	{"ホ", "ho"},
	{"ボ", "bo"},
	{"ポ", "po"},
	{"マ", "ma"},
	{"ミ", "mi"},
	{"ミィ", "myi"},
	{"ミェ", "mye"},
	{"ミャ", "mya"},
	{"ミュ", "myu"},
	{"ミョ", "myo"},
	{"ム", "mu"},
	{"メ", "me"},
	{"モ", "mo"},
	{"ャ", "xya"},
	{"ヤ", "ya"},
	{"ュ", "xyu"},
	{"ユ", "yu"},
	{"ョ", "xyo"},
	{"ヨ", "yo"},
	{"ラ", "ra"},
	{"リ", "ri"},
	{"リィ", "ryi"},
	{"リェ", "rye"},
	{"リャ", "rya"},
	{"リュ", "ryu"},
	{"リョ", "ryo"},
	{"ル", "ru"},
	{"レ", "re"},
	{"ロ", "ro"},
	{"ヮ", "xwa"},
	{"ワ", "wa"},
	{"ヰ", "wyi"},
	{"ヱ", "wye"},
	{"ヲ", "wo"},
	{"ヴ", "vu"},
	{"ヴァ", "va"},
	{"ヴィ", "vi"},
	{"ヴェ", "ve"},
	{"ヴォ", "vo"},
	{"ヴャ", "vya"},
	{"ヴュ", "vyu"},
	{"ヴョ", "vyo"},
	{"ヵ", "xka"},
	{"ヶ", "xke"},
	{"ㇻ", "xra"},
	{"ㇼ", "xri"},
	{"ㇽ", "xru"},
	{"ㇾ", "xre"},
	{"ㇿ", "xro"},
};

std::string LibMecab::convert_roman_to_katakana(std::string_view const &romaji)
{
	std::string result;
	auto Write = [&](char const *p){
		// fwrite(p, 1, strlen(p), stdout);
		result.append(p);
	};

	char const *begin = romaji.data();
	char const *end = begin + romaji.size();
	char const *ptr = begin;
	while (ptr < end) {
		if (*ptr == 'n') {
			if (ptr + 1 < end && strchr("aiueoy", ptr[1])) {
				// through
			} else {
				Write("ン");
				ptr++;
				if (ptr < end && *ptr == 'n') {
					ptr++;
				}
				continue;
			}
		}
		bool match = false;
		for (KanaRoma const &item : kana_roma_table) {
			char const *roma = item.roma;
			size_t i = 0;
			while (1) {
				if (roma[i] == 0) {
					ptr += i;
					match = true;
					Write(item.kana);
					break;
				}
				if (ptr[i] == 'l' && roma[i] == 'x') {
					// through
				} else {
					if (roma[i] != ptr[i]) break;
				}
				i++;
			}
		}
		if (!match) {
			if (ptr + 1 < end) {
				int c = (unsigned char)*ptr;
				if ((c == 't' && ptr[1] == 'c') || (isalpha(c) && ptr[1] == c)) {
					Write("ッ");
					ptr++;
					continue;
				}
			}
			ptr++;
		}
	}

	return result;
}

static std::vector<std::string_view> split(const char *feature)
{
	std::vector<std::string_view> result;
	char const *p = feature;
	while (*p) {
		char const *start = p;
		while (*p && *p != ',') ++p;
		result.emplace_back(start, p - start);
		if (*p) ++p; // skip ','
	}
	return result;
}

struct LibMecab::Private {
	MeCab::Tagger *tagger = nullptr;
};

LibMecab::LibMecab()
	: m(new Private)
{
}

LibMecab::~LibMecab()
{
	close();
	delete m;
}


bool LibMecab::open(const char *dicpath)
{
	if (m->tagger) {
		fprintf(stderr, "Already opened\n");
		return false;
	}
	std::string args = std::string("-d ") + dicpath;
	args += " --unk-feature=unknown";
	m->tagger = MeCab::createTagger(args.c_str());
	return m->tagger != nullptr;
}

void LibMecab::close()
{
	if (m->tagger) {
		delete m->tagger;
		m->tagger = nullptr;
	}
}

std::vector<LibMecab::Part> LibMecab::parse(const std::string_view &line)
{
	if (!m->tagger) return {};
	std::vector<Part> parts;
	MeCab::Node const *node = m->tagger->parseToNode(line.data(), line.size());
	while (node) {
		if (node->stat != MECAB_BOS_NODE && node->stat != MECAB_EOS_NODE) {
			std::vector<std::string_view> features = split(node->feature);
			Part part;
			if (features.size() == 1 && features[0] == "unknown") {
				part.text = std::string(line.substr(node->surface - line.data(), node->length));
			} else if (features.size() > 7) {
				part.text = std::string(features[7]);
			}
			if (!part.text.empty()) {
				part.offset = node->surface - line.data();
				part.length = node->length;
				parts.push_back(part);
			}
		}
		node = node->next;
	}
	return parts;
}
