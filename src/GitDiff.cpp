#include "GitDiff.h"

#include <QDebug>
#include <QThread>
#include "MainWindow.h"

// TreeItem

struct TreeItem {
	enum Type {
		UNKNOWN,
		TREE,
		BLOB,
	};
	Type type = UNKNOWN;
	QString name;
	QString id;
	QString mode;

	QString to_string_() const
	{
		QString t;
		switch (type) {
		case TREE: t = "TREE"; break;
		case BLOB: t = "BLOB"; break;
		}
		return QString("GitTreeItem:{ %1 %2 %3 %4 }").arg(t).arg(id).arg(mode).arg(name);
	}
};

bool parseTree(GitPtr g, GitObjectCache *objcache, QString const &id, QString const &path_prefix, TreeItemList *out)
{
	out->clear();
	if (g && !id.isEmpty()) {
		QByteArray ba = objcache->cat_file(g, id);
		if (!ba.isEmpty()) { // 内容を取得
			QString s = QString::fromUtf8(ba);
			QStringList lines = misc::splitLines(s);
			for (QString const &line : lines) {
				int tab = line.indexOf('\t'); // タブより後ろにパスがある
				if (tab > 0) {
					QString stat = line.mid(0, tab); // タブの手前まで
					QStringList vals = misc::splitWords(stat); // 空白で分割
					if (vals.size() >= 3) {
						TreeItem data;
						data.mode = vals[0]; // ファイルモード
						data.id = vals[2]; // id（ハッシュ値）
						QString type = vals[1]; // 種類（tree/blob）
						QString path = line.mid(tab + 1); // パス
						path = Git::trimPath(path);
						data.name = path_prefix.isEmpty() ? path : misc::joinWithSlash(path_prefix, path);
						if (type == "tree") {
							data.type = TreeItem::TREE;
							out->push_back(data);
						} else if (type == "blob") {
							data.type = TreeItem::BLOB;
							out->push_back(data);
						}
					}
				}
			}
			return true;
		}
	}
	return false;
}

// PathToIdMap

class GitDiff::LookupTable {
private:
public:
	std::map<QString, QString> path_to_id_map;
	std::map<QString, QString> id_to_path_map;
public:

	typedef std::map<QString, QString>::const_iterator const_iterator;

	void store(QString const &path, QString const &id)
	{
		path_to_id_map[path] = id;
		id_to_path_map[id] = path;
	}

	void store(TreeItemList const &files)
	{
		for (TreeItem const &cd : files) {
			store(cd.name, cd.id);
		}
	}

	const_iterator find_path(QString const &path) const
	{
		return path_to_id_map.find(path);
	}

	const_iterator end_path() const
	{
		return path_to_id_map.end();
	}
};

// GitCommit

class GitCommit {
public:
	QString tree_id;
	QStringList parents;

	bool parseCommit(GitPtr g, GitObjectCache *objcache, QString const &id);
};

