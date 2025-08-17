
#include "GitObjectManager.h"
#include "GitPack.h"
#include "GitPackIdxV2.h"
#include "MemoryReader.h"
#include "common/joinpath.h"
#include "common/misc.h"
#include <QBuffer>
#include <QDebug>
#include <QDirIterator>
#include <QFile>
#include <memory>
#include <set>

struct GitObjectManager::Private {
	std::mutex *mutex_ = nullptr;
	QString subdir_git_objects;
	QString subdir_git_objects_pack;
	std::vector<std::shared_ptr<GitPackIdxV2>> git_idx_list;
};

GitObjectManager::GitObjectManager(std::mutex *mutex)
	: m(new Private)
{
	m->mutex_ = mutex;
	init();
	setup();
}

GitObjectManager::GitObjectManager(GitObjectManager const &other)
	: m(new Private(*other.m))
{
}

GitObjectManager::~GitObjectManager()
{
	delete m;
}

GitObjectManager &GitObjectManager::operator =(const GitObjectManager &other)
{
	if (this != &other) {
		delete m;
		m = new Private(*other.m);
	}
	return *this;
}

void GitObjectManager::init()
{
	m->subdir_git_objects = ".git/objects";
	m->subdir_git_objects_pack = m->subdir_git_objects / "pack";
}

void GitObjectManager::setup()
{
	clearIndexes();
}

void GitObjectManager::loadIndexes(GitRunner g, std::mutex *mutex)
{
	auto Do = [&](){
		if (m->git_idx_list.empty()) {
			QString path = g.workingDir() / m->subdir_git_objects_pack;
			QDirIterator it(path, { "pack-*.idx" }, QDir::Files | QDir::Readable);
			while (it.hasNext()) {
				it.next();
				std::shared_ptr<GitPackIdxV2> idx = std::make_shared<GitPackIdxV2>();
				idx->pack_idx_path = it.filePath();
				idx->parse(idx->pack_idx_path, true);
				m->git_idx_list.push_back(idx);
			}
		}
	};

	if (mutex) {
		std::lock_guard lock(*mutex);
		Do();
	} else {
		Do();
	}
}

