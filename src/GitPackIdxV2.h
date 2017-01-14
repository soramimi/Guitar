#ifndef GITPACKIDXV2_H
#define GITPACKIDXV2_H

#include <QIODevice>
#include <stdint.h>
#include <vector>

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

	static inline uint32_t read_uint32_be(void const *p);

	static inline uint32_t get_fanout(header_t const *t, int i);

public:
	uint32_t count() const;
	uint8_t const *object(int i) const;
	bool parse(QIODevice *in);
};


#endif // GITPACKIDXV2_H
