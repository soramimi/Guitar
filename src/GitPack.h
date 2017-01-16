#ifndef GITPACK_H
#define GITPACK_H

#include <QIODevice>
#include <stdint.h>
#include "GitPackIdxV2.h"

class GitPack {
public:
	enum class Type {
		UNKNOWN = 0,
		COMMIT = 1,
		TREE = 2,
		BLOB = 3,
		TAG = 4,
		UNDEFINED = 5,
		OFS_DELTA = 6,
		REF_DELTA = 7,
	};
	struct Object {
		Type type = Type::UNKNOWN;
		QByteArray content;
		uint64_t offset = 0;
		size_t packed_size = 0;
		size_t expanded_size = 0;
	};
private:
	static uint32_t read_uint32_be(const void *p)
	{
		uint8_t const *q = (uint8_t const *)p;
		return (q[0] << 24) | (q[1] << 16) | (q[2] << 8) | q[3];
	}

	static bool load(QIODevice *file, GitPackIdxV2::Item const *item, Object *out);
public:
	static bool decompress(QIODevice *in, bool process_header, Type type, size_t expanded_size, QByteArray *out, size_t *consumed = nullptr);
	static bool load(const QString &packfile, const GitPackIdxV2::Item *item, GitPack::Object *out);
};

#endif // GITPACK_H
