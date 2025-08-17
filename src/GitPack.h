#ifndef GITPACK_H
#define GITPACK_H

#include "GitTypes.h"
#include <QIODevice>
#include <cstdint>

struct GitPackIdxItem;

struct GitPackInfo {
	GitObject::Type type = GitObject::Type::UNKNOWN;
	size_t expanded_size = 0;
	uint64_t offset = 0;
	QString ref_id;
	uint32_t checksum = 0;
};

struct GitPackObject : public GitPackInfo {
	QByteArray content;
	size_t packed_size = 0;
};

class GitPack {
private:
	static uint32_t read_uint32_be(const void *p)
	{
		auto const *q = (uint8_t const *)p;
		return (q[0] << 24) | (q[1] << 16) | (q[2] << 8) | q[3];
	}

public:
	static bool decompress(QIODevice *in, size_t expanded_size, QByteArray *out, size_t *consumed = nullptr, uint32_t *crc = nullptr);
	static bool load(QIODevice *file, GitPackIdxItem const *item, GitPackObject *out);
	static bool load(QString const &packfile, GitPackIdxItem const *item, GitPackObject *out);
	static bool seekPackedObject(QIODevice *file, GitPackIdxItem const *item, GitPackInfo *out);
	static void decodeTree(QByteArray *out);
	static GitObject::Type stripHeader(QByteArray *out);
};

#endif // GITPACK_H
