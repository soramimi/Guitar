#include "GitPack.h"
#include "GitPackIdxV2.h"
#include <QDebug>
#include <QFile>
#include <zlib.h>

bool GitPack::decodeTree(QByteArray *out)
{
	QByteArray result;
	qsizetype pos = 0;
	while (pos < out->size()) {
		auto space = out->indexOf(' ', pos);
		if (space < 0) return false;
		auto mode = out->mid(pos, space - pos);
		char const *type = nullptr;
		if (mode == "40000" || mode == "040000") { mode = "040000"; type = "tree"; }
		else if (mode == "100644" || mode == "100755" || mode == "120000") type = "blob";
		else if (mode == "160000") type = "commit";
		else return false;
		auto nul = out->indexOf('\0', space + 1);
		if (nul <= space + 1 || out->size() - nul - 1 < 20) return false;
		QByteArray name = out->mid(space + 1, nul - space - 1);
		if (name.contains('/')) return false;
		// Also quote edge whitespace, which gitTrimPath would otherwise trim.
		bool needs_quotes = name.front() == ' ' || name.back() == ' ';
		QByteArray quoted("\"");
		for (unsigned char c : name) {
			if (c == '\\' || c == '"') { needs_quotes = true; quoted += '\\'; quoted += char(c); }
			else if (c < 32 || c >= 127) {
				needs_quotes = true;
				quoted += '\\'; quoted += char('0' + (c >> 6));
				quoted += char('0' + ((c >> 3) & 7)); quoted += char('0' + (c & 7));
			} else quoted += char(c);
		}
		quoted += '"';
		if (!needs_quotes) quoted = name;
		result += mode + ' ' + type + ' ' + out->mid(nul + 1, 20).toHex() + '\t' + quoted + '\n';
		pos = nul + 21;
	}
	*out = std::move(result);
	return true;
}

GitObject::Type GitPack::stripHeader(QByteArray *out)
{
	auto nul = out->indexOf('\0');
	if (nul < 0 || nul > 32) return GitObject::Type::UNKNOWN;
	auto space = out->indexOf(' ');
	if (space <= 0 || space >= nul - 1) return GitObject::Type::UNKNOWN;
	auto name = out->left(space);
	GitObject::Type type = GitObject::Type::UNKNOWN;
	if (name == "blob") type = GitObject::Type::BLOB;
	else if (name == "tree") type = GitObject::Type::TREE;
	else if (name == "commit") type = GitObject::Type::COMMIT;
	else if (name == "tag") type = GitObject::Type::TAG;
	if (type == GitObject::Type::UNKNOWN) return type;
	uint64_t size = 0;
	for (auto i = space + 1; i < nul; ++i) {
		unsigned char c = out->at(i);
		if (c < '0' || c > '9' || size > (UINT64_MAX - (c - '0')) / 10) return GitObject::Type::UNKNOWN;
		size = size * 10 + c - '0';
	}
	if (size != uint64_t(out->size() - nul - 1)) return GitObject::Type::UNKNOWN;
	*out = out->mid(nul + 1);
	return type;
}

bool GitPack::decompress(QIODevice *in, size_t expanded_size, QByteArray *out, size_t *consumed, uint32_t *crc, bool exact_size)
{
	if (consumed) *consumed = 0;
	z_stream stream{};
	if (inflateInit(&stream) != Z_OK) return false;
	struct Cleanup { z_stream *stream; ~Cleanup() { inflateEnd(stream); } } cleanup{&stream};
	uint8_t input[65536], output[65536];
	QByteArray result;
	uint32_t checksum = crc ? *crc : 0;
	size_t total = 0;
	while (true) {
		if (stream.avail_in == 0) {
			auto n = in->read(reinterpret_cast<char *>(input), sizeof(input));
			if (n < 0) return false;
			stream.next_in = input;
			stream.avail_in = uInt(n);
		}
		auto before = stream.avail_in;
		auto input_ptr = stream.next_in;
		stream.next_out = output;
		stream.avail_out = sizeof(output);
		int err = inflate(&stream, Z_NO_FLUSH);
		auto used = before - stream.avail_in;
		total += used;
		checksum = crc32(checksum, input_ptr, used);
		size_t n = sizeof(output) - stream.avail_out;
		if (size_t(result.size()) > expanded_size || n > expanded_size - size_t(result.size())) return false;
		result.append(reinterpret_cast<char *>(output), qsizetype(n));
		if (err == Z_STREAM_END) {
			if (exact_size && size_t(result.size()) != expanded_size) return false;
			*out = std::move(result);
			if (consumed) *consumed = total;
			if (crc) *crc = checksum;
			return true;
		}
		if (err != Z_OK || (used == 0 && n == 0)) return false;
	}
}

