#ifndef GITPACKIDXV2_H
#define GITPACKIDXV2_H

#include <QIODevice>
#include <cstdint>
#include <vector>
#include "GitPack.h"
#include <memory>

struct GitPackIdxItem {
	uint8_t id[GIT_ID_LENGTH / 2];
	Git::Object::Type type = Git::Object::Type::UNKNOWN;
	size_t offset = 0;
	size_t packed_size = 0;
	size_t expanded_size = 0;
	uint32_t checksum;
	static QString qid(GitPackIdxItem const &item);
};

class GitPackIdxV2 {
	friend class GitObjectManager;
	friend class MainWindow; // for debug
private:
	QString pack_idx_path; // e.g. "/path/to/pack-56430ed038c968ded87eb3756dcde85bfafc10ce.idx"

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
		trailer_t trailer;
		std::vector<object_id_t> objects;
		std::vector<GitPackIdxItem> item_list;
		std::vector<GitPackIdxItem *> item_list_order_by_id;
		std::vector<GitPackIdxItem *> item_list_order_by_offset;
	} d;

private:

	static QString toString(const uint8_t *p);
	static inline uint32_t read_uint32_be(void const *p);
	static inline uint32_t get_fanout(header_t const *t, int i);

	const uint8_t *object(int i) const;
	void clear();
	bool parse(QIODevice *in, int ids_only);
	bool parse(QString const &idxfile, int ids_only);
	void fetch() const;
public:
	GitPackIdxV2() = default;
	~GitPackIdxV2();
	QString pack_file_path() const;
	GitPackIdxItem const *item(QString const &id) const;
	GitPackIdxItem const *item(size_t offset) const;
	void each(std::function<bool(GitPackIdxItem const *)> const &fn) const;
};

using GitPackIdxPtr = std::shared_ptr<GitPackIdxV2>;

#endif // GITPACKIDXV2_H
