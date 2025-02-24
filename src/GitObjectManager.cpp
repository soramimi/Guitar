
#include "GitObjectManager.h"
#include "Git.h"
#include "common/joinpath.h"
#include "common/misc.h"
#include <QBuffer>
#include <QDebug>
#include <QDirIterator>
#include <QFile>
#include <memory>
#include <set>
#include "Profile.h"

GitObjectManager::GitObjectManager()
{
	init();
	setup();
}

void GitObjectManager::init()
{
	subdir_git_objects = ".git/objects";
	subdir_git_objects_pack = subdir_git_objects / "pack";
}

void GitObjectManager::setup()
{
	clearIndexes();
}

void GitObjectManager::loadIndexes(GitRunner g)
{
	// QMutexLocker lock(&mutex);
	if (git_idx_list.empty()) {
		QString path = g.workingDir() / subdir_git_objects_pack;
		QDirIterator it(path, { "pack-*.idx" }, QDir::Files | QDir::Readable);
		while (it.hasNext()) {
			it.next();
			GitPackIdxPtr idx = std::make_shared<GitPackIdxV2>();
			idx->pack_idx_path = it.filePath();
			idx->parse(idx->pack_idx_path, true);
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
			source_item = idx->item(Git::Hash(info.ref_id));
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
	QFile packfile(idx->pack_file_path());
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

bool GitObjectManager::extractObjectFromPackFile(GitRunner g, Git::Hash const &id, QByteArray *out, Git::Object::Type *type)
{
	loadIndexes(g);

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

QString GitObjectManager::findObjectPath(GitRunner g, Git::Hash const &id)
{
	if (Git::isValidID(id)) {
		int count = 0;
		QString absolute_path;
		QString name = id.toQString();
		QString xx = name.mid(0, 2); // leading two xdigits
		QString name2 = name.mid(2);  // remaining xdigits
		QString dir = g.workingDir() / subdir_git_objects / xx; // e.g. /home/user/myproject/.git/objects/5a
		QDirIterator it(dir, QDir::Files);
		while (it.hasNext()) {
			it.next();
			if (it.fileName().startsWith(name2)) {
				QString id = xx + it.fileName(); // complete id
				if (id.size() == GIT_ID_LENGTH && Git::isValidID(id)) {
					absolute_path = dir / it.fileName();
					count++;
				}
			}
		}
		if (count == 1) return absolute_path;
		if (count > 1) qDebug() << Q_FUNC_INFO << "ambiguous id" << id.toQString();
	}
	return {};
}

bool GitObjectManager::loadObject(GitRunner g, Git::Hash const &id, QByteArray *out, Git::Object::Type *type)
{
	QString path = findObjectPath(g, id);
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

bool GitObjectManager::catFile(GitRunner g, Git::Hash const &id, QByteArray *out, Git::Object::Type *type)
{
	*type = Git::Object::Type::UNKNOWN;
	if (loadObject(g, id, out, type)) return true;
	if (extractObjectFromPackFile(g, id, out, type)) return true;
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

void GitObjectCache::clear()
{
	items.clear();
	revparsemap.clear();
	object_manager.setup();
}

Git::Hash GitObjectCache::revParse(GitRunner g, QString const &name)
{
	if (!g.isValidWorkingCopy()) return {};

	{
		// QMutexLocker lock(&object_manager.mutex);
		auto it = revparsemap.find(name);
		if (it != revparsemap.end()) {
			return it->second;
		}
	}

	auto id = g.rev_parse(name);

	{
		// QMutexLocker lock(&object_manager.mutex);
		revparsemap[name] = id;
		return id;
	}
}

Git::Object GitObjectCache::catFile(GitRunner g, Git::Hash const &id, std::mutex *mutex)
{
	// PROFILE;

	class LockGuard {
	private:
		std::mutex *mutex_;
	public:
		LockGuard(std::mutex *mutex)
			: mutex_(mutex)
		{
			if (mutex) mutex->lock();
		}
		~LockGuard()
		{
			if (mutex_) mutex_->unlock();
		}
	};

	{
		LockGuard lock(mutex);

		size_t n = items.size();
		size_t i = n;
		while (i > 0) {
			i--;
			if (item_id(i) == id) {
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
		Git::Object obj;
		obj.type = type;
		obj.content = ba;
		{
			LockGuard lock(mutex);

			auto item = std::make_shared<Item>();
			item->id = id;
			item->ba = ba;
			item->type = type;
			items.push_back(item);
		}
		return obj;
	};

	if (object_manager.catFile(g, id, &ba, &type)) { // 独自実装のファイル取得
		return Store();
	}

	if (true) {
		auto ret = g.cat_file(id);
		if (ret) { // 外部コマンド起動の git cat-file -p を試してみる
			// 上の独自実装のファイル取得が正しく動作していれば、ここには来ないはず
			qDebug() << __FILE__ << __LINE__ << Q_FUNC_INFO << id.toQString();
			ba = *ret;
			return Store();
		}
	}

	qDebug() << "failed to cat file: " << id.toQString();

	return Git::Object();
}

bool GitCommit::parseCommit(GitRunner g, GitObjectCache *objcache, Git::Hash const &id, GitCommit *out)
{
	*out = {};
	if (id.isValid()) {
		QStringList parents;
		{
			Git::Object obj = objcache->catFile(g, id);
			if (!obj.content.isEmpty()) {
				QStringList lines = misc::splitLines(QString::fromUtf8(obj.content));
				for (QString const &line : lines) {
					int i = line.indexOf(' ');
					if (i < 1) break;
					QString key = line.mid(0, i);
					QString val = line.mid(i + 1).trimmed();
					if (key == "tree") {
						out->tree_id = val;
					} else if (key == "parent") {
						parents.push_back(val);
					}
				}
			}
		}
		if (!out->tree_id.isEmpty()) { // サブディレクトリ
			out->parents.append(parents);
			return true;
		}
	}
	return false;
}

void parseGitTreeObject(QByteArray const &ba, const QString &path_prefix, GitTreeItemList *out)
{
	*out = {};
	QString s = QString::fromUtf8(ba);
	QStringList lines = misc::splitLines(s);
	for (QString const &line : lines) {
		int tab = line.indexOf('\t'); // タブより後ろにパスがある
		if (tab > 0) {
			QString stat = line.mid(0, tab); // タブの手前まで
			QStringList vals = misc::splitWords(stat); // 空白で分割
			if (vals.size() >= 3) {
				GitTreeItem data;
				data.mode = vals[0]; // ファイルモード
				data.id = vals[2]; // id（ハッシュ値）
				QString type = vals[1]; // 種類（tree/blob）
				QString path = line.mid(tab + 1); // パス
				path = Git::trimPath(path);
				data.name = path_prefix.isEmpty() ? path : misc::joinWithSlash(path_prefix, path);
				if (type == "tree") {
					data.type = GitTreeItem::TREE;
				} else if (type == "blob") {
					data.type = GitTreeItem::BLOB;
				} else if (type == "commit") {
					data.type = GitTreeItem::COMMIT;
				}
				if (data.type != GitTreeItem::UNKNOWN) {
					out->push_back(data);
				}
			}
		}
	}
}

bool parseGitTreeObject(GitRunner g, GitObjectCache *objcache, const QString &commit_id, const QString &path_prefix, GitTreeItemList *out) // TODO: change commit_id as Git::Hash
{
	out->clear();
	if (!commit_id.isEmpty()) {
		Git::Object obj = objcache->catFile(g, Git::Hash(commit_id));
		if (!obj.content.isEmpty()) { // 内容を取得
			parseGitTreeObject(obj.content, path_prefix, out);
			return true;
		}
	}
	return false;
}

/**
 * @brief GitObjectManager::findObject
 * @param id
 * @param repo_local_dir
 */
QStringList GitObjectManager::findObject(const QString &id, QString const &repo_local_dir)
{
	QStringList list;
	std::set<QString> set;
	if (Git::isValidID(id)) {
		{
			QString a = id.mid(0, 2);
			QString b = id.mid(2);
			QString dir = repo_local_dir / ".git/objects" / a;
			QDirIterator it(dir);
			while (it.hasNext()) {
				it.next();
				QFileInfo info = it.fileInfo();
				if (info.isFile()) {
					QString c = info.fileName();
					if (c.startsWith(b)) {
						set.insert(set.end(), a + c);
					}
				}
			}
		}
		{
			QString dir = repo_local_dir / ".git/objects/pack";
			QDirIterator it(dir);
			while (it.hasNext()) {
				it.next();
				QFileInfo info = it.fileInfo();
				if (info.isFile() && info.fileName().startsWith("pack-") && info.fileName().endsWith(".idx")) {
					GitPackIdxV2 idx;
					idx.parse(info.absoluteFilePath(), false);
					idx.each([&](GitPackIdxItem const *item){
						QString item_id = GitPackIdxItem::qid(*item);
						if (item_id.startsWith(id)) {
							set.insert(item_id);
						}
						return true;
					});
				}
			}
		}
		for (QString const &s : set) {
			list.push_back(s);
		}
	}
	return list;
}