std::optional<QByteArray> GitPack::applyDelta(QByteArray const &base, QByteArray const &delta)
{
	qsizetype pos = 0;
	auto number = [&]() -> std::optional<uint64_t> {
		uint64_t result = 0;
		for (unsigned shift = 0; shift < 64 && pos < delta.size(); shift += 7) {
			uint8_t c = delta.at(pos++);
			if (uint64_t(c & 127) > (UINT64_MAX >> shift)) return {};
			result |= uint64_t(c & 127) << shift;
			if (!(c & 128)) return result;
		}
		return {};
	};
	auto old_size = number(), new_size = number();
	if (!old_size || !new_size || *old_size != uint64_t(base.size()) || *new_size > 1000000000) return {};
	QByteArray result;
	while (pos < delta.size()) {
		uint8_t op = delta.at(pos++);
		if (!op) return {};
		if (op & 128) {
			uint64_t offset = 0, length = 0;
			for (unsigned i = 0; i < 7; ++i) {
				if (op & (1 << i)) {
					if (pos == delta.size()) return {};
					auto byte = uint64_t(uint8_t(delta.at(pos++)));
					if (i < 4) offset |= byte << (8 * i);
					else length |= byte << (8 * (i - 4));
				}
			}
			if (!length) length = 0x10000;
			if (offset > uint64_t(base.size()) || length > uint64_t(base.size()) - offset ||
				length > *new_size - uint64_t(result.size())) return {};
			result.append(base.constData() + offset, qsizetype(length));
		} else {
			if (op > delta.size() - pos || uint64_t(op) > *new_size - uint64_t(result.size())) return {};
			result.append(delta.constData() + pos, op);
			pos += op;
		}
	}
	if (uint64_t(result.size()) != *new_size) return {};
	return result;
}

bool GitPack::seekPackedObject(QIODevice *file, GitPackIdxItem const *item, GitPackInfo *out)
{
	try {
		GitPackInfo info;

		auto Read = [&](void *ptr, size_t len){
			const auto l = file->read((char *)ptr, len);
			if (l < 0 || ((size_t)(l)) != len) {
				throw QString("failed to read");
			}
			info.checksum = (uint32_t)crc32(info.checksum, (uint8_t const *)ptr, (uint32_t)len);
		};

		if (!file->seek(0)) return false;

		uint32_t header[3];
		Read(header, sizeof(int32_t) * 3);
		if (memcmp(header, "PACK", 4) != 0) throw QString("invalid pack file");
		uint32_t version = read_uint32_be(header + 1);
		if (version != 2 && version != 3) throw QString("invalid pack file version");
		/*int count = */read_uint32_be(header + 2);

		if (item->offset < 12 || item->offset > uint64_t(INT64_MAX) || !file->seek(qint64(item->offset))) return false;

		info.checksum = 0;

		// cf. https://github.com/github/git-msysgit/blob/master/builtin/unpack-objects.c
		{
			size_t size = 0;
			char c;
			Read(&c, 1);
			info.type = (GitObject::Type)((c >> 4) & 7);
			if (info.type == GitObject::Type::UNKNOWN || info.type == GitObject::Type::UNDEFINED) return false;
			size = c & 0x0f;
			int shift = 4;
			while (c & 0x80) {
				Read(&c, 1);
				if (shift >= int(sizeof(size_t) * 8) || size_t(c & 0x7f) > (SIZE_MAX >> shift)) return false;
				size |= size_t(c & 0x7f) << shift;
				shift += 7;
			}
			info.expanded_size = size;
		}
		if (info.type == GitObject::Type::OFS_DELTA) {
			uint64_t offset = 0;
			char c;
			Read(&c, 1);
			offset = c & 0x7f;
			while (c & 0x80) {
				Read(&c, 1);
				if (offset >= (UINT64_MAX >> 7)) return false;
				offset = ((offset + 1) << 7) | (c & 0x7f);
			}
			if (offset == 0 || offset > item->offset - 12) return false;
			info.offset = offset;
		} else if (info.type == GitObject::Type::REF_DELTA) {
			char bin[20];
			Read(bin, 20);
			char tmp[41];
			for (int i = 0; i < 20; i++) {
				sprintf(tmp + i * 2, "%02x", bin[i] & 0xff);
			}
			info.ref_id = std::string(tmp, GIT_ID_LENGTH);
		}

		*out = info;
		return true;
	} catch (QString const &e) {
		qDebug() << e;
	}
	return false;
}

bool GitPack::load(QIODevice *file, GitPackIdxItem const *item, GitPackObject *out)
{
	*out = GitPackObject();
	GitPackObject result;
	if (!seekPackedObject(file, item, &result) || result.expanded_size > 1000000000) return false;
	if (!decompress(file, result.expanded_size, &result.content, &result.packed_size, &result.checksum, true) ||
		result.checksum != item->checksum) return false;
	*out = std::move(result);
	return true;
}

bool GitPack::load(QString const &packfile, GitPackIdxItem const *item, GitPackObject *out)
{
	QFile file(packfile);
	if (file.open(QFile::ReadOnly)) {
		if (load(&file, item, out)) {
			return true;
		}
	}
	return false;
}

