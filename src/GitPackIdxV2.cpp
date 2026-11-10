#include "GitHash.h"
#include "GitPackIdxV2.h"
#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <limits>

std::string GitPackIdxItem::string(const GitPackIdxItem &item)
{
	char tmp[GIT_ID_LENGTH + 1];
	for (int i = 0; i < GIT_ID_LENGTH / 2; i++) {
		sprintf(tmp + i * 2, "%02x", item.id[i]);
	}
	return std::string(tmp, GIT_ID_LENGTH);
}

GitPackIdxV2::~GitPackIdxV2()
{
	clear();
}

QString GitPackIdxV2::pack_file_path() const
{
	QString path = pack_idx_path;
	if (path.endsWith(".idx")) {
		path = path.mid(0, path.size() - 3) + "pack";
		return path;
	}
	return {};
}

void GitPackIdxV2::clear()
{
	d = Data();
}

QString GitPackIdxV2::toString(uint8_t const *p)
{
	char tmp[41];
	for (int i = 0; i < GIT_ID_LENGTH / 2; i++) {
		sprintf(tmp + i * 2, "%02x", p[i]);
	}
	return QString::fromLatin1(tmp, GIT_ID_LENGTH);
}

uint32_t GitPackIdxV2::read_uint32_be(const void *p)
{
	auto const *q = (uint8_t const *)p;
	return (q[0] << 24) | (q[1] << 16) | (q[2] << 8) | q[3];
}

uint32_t GitPackIdxV2::get_fanout(const GitPackIdxV2::header_t *t, int i)
{
	return read_uint32_be(&t->fanout[i]);
}

const uint8_t *GitPackIdxV2::object(int i) const
{
	return d.objects[i].id;
}

bool GitPackIdxV2::parse(QIODevice *in, int ids_only)
{
	clear();
	try {
		auto Read = [&](void *ptr, size_t len){
			const auto l = in->read((char *)ptr, len);
			return l >= 0 && static_cast<size_t>(l) == len;
		};
		static char const magic[] = "\xff\x74\x4f\x63\x00\x00\x00\x02";
		if (!Read(&d.header, sizeof(d.header)))    throw QString("failed to read the idx header");
		if (memcmp(d.header.magic, magic, 8) != 0) throw QString("invalid idx header");
		for (int i = 1; i < 256; ++i) {
			if (get_fanout(&d.header, i) < get_fanout(&d.header, i - 1)) throw QString("invalid fanout");
		}
		uint32_t size = get_fanout(&d.header, 255);
		if (size > 2000000) throw QString("number of objects in the idx file is too big");
		size_t size4 = size * sizeof(uint32_t);

		d.objects.resize(size);
		if (!Read(d.objects.data(), size * 20))       throw QString("failed to read the objects");

		for (size_t i = 1; i < size; ++i) {
			if (memcmp(d.objects[i - 1].id, d.objects[i].id, 20) >= 0) throw QString("invalid object order");
		}
		if (ids_only) return true;

		std::vector<uint32_t> checksums(size);
		std::vector<uint32_t> offsets(size);

		QCryptographicHash sha1(QCryptographicHash::Sha1);

#if (QT_VERSION < QT_VERSION_CHECK(6, 4, 0))
		auto sha1AddData = [&](void const *p, size_t n){
			sha1.addData((char const *)p, n);
		};
#else
		auto sha1AddData = [&](void const *p, size_t n){
			sha1.addData(QByteArrayView((char const *)p, n));
		};
#endif

		sha1AddData((char const *)&d.header, sizeof(d.header));
		sha1AddData((char const *)d.objects.data(), int(size * 20));

		if (!Read(checksums.data(), size4))         throw QString("failed to read the checksums");
		sha1AddData((char const *)checksums.data(), size4);
		if (!Read(offsets.data(), size4))           throw QString("failed to read the offset_values");
		sha1AddData((char const *)offsets.data(), size4);

		size_t large_count = 0;
		for (auto const &value : offsets) if (read_uint32_be(&value) & 0x80000000u) ++large_count;
		std::vector<uint64_t> large_offsets(large_count);
		for (auto &offset : large_offsets) {
			uint8_t bytes[8];
			if (!Read(bytes, sizeof(bytes))) throw QString("failed to read large offsets");
			sha1AddData(bytes, sizeof(bytes));
			offset = (uint64_t(read_uint32_be(bytes)) << 32) | read_uint32_be(bytes + 4);
		}
		if (!Read(&d.trailer, sizeof(d.trailer)))  throw QString("failed to read the trailer");
		sha1AddData((char const *)&d.trailer.packfile_checksum, sizeof(d.trailer) - 20);

		QByteArray chksum = sha1.result();
		Q_ASSERT(chksum.size() == 20);
		if (memcmp(chksum.data(), d.trailer.idxfile_checksum, 20) != 0) {
			throw QString("idx checksum is not correct");
		}

		d.item_list.resize(size);
		d.item_list_order_by_id.resize(size);
		d.item_list_order_by_offset.resize(size);

		auto offset = [&](size_t i) {
			uint64_t value = read_uint32_be(&offsets[i]);
			if (value & 0x80000000u) {
				auto index = value & 0x7fffffffu;
				if (index >= large_offsets.size()) throw QString("invalid large offset index");
				value = large_offsets[index];
			}
			if (value < 12 || value > std::numeric_limits<size_t>::max()) throw QString("invalid object offset");
			return size_t(value);
		};

		auto checksum = [&](size_t i) {
			return read_uint32_be(&checksums[i]);
		};

		for (size_t i = 0; i < size; i++) {
			GitPackIdxItem *item = &d.item_list[i];
			item->offset = offset(i);
			item->checksum = checksum(i);
			memcpy(item->id, d.objects[i].id, GIT_ID_LENGTH / 2);
		}

		for (size_t i = 0; i < size; i++) {
			d.item_list_order_by_id[i] = &d.item_list[i];
			d.item_list_order_by_offset[i] = &d.item_list[i];
		}

		std::sort(d.item_list_order_by_id.begin(), d.item_list_order_by_id.end(), [](GitPackIdxItem *l, GitPackIdxItem *r){
			return memcmp(l->id, r->id, GIT_ID_LENGTH / 2) < 0;
		});

		std::sort(d.item_list_order_by_offset.begin(), d.item_list_order_by_offset.end(), [](GitPackIdxItem *l, GitPackIdxItem *r){
			return l->offset < r->offset;
		});

		for (size_t i = 1; i < size; ++i) {
			auto previous = d.item_list_order_by_offset[i - 1];
			auto next = d.item_list_order_by_offset[i];
			if (next->offset <= previous->offset) throw QString("duplicate object offset");
			previous->packed_size = next->offset - previous->offset;
		}
		return true;
	} catch (QString const &e) {
		qDebug() << e;
	}
	clear();
	return false;
}

