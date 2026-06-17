// mecabのソースにlibmecab.cppがあり、LibMecab.cppとすると、
// Windowsでファイル名が競合するため、MyMecab.cppとしています。

#include "MyMecab.h"
#include "cstring"
#include <mecab.h>
#include <string>
#include <vector>
#include "gzip.h"

using namespace incrementalsearch;

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

struct MyMecab::Private {
	MeCab::Tagger *tagger = nullptr;
};

MyMecab::MyMecab()
	: m(new Private)
{
}

MyMecab::~MyMecab()
{
	close();
	delete m;
}

MyMecab::operator bool() const
{
	return (bool)m->tagger;
}

bool MyMecab::open(const char *dicpath)
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

void MyMecab::close()
{
	if (m->tagger) {
		delete m->tagger;
		m->tagger = nullptr;
	}
}

std::vector<MyMecab::Part> MyMecab::parse(const std::string_view &line)
{
	if (!m->tagger) {
		// fprintf(stderr, "Maybe MeCab is not initialized. Please call open() MeCab before calling parse().\n");
		return {};
	}
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
