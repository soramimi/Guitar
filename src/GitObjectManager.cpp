#include "GitObjectManager.h"
#include "Git.h"
#include "GitPack.h"

#include <QBuffer>
#include <QDebug>
#include <QDirIterator>
#include <QFile>
#include "joinpath.h"
#include "misc.h"
#include "GitPackIdxV2.h"

GitObjectManager::GitObjectManager(const QString &workingdir)
	: working_dir(workingdir)
{
}




void ApplyDelta(GitPack::Object *obj0, GitPack::Object *obj1, QByteArray *out)
{
	if (obj1->content.size() > 0) {
		uint8_t const *begin = (uint8_t const *)obj1->content.data();
		uint8_t const *end = begin + obj1->content.size();
		uint8_t const *ptr = begin;
		auto ReadNumber = [&](){
			uint64_t n = 0;
			int shift = 0;
			while (ptr < end) {
				uint64_t c = *ptr;
				ptr++;
				n |= (c & 0x7f) << shift;
				shift += 7;
				if (!(c & 0x80)) break;
			}
			return n;
		};
		uint64_t a = ReadNumber(); // older file size
		uint64_t b = ReadNumber(); // newer file size
		qDebug() << a << b;
		while (ptr < end) {
			uint8_t op = *ptr;
			ptr++;
			if (op & 0x80) { // copy operation
				int32_t offset = 0;
				for (int i = 0; i < 4; i++) {
					if ((op >> i) & 1) {
						if (ptr < end) {
							offset |= *ptr << (i * 8);
							ptr++;
						}
					}
				}
				int32_t length = 0;
				for (int i = 0; i < 3; i++) {
					if ((op >> (i + 4)) & 1) {
						if (ptr < end) {
							length |= *ptr << (i * 8);
							ptr++;
						}
					}
				}
				qDebug() << "copy: " << offset << length;
				out->append(obj0->content.data() + offset, length);
			} else { // insert operation
				int length = op & 0x7f;
				qDebug() << "insert: " << length;
				if (ptr + length <= end) {
					out->append((char const *)ptr, length);
					ptr += length;
				}
			}
		}
	}
}

bool LoadPackFile_(GitPackIdxV2 *idx, QIODevice *packfile, GitPackIdxItem const *item, GitPack::Object *out)
{
	GitPack::Info info;
	if (GitPack::query(packfile, item, &info)) {
		if (info.type == GitPack::Type::OFS_DELTA) {
//			if (index > 0) {
				GitPack::Object source;
				GitPackIdxItem const *item2 = idx->item_by_offset(item->offset - info.offset);
				if (item2 && LoadPackFile_(idx, packfile, item2, &source)) {
					if (item->offset == 2900923) {
						qDebug();
					}
					GitPack::Object delta;
					if (GitPack::load(packfile, item, &delta)) {
						QByteArray ba;
						ApplyDelta(&source, &delta, &ba);
						*out = GitPack::Object();
						out->type = source.type;
						out->content = std::move(ba);
//out->content = source.content;
						out->expanded_size = out->content.size();
						return true;
					}
				}
//			}
		} else if (info.type == GitPack::Type::REF_DELTA) {
			qDebug() << "not supported";
			return false; // not supported
		}
	}
	if (GitPack::load(packfile, item, out)) {
		return true;
	}
	return false;
}

bool LoadPackFile(GitPackIdxV2 *idx, QString const &packfilepath, QString const &id, GitPack::Object *out)
{
	*out = GitPack::Object();
	QFile packfile(packfilepath);
	if (packfile.open(QFile::ReadOnly)) {
		GitPackIdxItem const *item = idx->item_(id);
//		int i = idx->number(id);
		if (LoadPackFile_(idx, &packfile, item, out)) {
//			GitPack::stripHeader(&out->content);
			if (out->type == GitPack::Type::TREE) {
				GitPack::decodeTree(&out->content);
			}
			return true;
		}
	}
	return false;
}


bool GitObjectManager::loadObject_(QString const &id, QByteArray *out)
{
	QString path;
	Git::findObjectID(working_dir, id, &path);
	if (!path.isEmpty()) {
		QFile file(path);
		if (file.open(QFile::ReadOnly)) {
			if (GitPack::decompress(&file, GitPack::Type::UNKNOWN, 1000000000, out)) {
				GitPack::Type t = GitPack::stripHeader(out);
				if (t == GitPack::Type::TREE) {
					GitPack::decodeTree(out);
				}
				return true;
			}
		}
	}
	return false;
}

static bool load(QString const &path, GitPackIdxV2 const *idx, size_t i, QByteArray *out)
{
	GitPackIdxItem const *item = idx->item(i);
	if (item) {
		GitPack::Object obj;
		if (GitPack::load(path, item, &obj)) {
			if (obj.type == GitPack::Type::OFS_DELTA) {
				if (i > 0) {
					if (load(path, idx, i - 1, out)) {
						return true;
					}
				}
			}
			*out = std::move(obj.content);
			return true;
		}
	}
	return false;
}

bool GitObjectManager::extractObjectFromPackFile_(QString const &id, QByteArray *out)
{
	struct Entry {
		QString path;
		size_t index;
	};
	GitPackIdxV2 idx;
	std::vector<Entry> entries;
	{
		QString subdir = ".git/objects/pack";
		QString path = working_dir / subdir;
		QDirIterator it(path, { "pack-*.idx" }, QDir::Files | QDir::Readable);
		while (it.hasNext()) {
			it.next();
			if (idx.parse(it.filePath())) {
				size_t n = idx.count();
				for (size_t i = 0; i < n; i++) {
					GitPackIdxItem const *item = idx.item(i);
					if (item && item->id.startsWith(id)) {
						Entry e;
						QString packfilename = it.fileInfo().baseName() + ".pack";
						e.path = working_dir / subdir / packfilename;
						e.index = i;
						entries.push_back(e);
					}
				}
			}
		}
	}
	for (Entry const &e : entries) {
		Entry const &e = entries.front();
		GitPack::Object obj;
		if (LoadPackFile(&idx, e.path, id, &obj)) {
			*out = std::move(obj.content);
			return true;
		}
	}
	return false;
}

bool GitObjectManager::loadObjectFile(const QString &id, QByteArray *out)
{
	if (loadObject_(id, out)) return true;
	if (extractObjectFromPackFile_(id, out)) return true;
	return false;
}