bool GitPackIdxV2::parse(QString const &idxfile, int ids_only)
{
	if (!ids_only) {
		clear();
	}
	QFile file(idxfile);
	if (file.open(QFile::ReadOnly)) {
		if (parse(&file, ids_only)) {
			return true;
		}
	}
	return false;
}

void GitPackIdxV2::fetch() const
{
	if (d.objects.size() != d.item_list.size()) {
		GitPackIdxV2 *self = const_cast<GitPackIdxV2 *>(this);
		self->parse(pack_idx_path, false);
	}
}

GitPackIdxItem const *GitPackIdxV2::item(const GitHash &id) const
{
	fetch();
#if 0
	for (const auto *p : d.item_list) {
		if (p->qid() == id.toQString()) {
			return p;
		}
	}
#else
	QString id1 = QString::fromStdString(id.toString());
	if (id1.size() == GIT_ID_LENGTH) {
		uint8_t id2[GIT_ID_LENGTH / 2];
		char tmp[3];
		for (int i = 0; i < GIT_ID_LENGTH / 2; i++) {
			tmp[0] = id1[i * 2 + 0].toLatin1();
			tmp[1] = id1[i * 2 + 1].toLatin1();
			tmp[2] = 0;
			id2[i] = strtol(tmp, nullptr, 16);
		}

		auto Compare = [](uint8_t const *l, uint8_t const *r){
			return memcmp(l, r, GIT_ID_LENGTH / 2);
		};

		size_t lo = 0;
		size_t hi = d.item_list_order_by_id.size();
		while (lo < hi) {
			size_t m = (lo + hi) / 2;
			auto const *p = d.item_list_order_by_id.at(m);
			int c = Compare(p->id, id2);
			if (c == 0) return p;
			if (c < 0) {
				lo = m + 1;
			} else {
				hi = m;
			}
		}
	}
#endif
	return nullptr;
}

GitPackIdxItem const *GitPackIdxV2::item(size_t offset) const
{
	fetch();
#if 0
	for (const auto *item : d.item_list) {
		if (item->offset == offset) return item;
	}
#else
	auto Compare = [](GitPackIdxItem const *l, size_t r){
		if (l->offset < r) return -1;
		if (l->offset > r) return 1;
		return 0;
	};

	size_t lo = 0;
	size_t hi = d.item_list_order_by_offset.size();
	while (lo < hi) {
		size_t m = (lo + hi) / 2;
		auto const *p = d.item_list_order_by_offset.at(m);
		int c = Compare(p, offset);
		if (c == 0) return p;
		if (c < 0) {
			lo = m + 1;
		} else {
			hi = m;
		}
	}
#endif
	return nullptr;
}

void GitPackIdxV2::each(std::function<bool(GitPackIdxItem const *)> const &fn) const
{
	fetch();
	for (GitPackIdxItem const &p : d.item_list) {
		if (!fn(&p)) break;
	}
}