bool GitCommit::parseCommit(GitPtr g, GitObjectCache *objcache, const QString &id)
{
	parents.clear();
	if (g && !id.isEmpty()) {
		QStringList parents;
		{
			QByteArray ba = objcache->cat_file(g, id);
			if (!ba.isEmpty()) {
				QStringList lines = misc::splitLines(QString::fromUtf8(ba));
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

// GitDiff

QString GitDiff::makeKey(const QString &a_id, const QString &b_id)
{
	return  a_id + ".." + b_id;
}

QString GitDiff::makeKey(Git::Diff const &diff)
{
	return  makeKey(diff.blob.a_id, diff.blob.b_id);
}

class CommitListThread : public QThread {
private:
	struct Data {
		GitPtr g;
		QString id;
		QString dir;
		GitObjectCache *objcache;
	};
	Data d;
public:
	void run()
	{
		parseTree(d.g, d.objcache, d.id, d.dir, &files);
	}
public:
	GitCommit commit;
	TreeItemList files;
	CommitListThread(GitPtr g, GitObjectCache *objcache, const QString &id, const QString &dir)
	{
		d.g = g;
		d.objcache = objcache;
		d.id = id;
		d.dir = dir;
	}
};

void GitDiff::diff_tree_(GitPtr g, const QString &dir, QString older_id, QString newer_id)
{
#if SINGLE_THREAD
	CommitListThread older(g->dup(), objcache, older_id, dir);
	older.run();
	CommitListThread newer(g->dup(), objcache, newer_id, dir);
	newer.run();
#else // multi thread
	CommitListThread older(g->dup(), objcache, older_id, dir);
	CommitListThread newer(g->dup(), objcache, newer_id, dir);
	older.start();
	newer.start();
	older.wait();
	newer.wait();
#endif

	MapList diffmap;

	diffmap.push_back(LookupTable());
	LookupTable &map = diffmap.front();
	for (TreeItem const &cd : older.files) {
		checkInterrupted();
		map.store(cd.name, cd.id);
	}

	commit_into_map(g, &newer.files, &diffmap); // recursive
}

void GitDiff::AddItem(Git::Diff *item, QList<Git::Diff> *diffs)
{
	item->diff = QString("diff --git a/%1 b/%2").arg(item->path).arg(item->path);
	item->index = QString("index %1..%2 %3").arg(item->blob.a_id).arg(item->blob.b_id).arg(item->mode);
	diffs->push_back(*item);
}

void GitDiff::commit_into_map(GitPtr g, TreeItemList const *files, const GitDiff::MapList *diffmap)
{
	for (TreeItem const &cd : *files) {
		bool found = false;
		for (LookupTable const &map : *diffmap) { // map（新しいコミット）の中を探す
			checkInterrupted();
			auto it = map.find_path(cd.name);
			if (it != map.end_path()) {
				found = true; // 見つかった
				if (cd.id != it->second) { // 変更されている
					if (cd.type == TreeItem::TREE) { // ディレクトリ
						QString path = cd.name;
						QString older_id = it->second;
						QString newer_id = cd.id;
						diff_tree_(g, path, older_id, newer_id); // 子ディレクトリを探索
					} else if (cd.type == TreeItem::BLOB) { // ファイル
						Git::Diff item;
						item.path = cd.name;
						item.mode = cd.mode;
						item.blob.a_id = it->second;
						item.blob.b_id = cd.id;
						AddItem(&item, &diffs);
					}
					break;
				}
			}
		}
		if (!found) { // 新しく追加されたファイル
			if (cd.type == TreeItem::TREE) {
				QString path = cd.name;//misc::joinWithSlash(dir, cd.path); // 子ディレクトリ名
				diff_tree_(g, path, QString(), cd.id); // 子ディレクトリを探索
			} else {
				Git::Diff item;
				item.path = cd.name;
				item.mode = cd.mode;
				item.blob.b_id = cd.id;
				AddItem(&item, &diffs);
			}
		}
	}
}

void GitDiff::parseTree_(GitPtr g, GitObjectCache *objcache, const QString &dir, const QString &id, std::set<QString> *dirset, GitDiff::MapList *path_to_id_map)
{
	if (!dir.isEmpty()) {
		auto it = dirset->find(dir);
		if (it != dirset->end()) {
			return;
		}
		dirset->insert(dir);
	}

	TreeItemList files;
	parseTree(g, objcache, id, dir, &files);
	path_to_id_map->push_back(LookupTable());
	LookupTable &map = path_to_id_map->front();
	for (TreeItem const &cd : files) {
		map.store(cd.name, cd.id);
	}
}

QString GitDiff::prependPathPrefix(QString const &path)
{
	return PATH_PREFIX + path;
}

QString GitDiff::diffFile(GitPtr g, QString const &a_id, QString const &b_id)
{
	QString path_prefix = PATH_PREFIX;
	if (b_id.startsWith(path_prefix)) {
		QString path = b_id.mid(path_prefix.size());
		return g->diff_to_file(a_id, path);
	} else {
		return g->diff(a_id, b_id);
	}
}

void GitDiff::parseDiff(QString const &s, Git::Diff const *info, Git::Diff *out)
{
	QStringList lines = misc::splitLines(s);

	out->diff = QString("diff --git ") + ("a/" + info->path) + ' ' + ("b/" + info->path);
	out->index = QString("index ") + info->blob.a_id + ".." + info->blob.b_id + ' ' + info->mode;
	out->path = info->path;
	out->blob = info->blob;

	bool atat = false;
	for (QString const &line : lines) {
		ushort c = line.utf16()[0];
		if (c == '@') {
			if (line.startsWith("@@ ")) {
				out->hunks.push_back(Git::Hunk());
				out->hunks.back().at = line;
				atat = true;
			}
		} else {
			if (atat && c == '\\') { // e.g. \ No newline at end of file...
				// ignore this line
			} else {
				if (atat) {
					if (c == ' ' || c == '-' || c == '+') {
						// nop
					} else {
						atat = false;
					}
				}
				if (atat) {
					if (!out->hunks.isEmpty()) {
						out->hunks.back().lines.push_back(line);
					}
				}
			}
		}
	}
}

bool GitDiff::diff(QString id, QList<Git::Diff> *out, bool uncommited)
{
	if (!g) return false;

	out->clear();
	this->objcache = objcache;

	auto MakeDiffMapList = [&](GitPtr g, QStringList const &parents, MapList *path_to_id_map){
		for (QString parent : parents) {
			GitCommit list;
			TreeItemList files;
			list.parseCommit(g, objcache, parent);
			parseTree(g, objcache, list.tree_id, QString(), &files);
			path_to_id_map->push_back(LookupTable());
			LookupTable &map = path_to_id_map->front();
			for (TreeItem const &cd : files) {
				map.store(cd.name, cd.id);
			}
		}
	};

	try {
		if (uncommited) {

			QString parent = g->rev_parse_HEAD(); // HEADのインデックスを取得
			QStringList parents;
			parents.push_back(parent);


			MapList diffmaplist;
			MakeDiffMapList(g, parents, &diffmaplist);

			std::set<QString> dirset;

			Git::FileStatusList stats = g->status(); // git status

			for (Git::FileStatus const &fs : stats) {
				QString file = fs.path1();
				QStringList list = file.split('/');
				QString path;
				for (int i = 0; i + 1 < list.size(); i++) {
					MapList diffmaplist2;
					QString const &s = list[i];
					path = misc::joinWithSlash(path, s);
					for (LookupTable const &map : diffmaplist) {
						checkInterrupted();
						auto it = map.find_path(path);
						if (it != map.end_path()) {
							QString id = it->second;
							parseTree_(g, objcache, path, id, &dirset, &diffmaplist2);
							break;
						}
					}
					diffmaplist.insert(diffmaplist.end(), diffmaplist2.begin(), diffmaplist2.end());
				}
			}
			for (Git::FileStatus const &st : stats) {
				bool done = false;
				for (LookupTable const &map : diffmaplist) {
					checkInterrupted();
					auto it = map.find_path(st.path1());
					if (it != map.end_path()) {
						Git::Diff item;
						item.blob.a_id = it->second;
						item.blob.b_id = prependPathPrefix(st.path1());
						item.path = st.path1();
						if (st.code() == Git::FileStatusCode::RenamedInIndex) {
							item.blob.b_id = prependPathPrefix(st.path2());
							item.path = st.path2();
						}
						AddItem(&item, &diffs);
						done = true;
						break;
					}
				}
				if (!done) {
					Git::Diff item;
					item.blob.b_id = prependPathPrefix(st.path1());
					item.path = st.path1();
					AddItem(&item, &diffs);
				}
			}

		} else {

			{ // diff_raw test
				TreeItemList files;
				GitCommit newcommit;
				newcommit.parseCommit(g, objcache, id);
				parseTree(g, objcache, newcommit.tree_id, QString(), &files);

				if (newcommit.parents.isEmpty()) {
					for (TreeItem const &d : files) {
						Git::Diff diff(d.id, d.name, d.mode);
						diffs.push_back(diff);
					}
				} else {
					std::map<QString, Git::Diff> diffmap;

					std::set<QString> deleted_set;
					QList<Git::DiffRaw> list;
					for (QString const &parent : newcommit.parents) {
						QList<Git::DiffRaw> l = g->diff_raw(parent, id);
						for (Git::DiffRaw const &item : l) {
							if (item.state.startsWith('D')) {
								deleted_set.insert(item.a.id);
							} else {
								list.push_back(item);
							}
						}
					}
					for (Git::DiffRaw const &item : list) {
						QString file;
						if (!item.files.isEmpty()) {
							file = item.files.front();
						}
						Git::Diff diff;
						diff.diff = QString("diff --git a/%1 b/%2").arg(file).arg(file);
						diff.index = QString("index %1..%2 %3").arg(item.a.id).arg(item.b.id).arg(item.b.mode);
						diff.path = file;
						diff.mode = item.b.mode;
						if (Git::isValidID(item.a.id)) diff.blob.a_id = item.a.id;
						if (Git::isValidID(item.b.id)) diff.blob.b_id = item.b.id;

						if (!diff.blob.a_id.isEmpty()) {
							if (!diff.blob.b_id.isEmpty()) {
								diff.type = Git::Diff::Type::Changed;
							} else {
								diff.type = Git::Diff::Type::Deleted;
							}
						} else if (!diff.blob.b_id.isEmpty()) {
							if (deleted_set.find(diff.blob.b_id) != deleted_set.end()) {
								diff.type = Git::Diff::Type::Renamed;
							} else {
								diff.type = Git::Diff::Type::Added;
							}
						}

						if (diffmap.find(diff.path) == diffmap.end()) {
							diffmap[diff.path] = diff;
						}
					}

					for (auto const &pair : diffmap) {
						diffs.push_back(pair.second);

					}
				}
			}
		}

		std::sort(diffs.begin(), diffs.end(), [](Git::Diff const &left, Git::Diff const &right){
			return left.path.compare(right.path, Qt::CaseInsensitive) < 0;
		});

		*out = std::move(diffs);
		return true;
	} catch (Interrupted &) {
		out->clear();
	}
	return false;
}


class CommitTree {
public:
	GitPtr g;
	GitObjectCache *objcache;
	TreeItemList root_item_list;

	CommitTree(GitPtr g, GitObjectCache *objcache)
	{
		this->g = g;
		this->objcache = objcache;
	}

	QString lookup(QString const &file)
	{
		int i = file.lastIndexOf('/');
		if (i >= 0) {
			QString subdir = file.mid(0, i);
			QString name = file.mid(i + 1);
			QString id = lookup(subdir);
			TreeItemList list;
			if (parseTree(g, objcache, id, QString(), &list)) {
				for (TreeItem const &d : list) {
					if (d.name == name) {
						return d.id;
					}
				}
			}
		} else {
			for (TreeItem const &d : root_item_list) {
				if (d.name == file) {
					return d.id;
				}
			}
		}
		return QString();
	}

	void parseCommit(QString const &commit_id)
	{
		GitCommit commit;
		commit.parseCommit(g, objcache, commit_id);
		parseTree(g, objcache, commit.tree_id, QString(), &root_item_list);
	}
};

QString lookupFileID(GitPtr g, GitObjectCache *objcache, const QString &commit_id, const QString &file)
// 指定されたコミットに属するファイルのIDを求める
{
	CommitTree commit_tree(g, objcache);
	commit_tree.parseCommit(commit_id);
	return commit_tree.lookup(file);
}