void GitObjectManager::clearIndexes()
{
	m->git_idx_list.clear();
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

bool GitObjectManager::loadPackedObject(std::shared_ptr<GitPackIdxV2> const &idx, QIODevice *packfile, GitPackIdxItem const *item, GitPackObject *out)
{
	GitPackInfo info;
	if (GitPack::seekPackedObject(packfile, item, &info)) {
		GitPackIdxItem const *source_item = nullptr;
		if (info.type == GitObject::Type::OFS_DELTA) {
			source_item = idx->item(item->offset - info.offset);
		} else if (info.type == GitObject::Type::REF_DELTA) {
			source_item = idx->item(GitHash(info.ref_id));
		}
		if (source_item) {
			GitPackObject source;
			if (source_item && loadPackedObject(idx, packfile, source_item, &source)) {
				GitPackObject delta;
				if (GitPack::load(packfile, item, &delta)) {
					if (delta.checksum != item->checksum) {
						qDebug() << "crc checksum incorrect";
						return false;
					}
					QByteArray ba;
					applyDelta(&source.content, &delta.content, &ba);
					*out = GitPackObject();
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

bool GitObjectManager::extractObjectFromPackFile(std::shared_ptr<GitPackIdxV2> const &idx, GitPackIdxItem const *item, GitPackObject *out)
{
	*out = GitPackObject();
	QFile packfile(idx->pack_file_path());
	if (packfile.open(QFile::ReadOnly)) {
		if (loadPackedObject(idx, &packfile, item, out)) {
			if (out->type == GitObject::Type::TREE) {
				GitPack::decodeTree(&out->content);
			}
			return true;
		}
	}
	return false;
}

bool GitObjectManager::extractObjectFromPackFile(GitRunner g, GitHash const &id, QByteArray *out, GitObject::Type *type, std::mutex *mutex)
{
	auto Do = [&](){
		loadIndexes(g, nullptr);

		for (std::shared_ptr<GitPackIdxV2> const &idx : m->git_idx_list) {
			GitPackIdxItem const *item = idx->item(id);
			if (item) {
				GitPackObject obj;
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
	};
	if (mutex) {
		std::lock_guard lock(*mutex);
		return Do();
	} else {
		return Do();
	}
}

QString GitObjectManager::findObjectPath(GitRunner g, GitHash const &id)
{
	if (id.isValid()) {
		int count = 0;
		QString absolute_path;
		QString name = id.toQString();
		QString xx = name.mid(0, 2); // leading two xdigits
		QString name2 = name.mid(2);  // remaining xdigits
		QString dir = g.workingDir() / m->subdir_git_objects / xx; // e.g. /home/user/myproject/.git/objects/5a

		std::optional<std::vector<GitFileItem>> ret = g.ls(dir);
		if (ret) {
			for (GitFileItem const &item : *ret) {
				if (item.name.startsWith(name2)) {
					QString id = xx + item.name; // complete id
					if (id.size() == GIT_ID_LENGTH && GitHash::isValidID(id)) {
						absolute_path = dir / item.name;
						count++;
					}
				}
			}
		} else {
			qDebug() << Q_FUNC_INFO << "failed to list objects in" << dir;
		}

		if (count == 1) return absolute_path;
		if (count > 1) qDebug() << Q_FUNC_INFO << "ambiguous id" << id.toQString();
	}
	return {};
}

bool GitObjectManager::loadObject(GitRunner g, GitHash const &id, QByteArray *out, GitObject::Type *type)
{
	QString path = findObjectPath(g, id);
	if (!path.isEmpty()) {
		auto ret = g.readfile(path);
		if (ret) {
			MemoryReader reader(ret->data(), ret->size());
			reader.open(MemoryReader::ReadOnly);
			if (GitPack::decompress(&reader, 1000000000, out)) {
				*type = GitPack::stripHeader(out);
				if (*type == GitObject::Type::TREE) {
					GitPack::decodeTree(out);
				}
				return true;
			}
		}
	}
	return false;
}

bool GitObjectManager::catFile(GitRunner g, GitHash const &id, QByteArray *out, GitObject::Type *type)
{
	*type = GitObject::Type::UNKNOWN;
	if (loadObject(g, id, out, type)) return true;
	if (extractObjectFromPackFile(g, id, out, type, m->mutex_)) return true;
	return false;
}

//

size_t GitObjectCache::size() const
{
	size_t size = 0;
	for (ItemPtr const &item : items_) {
		size += item->ba.size();
	}
	return size;
}

void GitObjectCache::clear()
{
	items_.clear();
	rev_parse_map_.clear();
	object_manager_.setup();
}

GitHash GitObjectCache::revParse(GitRunner g, QString const &name)
{
	if (!g.isValidWorkingCopy()) return {};

	GitHash ret;

	auto A = [&](){
		auto it = rev_parse_map_.find(name);
		if (it != rev_parse_map_.end()) {
			ret = it->second;
		}
	};
	if (mutex_) {
		std::lock_guard lock(*mutex_);
		A();
	} else {
		A();
	}
	if (ret.isValid()) {
		qDebug() << name << ret.toQString();
		return ret;
	}

	ret = g.rev_parse(name);

	auto B = [&](){
		rev_parse_map_[name] = ret;
	};
	if (mutex_) {
		std::lock_guard lock(*mutex_);
		B();
	} else {
		B();
	}
	return ret;
}

GitObject GitObjectCache::catFile(GitRunner g, GitHash const &id)
{
	auto Do = [&]()-> GitObject {
		size_t n = items_.size();
		size_t i = n;
		while (i > 0) {
			i--;
			Q_ASSERT(items_[i]);
			if (items_[i]->id == id) {
				ItemPtr item = items_[i];
				if (i + 1 < n) { // move to last
					items_.erase(items_.begin() + i);
					items_.push_back(item);
				}
				GitObject obj;
				obj.type = item->type;
				obj.content = item->ba;
				return obj;
			}
		}

		while (size() > 100000000) { // 100MB
			items_.erase(items_.begin());
		}

		return {};
	};
	GitObject obj;
	if (mutex_) {
		std::lock_guard lock(*mutex_);
		obj = Do();
	} else {
		obj = Do();
	}
	if (obj) return obj;

	QByteArray ba;
	GitObject::Type type = GitObject::Type::UNKNOWN;

	auto Store = [&](){
		GitObject obj;
		obj.type = type;
		obj.content = ba;
		{
			auto item = std::make_shared<Item>();
			item->id = id;
			item->ba = ba;
			item->type = type;
			items_.push_back(item);
		}
		return obj;
	};

	if (object_manager_.catFile(g, id, &ba, &type)) { // 独自実装のファイル取得
		if (mutex_) {
			std::lock_guard lock(*mutex_);
			return Store();
		} else {
			return Store();
		}
	}

	if (true) {
		auto ret = g.cat_file(id);
		if (ret) { // 外部コマンド起動の git cat-file -p を試してみる
			// 上の独自実装のファイル取得が正しく動作していれば、ここには来ないはず
			qDebug() << __FILE__ << __LINE__ << Q_FUNC_INFO << id.toQString();
			ba = *ret;
			if (mutex_) {
				std::lock_guard lock(*mutex_);
				return Store();
			} else {
				return Store();
			}
		}
	}

	qDebug() << "failed to cat file: " << id.toQString();

	return GitObject();
}

bool GitCommit::parseCommit(GitRunner g, GitObjectCache *objcache, GitHash const &id, GitCommit *out)
{
	*out = {};
	if (id.isValid()) {
		QStringList parents;
		{
			GitObject obj = objcache->catFile(g, id);
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
				path = gitTrimPath(path);
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

bool parseGitTreeObject(GitRunner g, GitObjectCache *objcache, const QString &commit_id, const QString &path_prefix, GitTreeItemList *out) // TODO: change commit_id as GitHash
{
	out->clear();
	if (!commit_id.isEmpty()) {
		GitObject obj = objcache->catFile(g, GitHash(commit_id));
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
	if (GitHash::isValidID(id)) {
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
