#include "GitObjectManager.h"
#include "Git.h"
#include "joinpath.h"
#include "misc.h"
#include <QBuffer>
#include <QDebug>
#include <QDirIterator>
#include <QFile>

GitObjectManager::GitObjectManager()
{
	subdir_git_objects = ".git/objects";
	subdir_git_objects_pack = subdir_git_objects / "pack";
}

void GitObjectManager::setup(GitPtr g)
{
	this->g = g;
	clearIndexes();
}

QString GitObjectManager::workingDir()
{
	return g->workingRepositoryDir();
}

void GitObjectManager::loadIndexes()
{
	QMutexLocker lock(&mutex);
	if (git_idx_list.empty()) {
		QString path = workingDir() / subdir_git_objects_pack;
		QDirIterator it(path, { "pack-*.idx" }, QDir::Files | QDir::Readable);
		while (it.hasNext()) {
			it.next();
			GitPackIdxPtr idx = GitPackIdxPtr(new GitPackIdxV2());
			idx->basename = it.fileInfo().baseName();
			idx->parse(it.filePath());
			git_idx_list.push_back(idx);
		}
		qDebug() << "idx loaded";
	}
}

void GitObjectManager::clearIndexes()
{
	git_idx_list.clear();
}

void GitObjectManager::applyDelta(QByteArray *base_obj, QByteArray *delta_obj, QByteArray *out)
{
	if (delta_obj->size() > 0) {
		uint8_t const *begin = (uint8_t const *)delta_obj->data();
		uint8_t const *end = begin + delta_obj->size();
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
		(void)a;
		(void)b;
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
				out->append(base_obj->data() + offset, length);
			} else { // insert operation
				int length = op & 0x7f;
				if (ptr + length <= end) {
					out->append((char const *)ptr, length);
					ptr += length;
				}
			}
		}
	}
}

bool GitObjectManager::loadPackedObject(GitPackIdxPtr idx, QIODevice *packfile, GitPackIdxItem const *item, GitPack::Object *out)
{
	GitPack::Info info;
	if (GitPack::seekPackedObject(packfile, item, &info)) {
		GitPackIdxItem const *source_item = nullptr;
		if (info.type == GitPack::Type::OFS_DELTA) {
			source_item = idx->item(item->offset - info.offset);
		} else if (info.type == GitPack::Type::REF_DELTA) {
			source_item = idx->item(info.ref_id);
		}
		if (source_item) { // if deltified object
			GitPack::Object source;
			if (source_item && loadPackedObject(idx, packfile, source_item, &source)) {
				GitPack::Object delta;
				if (GitPack::load(packfile, item, &delta)) {
					QByteArray ba;
					applyDelta(&source.content, &delta.content, &ba);
					*out = GitPack::Object();
					out->type = source.type;
					out->content = std::move(ba);
					out->expanded_size = out->content.size();
					return true;
				}
			}
			qDebug() << Q_FUNC_INFO << "failed";
			return false;
		}
	}
	if (GitPack::load(packfile, item, out)) {
		return true;
	}
	return false;
}

bool GitObjectManager::extractObjectFromPackFile(GitPackIdxPtr idx, GitPackIdxItem const *item, GitPack::Object *out)
{
	*out = GitPack::Object();
	QString packfilepath = workingDir() / subdir_git_objects_pack / (idx->basename + ".pack");
	QFile packfile(packfilepath);
	if (packfile.open(QFile::ReadOnly)) {
		if (loadPackedObject(idx, &packfile, item, out)) {
			if (out->type == GitPack::Type::TREE) {
				GitPack::decodeTree(&out->content);
			}
			return true;
		}
	}
	return false;
}

bool GitObjectManager::extractObjectFromPackFile(QString const &id, QByteArray *out)
{
	loadIndexes();

	for (GitPackIdxPtr idx : git_idx_list) {
		GitPackIdxItem const *item = idx->item(id);
		if (item) {
			GitPack::Object obj;
			if (extractObjectFromPackFile(idx, item, &obj)) {
				*out = std::move(obj.content);
				return true;
			}
			qDebug() << Q_FUNC_INFO << "failed";
			return false;
		}
	}
	return false;
}

QString GitObjectManager::findObjectPath(const QString &id)
{
	if (Git::isValidID(id)) {
		int count = 0;
		QString absolute_path;
		QString xx = id.mid(0, 2); // leading two xdigits
		QString name = id.mid(2);  // remaining xdigits
		QString dir = workingDir() / subdir_git_objects / xx; // e.g. /home/user/myproject/.git/objects/5a
		QDirIterator it(dir, QDir::Files);
		while (it.hasNext()) {
			it.next();
			if (it.fileName().startsWith(name)) {
				QString id = xx + it.fileName(); // complete id
				if (id.size() == 40 && Git::isValidID(id)) {
					absolute_path = dir / it.fileName();
					count++;
				}
			}
		}
		if (count == 1) return absolute_path;
		if (count > 1) qDebug() << Q_FUNC_INFO << "ambiguous id" << id;
	}
	return QString(); // not found
}

bool GitObjectManager::loadObject(QString const &id, QByteArray *out)
{
	QString path = findObjectPath(id);
	if (!path.isEmpty()) {
		QFile file(path);
		if (file.open(QFile::ReadOnly)) {
			if (GitPack::decompress(&file, 1000000000, out)) {
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

bool GitObjectManager::catFile(const QString &id, QByteArray *out)
{
	if (loadObject(id, out)) return true;
	if (extractObjectFromPackFile(id, out)) return true;
	return false;
}


//

size_t GitObjectCache::size() const
{
	size_t size = 0;
	for (ItemPtr const &item : items) {
		size += item->ba.size();
	}
	return size;
}

void GitObjectCache::setup(GitPtr g)
{
	object_manager.setup(g->dup());
}

QByteArray GitObjectCache::catFile(const QString &id)
{
	{
		QMutexLocker lock(&object_manager.mutex);
		size_t n = items.size();
		size_t i = n;
		while (i > 0) {
			i--;
			if (items[i]->id == id) {
				ItemPtr item = items[i];
				if (i + 1 < n) {
					items.erase(items.begin() + i);
					items.push_back(item);
				}
//				qDebug() << "hit: " << id;
				return item->ba;
			}
		}

		while (size() > 100000000) { // 100MB
			items.erase(items.begin());
		}
	}

	QByteArray ba;

	auto Store = [&](){
		QMutexLocker lock(&object_manager.mutex);
		Item *item = new Item();
		item->id = id;
		item->ba = std::move(ba);
		items.push_back(ItemPtr(item));
		return item->ba;
	};

	if (object_manager.catFile(id, &ba)) {
		return Store();
	}

	if (object_manager.git()->cat_file(id, &ba)) {
		return Store();
	}

	return QByteArray();
}


