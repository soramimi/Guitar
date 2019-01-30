#ifndef GITPACKIDXV2_H
#define GITPACKIDXV2_H

#include <QIODevice>
#include <cstdint>
#include <vector>
#include "GitPack.h"
#include <memory>

struct GitPackIdxItem {
	QString id;
	Git::Object::Type type = Git::Object::Type::UNKNOWN;
	size_t offset = 0;
	size_t packed_size = 0;
	size_t expanded_size = 0;
	uint32_t checksum;
};

class GitPackIdxV2 {
	friend class GitObjectManager;
	friend class MainWindow; // for debug
private:
	QString basename; // e.g. "pack-56430ed038c968ded87eb3756dcde85bfafc10ce"

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
		std::vector<GitPackIdxItem> item_list;
	} d;

private:

	static QString toString(const uint8_t *p);
	static inline uint32_t read_uint32_be(void const *p);
	static inline uint32_t get_fanout(header_t const *t, int i);

	uint8_t const *object(int i) const;
	uint32_t offset(int i) const;
	uint32_t checksum(int i) const;
	void clear();
	bool parse(QIODevice *in);
public:
	bool parse(QString const &idxpath);
	GitPackIdxItem const *item(QString const &id) const;
	GitPackIdxItem const *item(size_t offset) const;
	void each(std::function<bool(GitPackIdxItem const *)> const &fn) const;
};

using GitPackIdxPtr = std::shared_ptr<GitPackIdxV2>;

#endif // GITPACKIDXV2_H
