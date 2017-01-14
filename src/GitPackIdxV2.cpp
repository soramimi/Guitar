#include "GitPackIdxV2.h"
#include <QCryptographicHash>
#include <QDebug>


uint32_t GitPackIdxV2::read_uint32_be(const void *p)
{
	uint8_t const *q = (uint8_t const *)p;
	return (q[0] << 24) | (q[1] << 16) | (q[2] << 8) | q[3];
}

uint32_t GitPackIdxV2::get_fanout(const GitPackIdxV2::header_t *t, int i)
{
	return read_uint32_be(&t->fanout[i]);
}

uint32_t GitPackIdxV2::count() const
{
	return get_fanout(&d.header, 255);
}

const uint8_t *GitPackIdxV2::object(int i) const
{
	return d.objects[i].id;
}

bool GitPackIdxV2::parse(QIODevice *in)
{
	try {
		auto Read = [&](void *ptr, size_t len){
			return in->read((char *)ptr, len) == len;
		};
		static char const magic[] = "\xff\x74\x4f\x63\x00\x00\x00\x02";
		if (!Read(&d.header, sizeof(d.header)))    throw QString("failed to read the idx header");
		if (memcmp(d.header.magic, magic, 8) != 0) throw QString("invalid idx header");
		uint32_t size = count();
		if (size > 100000) throw QString("number of objects in the idx file is too big");
		size_t size4 = size * sizeof(uint32_t);
		d.objects.resize(size);
		d.checksums.resize(size);
		d.offsets.resize(size);
		if (!Read(&d.objects[0], size * 20))       throw QString("failed to read the objects");
		if (!Read(&d.checksums[0], size4))         throw QString("failed to read the checksums");
		if (!Read(&d.offsets[0], size4))           throw QString("failed to read the offsets");
		if (!Read(&d.trailer, sizeof(d.trailer)))  throw QString("failed to read the trailer");

		QCryptographicHash sha1(QCryptographicHash::Sha1);
		sha1.addData((char const *)&d.header, sizeof(d.header));
		sha1.addData((char const *)&d.objects[0], size * 20);
		sha1.addData((char const *)&d.checksums[0], size4);
		sha1.addData((char const *)&d.offsets[0], size4);
		sha1.addData((char const *)&d.trailer.packfile_checksum, sizeof(d.trailer) - 20);
		QByteArray chksum = sha1.result();
		Q_ASSERT(chksum.size() == 20);
		if (memcmp(chksum.data(), d.trailer.idxfile_checksum, 20) != 0) {
			throw QString("idx checksum is not correct");
		}
		return true;
	} catch (QString const &e) {
		qDebug() << e;
	}
	return false;
}
