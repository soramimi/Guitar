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

bool parse_tree_(GitObjectCache *objcache, QString const &commit_id, QString const &path_prefix, TreeItemList *out)
{
	out->clear();
	if (!commit_id.isEmpty()) {
		QByteArray ba = objcache->catFile(commit_id);
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

	bool parseCommit(GitObjectCache *objcache, QString const &id);
};

bool GitCommit::parseCommit(GitObjectCache *objcache, const QString &id)
{
	parents.clear();
	if (!id.isEmpty()) {
		QStringList parents;
		{
			QByteArray ba = objcache->catFile(id);
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

GitPtr GitDiff::git()
{
	return objcache->git();
}

QString GitDiff::makeKey(const QString &a_id, const QString &b_id)
{
	return  a_id + ".." + b_id;
}

QString GitDiff::makeKey(Git::Diff const &diff)
{
	return  makeKey(diff.blob.a_id, diff.blob.b_id);
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

void GitDiff::retrieveCompleteTree(QString const &dir, TreeItemList const *files, std::map<QString, TreeItem> *out)
{
	for (TreeItem const &d : *files) {
		QString path = misc::joinWithSlash(dir, d.name);
		if (d.type == TreeItem::BLOB) {
			(*out)[path] = d;
		} else if (d.type == TreeItem::TREE) {
			TreeItemList files2;
			parse_tree_(objcache, d.id, QString(), &files2);
			retrieveCompleteTree(path, &files2, out);
		}
	}
}

void GitDiff::retrieveCompleteTree(QString const &dir, TreeItemList const *files)
{
	for (TreeItem const &d : *files) {
		QString path = misc::joinWithSlash(dir, d.name);
		if (d.type == TreeItem::BLOB) {
			Git::Diff diff(d.id, path, d.mode);
			diffs.push_back(diff);
		} else if (d.type == TreeItem::TREE) {
			TreeItemList files2;
			parse_tree_(objcache, d.id, QString(), &files2);
			retrieveCompleteTree(path, &files2);
		}
	}
}

bool GitDiff::diff(QString id, QList<Git::Diff> *out)
{
	out->clear();
	diffs.clear();

	try {
		if (Git::isValidID(id)) { // 有効なID

			{ // diff_raw test
				TreeItemList files;
				GitCommit newer_commit;
				newer_commit.parseCommit(objcache, id);
				parse_tree_(objcache, newer_commit.tree_id, QString(), &files);

				if (newer_commit.parents.isEmpty()) { // 親がないなら最古のコミット
					retrieveCompleteTree(QString(), &files); // ツリー全体を取得
				} else {
					std::map<QString, Git::Diff> diffmap;

					std::set<QString> deleted_set;
					QList<Git::DiffRaw> list;
					for (QString const &parent : newer_commit.parents) {
						QList<Git::DiffRaw> l = git()->diff_raw(parent, id);
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
		} else { // 無効なIDなら、HEADと作業コピーのdiff

			GitCommitTree head_tree(objcache);
			head_tree.parseCommit(git()->rev_parse_HEAD()); // HEADが親

			Git::FileStatusList stats = git()->status(); // git status

			QString zero40(40, '0');

			for (Git::FileStatus const &fs : stats) {
				QString path = fs.path1();
				Git::Diff item;

				TreeItem treeitem;
				if (head_tree.lookup(path, &treeitem)) {
					item.blob.a_id = treeitem.id; // HEADにおけるこのファイルのID
					if (fs.isDeleted()) { // 削除されてる
						item.blob.b_id = zero40; // 削除された
					} else {
						item.blob.b_id = prependPathPrefix(path); // IDの代わりに実在するファイルパスを入れる
					}
					item.mode = treeitem.mode;
				} else {
					item.blob.a_id = zero40;
					item.blob.b_id = prependPathPrefix(path); // 実在するファイルパス
				}

				item.diff = QString("diff --git a/%1 b/%2").arg(path).arg(path);
				item.index = QString("index %1..%2 %3").arg(item.blob.a_id).arg(zero40).arg(item.mode);
				item.path = path;

				diffs.push_back(item);
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

bool GitDiff::diff_uncommited(QList<Git::Diff> *out)
{
	return diff(QString(), out);
}

// GitCommitTree

GitCommitTree::GitCommitTree(GitObjectCache *objcache)
	: objcache(objcache)
{
}

GitPtr GitCommitTree::git()
{
	return objcache->git();
}

QString GitCommitTree::lookup_(const QString &file, TreeItem *out)
{
	int i = file.lastIndexOf('/');
	if (i >= 0) {
		QString subdir = file.mid(0, i);
		QString name = file.mid(i + 1);
		QString tree_id;
		{
			auto it = tree_id_map.find(subdir);
			if (it != tree_id_map.end()) {
				tree_id = it->second;
			} else {
				tree_id = lookup_(subdir, out);
			}
		}
		TreeItemList list;
		if (parse_tree_(objcache, tree_id, QString(), &list)) {
			QString return_id;
			for (TreeItem const &d : list) {
				if (d.name == name) {
					return_id = d.id;
				}
				QString path = misc::joinWithSlash(subdir, d.name);
				if (d.type == TreeItem::BLOB) {
					if (out && d.name == name) {
						*out = d;
					}
					blob_map[path] = d;
				} else if (d.type == TreeItem::TREE) {
					tree_id_map[path] = d.id;
				}
			}
			return return_id;
		}
	} else {
		QString return_id;
		for (TreeItem const &d : root_item_list) {
			if (d.name == file) {
				return_id = d.id;
			}
			if (d.type == TreeItem::BLOB) {
				if (out && d.name == file) {
					*out = d;
				}
				blob_map[d.name] = d;
			} else if (d.type == TreeItem::TREE) {
				tree_id_map[d.name] = d.id;
			}
		}
		return return_id;
	}
	return QString();
}

QString GitCommitTree::lookup(const QString &file)
{
	auto it = blob_map.find(file);
	if (it != blob_map.end()) {
		return it->second.id;
	}
	return lookup_(file, nullptr);
}

bool GitCommitTree::lookup(const QString &file, TreeItem *out)
{
	*out = TreeItem();
	auto it = blob_map.find(file);
	if (it != blob_map.end()) {
		*out = it->second;
		return true;
	}
	return !lookup_(file, out).isEmpty();
}

void GitCommitTree::parseTree(const QString &tree_id)
{
	parse_tree_(objcache, tree_id, QString(), &root_item_list);
}

void GitCommitTree::parseCommit(const QString &commit_id)
{
	GitCommit commit;
	commit.parseCommit(objcache, commit_id);
	parseTree(commit.tree_id);
}

//

QString lookupFileID(GitObjectCache *objcache, const QString &commit_id, const QString &file)
// 指定されたコミットに属するファイルのIDを求める
{
	GitCommitTree commit_tree(objcache);
	commit_tree.parseCommit(commit_id);
	QString id = commit_tree.lookup(file);
	return id;
}
