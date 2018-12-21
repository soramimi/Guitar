#ifndef GITPACK_H
#define GITPACK_H

#include <QIODevice>
#include <stdint.h>
#include "Git.h"

struct GitPackIdxItem;

class GitPack {
public:
	struct Info {
		Git::Object::Type type = Git::Object::Type::UNKNOWN;
		size_t expanded_size = 0;
		uint64_t offset = 0;
		QString ref_id;
		uint32_t checksum = 0;
	};
	struct Object : public Info {
		QByteArray content;
		size_t packed_size = 0;
	};
private:
	static uint32_t read_uint32_be(const void *p)
	{
		uint8_t const *q = (uint8_t const *)p;
		return (q[0] << 24) | (q[1] << 16) | (q[2] << 8) | q[3];
	}

public:
	static bool decompress(QIODevice *in, size_t expanded_size, QByteArray *out, size_t *consumed = nullptr, uint32_t *crc = nullptr);
	static bool load(QIODevice *file, GitPackIdxItem const *item, Object *out);
	static bool load(QString const &packfile, const GitPackIdxItem *item, Object *out);
	static bool seekPackedObject(QIODevice *file, const GitPackIdxItem *item, Info *out);
	static void decodeTree(QByteArray *out);
	static Git::Object::Type stripHeader(QByteArray *out);
};

#endif // GITPACK_H
