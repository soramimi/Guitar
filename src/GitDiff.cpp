
#include "GitDiff.h"
#include "BasicMainWindow.h"
#include <QDebug>
#include <QThread>

// PathToIdMap

class GitDiff::LookupTable {
private:
public:
	std::map<QString, QString> path_to_id_map;
	std::map<QString, QString> id_to_path_map;
public:

	using const_iterator = std::map<QString, QString>::const_iterator;

	void store(QString const &path, QString const &id)
	{
		path_to_id_map[path] = id;
		id_to_path_map[id] = path;
	}

	void store(GitTreeItemList const &files)
	{
		for (GitTreeItem const &cd : files) {
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

// GitDiff

GitPtr GitDiff::git()
{
	return objcache->git();
}

GitPtr GitDiff::git(Git::SubmoduleItem const &submod)
{
	return objcache->git(submod);
}

QString GitDiff::makeKey(QString const &a_id, QString const &b_id)
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

QString GitDiff::diffObjects(GitPtr const &g, QString const &a_id, QString const &b_id)
{
	QString path_prefix = PATH_PREFIX;
	if (b_id.startsWith(path_prefix)) {
		QString path = b_id.mid(path_prefix.size());
		return g->diff_to_file(a_id, path);
	} else {
		return g->diff(a_id, b_id);
	}
}

QString GitDiff::diffFiles(GitPtr const &g, QString const &a_path, QString const &b_path)
{
	return g->diff_file(a_path, b_path);
}

void GitDiff::parseDiff(std::string const &s, Git::Diff const *info, Git::Diff *out)
{
	std::vector<std::string> lines;
	{
		char const *begin = s.c_str();
		char const *end = begin + s.size();
		misc::splitLines(begin, end, &lines, false);
	}

	out->diff = QString("diff --git ") + ("a/" + info->path) + ' ' + ("b/" + info->path);
	out->index = QString("index ") + info->blob.a_id + ".." + info->blob.b_id + ' ' + info->mode;
	out->path = info->path;
	out->blob = info->blob;
	out->a_submodule.item = info->a_submodule.item;
	out->a_submodule.commit = info->a_submodule.commit;
	out->b_submodule.item = info->b_submodule.item;
	out->b_submodule.commit = info->b_submodule.commit;

	bool atat = false;
	for (std::string const &line : lines) {
		int c = line[0] & 0xff;
		if (c == '@') {
			if (strncmp(line.c_str(), "@@ ", 3) == 0) {
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

void GitDiff::retrieveCompleteTree(QString const &dir, GitTreeItemList const *files, std::map<QString, GitTreeItem> *out)
{
	for (GitTreeItem const &d : *files) {
		QString path = misc::joinWithSlash(dir, d.name);
		if (d.type == GitTreeItem::BLOB) {
			(*out)[path] = d;
		} else if (d.type == GitTreeItem::TREE) {
			GitTreeItemList files2;
			parseGitTreeObject(objcache, d.id, QString(), &files2);
			retrieveCompleteTree(path, &files2, out);
		}
	}
}

void GitDiff::retrieveCompleteTree(QString const &dir, GitTreeItemList const *files)
{
	for (GitTreeItem const &d : *files) {
		QString path = misc::joinWithSlash(dir, d.name);
		if (d.type == GitTreeItem::BLOB) {
			Git::Diff diff(d.id, path, d.mode);
			diffs.push_back(diff);
		} else if (d.type == GitTreeItem::TREE) {
			GitTreeItemList files2;
			parseGitTreeObject(objcache, d.id, QString(), &files2);
			retrieveCompleteTree(path, &files2);
		}
	}
}

/**
 * @brief コミットの差分を取得する
 * @param id コミットID
 * @param out
 * @return
 */
bool GitDiff::diff(QString const &id, const QList<Git::SubmoduleItem> &submodules, QList<Git::Diff> *out)
{
	out->clear();
	diffs.clear();

	try {
		if (Git::isValidID(id)) { // 有効なID

			{ // diff_raw
				GitTreeItemList files;
				GitCommit newer_commit;
				GitCommit::parseCommit(objcache, id, &newer_commit);
				parseGitTreeObject(objcache, newer_commit.tree_id, QString(), &files);

				if (newer_commit.parents.isEmpty()) { // 親がないなら最古のコミット
					retrieveCompleteTree(QString(), &files); // ツリー全体を取得
				} else {
					std::map<QString, Git::Diff> diffmap;

					std::set<QString> deleted_set;
					std::set<QString> renamed_set;

					QList<Git::DiffRaw> list;
					for (QString const &parent : newer_commit.parents) {
						QList<Git::DiffRaw> l = git()->diff_raw(parent, id);
						for (Git::DiffRaw const &item : l) {
							if (item.state.startsWith('D')) {
								deleted_set.insert(item.a.id);
							}
							list.push_back(item);
						}
					}
					for (Git::DiffRaw const &item : list) {
						if (item.state.startsWith('A') || item.state.startsWith('C')) { // 追加されたファイル
							auto it = deleted_set.find(item.b.id); // 同じオブジェクトIDが削除リストに載っているなら
							if (it != deleted_set.end()) {
								renamed_set.insert(item.b.id); // 名前変更とみなす
							}
						} else if (item.state.startsWith('R')) { // 名前変更されたファイル
							renamed_set.insert(item.b.id);
						}
					}
					for (Git::DiffRaw const &item : list) {
						QString file;
						if (!item.files.isEmpty()) {
							file = item.files.back(); // 名前変更された場合はリストの最後が新しい名前
						}
						Git::Diff diff;
						diff.diff = QString("diff --git a/%1 b/%2").arg(file).arg(file);
						diff.index = QString("index %1..%2 %3").arg(item.a.id).arg(item.b.id).arg(item.b.mode);
						diff.path = file;
						diff.mode = item.b.mode;
						if (Git::isValidID(item.a.id)) diff.blob.a_id = item.a.id;
						if (Git::isValidID(item.b.id)) diff.blob.b_id = item.b.id;

#if 0
						if (!diff.blob.a_id.isEmpty()) {
							if (!diff.blob.b_id.isEmpty()) {
								if (renamed_set.find(diff.blob.b_id) != renamed_set.end()) {
									diff.type = Git::Diff::Type::Rename;
								} else {
									diff.type = Git::Diff::Type::Modify;
								}
							} else {
								if (renamed_set.find(diff.blob.a_id) != renamed_set.end()) { // 名前変更されたオブジェクトなら
									diff.type = Git::Diff::Type::Unknown; // マップに追加しない
								} else {
									diff.type = Git::Diff::Type::Delete; // 削除されたオブジェクト
								}
							}
						} else if (!diff.blob.b_id.isEmpty()) {
							if (renamed_set.find(diff.blob.b_id) != renamed_set.end()) {
								diff.type = Git::Diff::Type::Rename;
							} else {
								diff.type = Git::Diff::Type::Create;
							}
						}
#else
						diff.type = Git::Diff::Type::Unknown;
						int state = item.state.utf16()[0];
						switch (state) {
						case 'A': diff.type = Git::Diff::Type::Create;   break;
						case 'C': diff.type = Git::Diff::Type::Copy;     break;
						case 'D': diff.type = Git::Diff::Type::Delete;   break;
						case 'M': diff.type = Git::Diff::Type::Modify;   break;
						case 'R': diff.type = Git::Diff::Type::Rename;   break;
						case 'T': diff.type = Git::Diff::Type::ChType;   break;
						case 'U': diff.type = Git::Diff::Type::Unmerged; break;
						}

#endif

						if (diff.type != Git::Diff::Type::Unknown) {
							if (diffmap.find(diff.path) == diffmap.end()) {
								diffmap[diff.path] = diff;
							}
						}
					}

					for (auto const &pair : diffmap) {
						diffs.push_back(pair.second);
					}
				}
			}
		} else { // 無効なIDなら、HEADと作業コピーのdiff

			GitPtr g = objcache->git();
			QString head_id = objcache->revParse("HEAD");
			Git::FileStatusList stats = g->status_s(); // git status

			GitCommitTree head_tree(objcache);
			head_tree.parseCommit(head_id); // HEADが親

			QString zeros(GIT_ID_LENGTH, '0');

			for (Git::FileStatus const &fs : stats) {
				QString path = fs.path1();
				Git::Diff item;

				GitTreeItem treeitem;
				if (head_tree.lookup(path, &treeitem)) {
					item.blob.a_id = treeitem.id; // HEADにおけるこのファイルのID
					if (fs.isDeleted()) { // 削除されてる
						item.blob.b_id = zeros; // 削除された
					} else {
						item.blob.b_id = prependPathPrefix(path); // IDの代わりに実在するファイルパスを入れる
					}
					item.mode = treeitem.mode;
				} else {
					item.blob.a_id = zeros;
					item.blob.b_id = prependPathPrefix(path); // 実在するファイルパス
				}

				item.diff = QString("diff --git a/%1 b/%2").arg(path).arg(path);
				item.index = QString("index %1..%2 %3").arg(item.blob.a_id).arg(zeros).arg(item.mode);
				item.path = path;

				diffs.push_back(item);
			}
		}

		for (int i = 0; i < diffs.size(); i++) {
			Git::Diff *diff = &diffs[i];
			if (diff->isSubmodule()) {
				for (int j = 0; j < submodules.size(); j++) {
					if (submodules[j].path == diff->path) {
						diff->a_submodule.item = diff->b_submodule.item = submodules[j];
						diff->a_submodule.item.id = diff->blob.a_id;
						diff->b_submodule.item.id = diff->blob.b_id;

						GitPtr g = git(submodules[j]);
						g->queryCommit(diff->a_submodule.item.id, &diff->a_submodule.commit);
						g->queryCommit(diff->b_submodule.item.id, &diff->b_submodule.commit);

						// なぜか逆に来ることがあるみたい？
						if (diff->a_submodule.commit.commit_date > diff->b_submodule.commit.commit_date) {
							std::swap(diff->a_submodule, diff->b_submodule);
						}
						break;
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

bool GitDiff::diff_uncommited(const QList<Git::SubmoduleItem> &submodules, QList<Git::Diff> *out)
{
	return diff({}, submodules, out);
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

QString GitCommitTree::lookup_(QString const &file, GitTreeItem *out)
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
		GitTreeItemList list;
		if (parseGitTreeObject(objcache, tree_id, QString(), &list)) {
			QString return_id;
			for (GitTreeItem const &d : list) {
				if (d.name == name) {
					return_id = d.id;
				}
				QString path = misc::joinWithSlash(subdir, d.name);
				if (d.type == GitTreeItem::BLOB) {
					if (out && d.name == name) {
						*out = d;
					}
					blob_map[path] = d;
				} else if (d.type == GitTreeItem::TREE) {
					tree_id_map[path] = d.id;
				}
			}
			return return_id;
		}
	} else {
		QString return_id;
		for (GitTreeItem const &d : root_item_list) {
			if (d.name == file) {
				return_id = d.id;
			}
			if (d.type == GitTreeItem::BLOB) {
				if (out && d.name == file) {
					*out = d;
				}
				blob_map[d.name] = d;
			} else if (d.type == GitTreeItem::TREE) {
				tree_id_map[d.name] = d.id;
			}
		}
		return return_id;
	}
	return QString();
}

QString GitCommitTree::lookup(QString const &file)
{
	auto it = blob_map.find(file);
	if (it != blob_map.end()) {
		return it->second.id;
	}
	return lookup_(file, nullptr);
}

bool GitCommitTree::lookup(QString const &file, GitTreeItem *out)
{
	*out = GitTreeItem();
	auto it = blob_map.find(file);
	if (it != blob_map.end()) {
		*out = it->second;
		return true;
	}
	return !lookup_(file, out).isEmpty();
}

void GitCommitTree::parseTree(QString const &tree_id)
{
	parseGitTreeObject(objcache, tree_id, QString(), &root_item_list);
}

QString GitCommitTree::parseCommit(QString const &commit_id)
{
	GitCommit commit;
	GitCommit::parseCommit(objcache, commit_id, &commit);
	parseTree(commit.tree_id);
	return commit.tree_id;
}

//

/**
 * @brief 指定されたコミットに属するファイルのIDを求める
 * @param objcache
 * @param commit_id
 * @param file
 * @return
 */
QString lookupFileID(GitObjectCache *objcache, QString const &commit_id, QString const &file)
{
	GitCommitTree commit_tree(objcache);
	commit_tree.parseCommit(commit_id);
	QString id = commit_tree.lookup(file);
	return id;
}
