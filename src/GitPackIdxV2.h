#ifndef GITPACKIDXV2_H
#define GITPACKIDXV2_H

#include <QIODevice>
#include <stdint.h>
#include <vector>
#include "GitPack.h"

struct GitPackIdxItem {
	QString id;
	GitPack::Type type = GitPack::Type::UNKNOWN;
	size_t offset = 0;
	size_t packed_size = 0;
	size_t expanded_size = 0;
	uint32_t checksum;
};

class GitPackIdxV2 {
private:
	struct header_t {
		uint8_t magic[8];
		uint32_t fanout[256];
	};

	struct trailer_t {
		uint8_t packfile_checksum[20];
		uint8_t idxfile_checksum[20];
	};

	struct object_id_t {
		uint8_t id[20];
	};

	struct Data {
		header_t header;
		std::vector<object_id_t> objects;
		std::vector<uint32_t> checksums;
		std::vector<uint32_t> offsets;
		trailer_t trailer;
	} d;

	QString toString(const uint8_t *p);

	static inline uint32_t read_uint32_be(void const *p);

	static inline uint32_t get_fanout(header_t const *t, int i);

public:
private:
	std::vector<GitPackIdxItem> item_list;
	std::map<QString, GitPackIdxItem> item_map;
	uint8_t const *object(int i) const;
	const uint32_t offset(int i) const;
	const uint32_t checksum(int i) const;
public:
	uint32_t count() const;
	GitPackIdxItem const *item(size_t i) const;
	GitPackIdxItem const *item_(QString const &id) const;
	GitPackIdxItem const *item_by_offset(size_t offset) const;
	int number(const QString &id) const;
	std::map<QString, GitPackIdxItem> const *map() const;
	bool parse(QIODevice *in);
	bool parse(const QString &idxpath);
	void clear();
};


#endif // GITPACKIDXV2_H
