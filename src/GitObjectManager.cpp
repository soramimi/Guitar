
#include "GitObjectManager.h"
#include "Git.h"
#include "common/joinpath.h"
#include "common/misc.h"
#include <QBuffer>
#include <QDebug>
#include <QDirIterator>
#include <QFile>
#include <memory>

GitObjectManager::GitObjectManager()
{
	subdir_git_objects = ".git/objects";
	subdir_git_objects_pack = subdir_git_objects / "pack";
}

void GitObjectManager::setup(GitPtr const &g)
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
			GitPackIdxPtr idx = std::make_shared<GitPackIdxV2>();
			idx->basename = it.fileInfo().baseName();
			idx->parse(it.filePath());
			git_idx_list.push_back(idx);
		}
	}
}

void GitObjectManager::clearIndexes()
{
	git_idx_list.clear();
}

void GitObjectManager::applyDelta(QByteArray const *base_obj, QByteArray const *delta_obj, QByteArray *out)
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
		// cf. https://github.com/github/git-msysgit/blob/master/patch-delta.c
		while (ptr < end) {
			uint8_t op = *ptr;
			ptr++;
			if (op & 0x80) { // copy operation
				uint32_t offset = 0;
				uint32_t length = 0;
				if (op & 0x01) offset = *ptr++;
				if (op & 0x02) offset |= (*ptr++ << 8);
				if (op & 0x04) offset |= (*ptr++ << 16);
				if (op & 0x08) offset |= ((unsigned) *ptr++ << 24);
				if (op & 0x10) length = *ptr++;
				if (op & 0x20) length |= (*ptr++ << 8);
				if (op & 0x40) length |= (*ptr++ << 16);
				if (length == 0) length = 0x10000;

				if (offset + length > (uint32_t)base_obj->size()) {
					qDebug() << Q_FUNC_INFO << "base-file or delta-file is corrupted";
					out->clear();
					return;
				}
				out->append(base_obj->data() + offset, length);
			} else if (op > 0) { // insert operation
				int length = op & 0x7f;
				if (ptr + length <= end) {
					out->append((char const *)ptr, length);
					ptr += length;
				}
			} else {
				qDebug() << Q_FUNC_INFO << "unexpected delta opcode 0";
			}
		}
	}
}

bool GitObjectManager::loadPackedObject(GitPackIdxPtr const &idx, QIODevice *packfile, GitPackIdxItem const *item, GitPack::Object *out)
{
	GitPack::Info info;
	if (GitPack::seekPackedObject(packfile, item, &info)) {
		GitPackIdxItem const *source_item = nullptr;
		if (info.type == Git::Object::Type::OFS_DELTA) {
			source_item = idx->item(item->offset - info.offset);
		} else if (info.type == Git::Object::Type::REF_DELTA) {
			source_item = idx->item(info.ref_id);
		}
		if (source_item) { // if deltified object
			GitPack::Object source;
			if (source_item && loadPackedObject(idx, packfile, source_item, &source)) {
				GitPack::Object delta;
				if (GitPack::load(packfile, item, &delta)) {
					if (delta.checksum != item->checksum) {
						qDebug() << "crc checksum incorrect";
						return false;
					}
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
	return GitPack::load(packfile, item, out);
}

bool GitObjectManager::extractObjectFromPackFile(GitPackIdxPtr const &idx, GitPackIdxItem const *item, GitPack::Object *out)
{
	*out = GitPack::Object();
	QString packfilepath = workingDir() / subdir_git_objects_pack / (idx->basename + ".pack");
	QFile packfile(packfilepath);
	if (packfile.open(QFile::ReadOnly)) {
		if (loadPackedObject(idx, &packfile, item, out)) {
			if (out->type == Git::Object::Type::TREE) {
				GitPack::decodeTree(&out->content);
			}
			return true;
		}
	}
	return false;
}

bool GitObjectManager::extractObjectFromPackFile(QString const &id, QByteArray *out, Git::Object::Type *type)
{
	loadIndexes();

	for (GitPackIdxPtr const &idx : git_idx_list) {
		GitPackIdxItem const *item = idx->item(id);
		if (item) {
			GitPack::Object obj;
			if (extractObjectFromPackFile(idx, item, &obj)) {
				*out = std::move(obj.content);
				*type = obj.type;
				return true;
			}
			qDebug() << Q_FUNC_INFO << "failed";
			return false;
		}
	}
	return false;
}

QString GitObjectManager::findObjectPath(QString const &id)
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
				if (id.size() == GIT_ID_LENGTH && Git::isValidID(id)) {
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

bool GitObjectManager::loadObject(QString const &id, QByteArray *out, Git::Object::Type *type)
{
	QString path = findObjectPath(id);
	if (!path.isEmpty()) {
		QFile file(path);
		if (file.open(QFile::ReadOnly)) {
			if (GitPack::decompress(&file, 1000000000, out)) {
				*type = GitPack::stripHeader(out);
				if (*type == Git::Object::Type::TREE) {
					GitPack::decodeTree(out);
				}
				return true;
			}
		}
	}
	return false;
}

bool GitObjectManager::catFile(QString const &id, QByteArray *out, Git::Object::Type *type)
{
	*type = Git::Object::Type::UNKNOWN;
	if (loadObject(id, out, type)) return true;
	if (extractObjectFromPackFile(id, out, type)) return true;
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

void GitObjectCache::setup(GitPtr const &g)
{
	items.clear();
	revparsemap.clear();
	if (g) {
		object_manager.setup(g->dup());
	}
}

QString GitObjectCache::revParse(QString const &name)
{
	GitPtr g = git();
	if (!g) return QString();

	{
		QMutexLocker lock(&object_manager.mutex);
		auto it = revparsemap.find(name);
		if (it != revparsemap.end()) {
			return it->second;
		}
	}

	QString id = g->rev_parse(name);

	{
		QMutexLocker lock(&object_manager.mutex);
		revparsemap[name] = id;
		return id;
	}
}

Git::Object GitObjectCache::catFile(QString const &id)
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
				Git::Object obj;
				obj.type = item->type;
				obj.content = item->ba;
				return obj;
			}
		}

		while (size() > 100000000) { // 100MB
			items.erase(items.begin());
		}
	}

	QByteArray ba;
	Git::Object::Type type = Git::Object::Type::UNKNOWN;

	auto Store = [&](){
		QMutexLocker lock(&object_manager.mutex);
		Item *item = new Item();
		item->id = id;
		item->ba = std::move(ba);
		item->type = type;
		items.push_back(ItemPtr(item));
		Git::Object obj;
		obj.type = item->type;
		obj.content = item->ba;
		return obj;
	};

	if (object_manager.catFile(id, &ba, &type)) { // 独自実装のファイル取得
		return Store();
	}

	if (true) {
		if (git()->cat_file(id, &ba)) { // 外部コマンド起動の git cat-file -p を試してみる
			// 上の独自実装のファイル取得が正しく動作していれば、ここには来ないはず
			qDebug() << __LINE__ << __FILE__ << Q_FUNC_INFO << id;
			return Store();
		}
	}

	qDebug() << "failed to cat file: " << id;

	return Git::Object();
}

QString GitObjectCache::getCommitIdFromTag(QString const &tag)
{
	QString commit_id;
	GitPtr g = git();
	if (g && g->isValidWorkingCopy()) {
		QString id = g->rev_parse(tag);
		Git::Object obj = catFile(id);
		switch (obj.type) {
		case Git::Object::Type::COMMIT:
			commit_id = id;
			break;
		case Git::Object::Type::TAG:
			if (!obj.content.isEmpty()) {
				misc::splitLines(obj.content, [&](char const *ptr, size_t len){
					if (commit_id.isEmpty()) {
						if (len >= 7 + GIT_ID_LENGTH && strncmp(ptr, "object ", 7) == 0) {
							QString id = QString::fromUtf8(ptr + 7, len - 7).trimmed();
							if (Git::isValidID(id)) {
								commit_id = id;
							}
						}
					}
					return QString();
				});
			}
			break;
		}
	}
	return commit_id;
}




bool GitCommit::parseCommit(GitObjectCache *objcache, QString const &id)
{
	parents.clear();
	if (!id.isEmpty()) {
		QStringList parents;
		{
			Git::Object obj = objcache->catFile(id);
			if (!obj.content.isEmpty()) {
				QStringList lines = misc::splitLines(QString::fromUtf8(obj.content));
				for (QString const &line : lines) {
					int i = line.indexOf(' ');
					if (i < 1) break;
					QString key = line.mid(0, i);
					QString val = line.mid(i + 1).trimmed();
					if (key == "tree") {
						tree_id = val;
					} else if (key == "parent") {
						parents.push_back(val);
					}
				}
			}
		}
		if (!tree_id.isEmpty()) { // サブディレクトリ
			this->parents.append(parents);
			return true;
		}
	}
	return false;
}

