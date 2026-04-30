
#include "GitDiffManager.h"
#include <QDebug>
#include <QThread>
#include "ApplicationGlobal.h"
#include "MainWindow.h"
#include "common/q/helper.h"
#include "common/strformat.h"

// PathToIdMap

class GitDiffManager::LookupTable {
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
			store((QS)cd.name, (QS)cd.id);
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

GitRunner GitDiffManager::git_for_submodule(GitRunner g, GitSubmoduleItem const &submod)
{
	return global->mainwindow->git_for_submodule(g, submod);
}

std::string GitDiffManager::makeKey(std::string const &a_id, std::string const &b_id)
{
	return  a_id + ".." + b_id;
}

std::string GitDiffManager::makeKey(GitDiff const &diff)
{
	return makeKey(diff.blob.a_id_or_path, diff.blob.b_id_or_path);
}

std::string GitDiffManager::prependPathPrefix(std::string const &path)
{
	return PATH_PREFIX + path;
}

std::string GitDiffManager::diffObjects(GitRunner g, std::string const &a_id, std::string const &b_id)
{
	std::string path_prefix = PATH_PREFIX;
	if (misc::starts_with(b_id, path_prefix)) {
		std::string path = b_id.substr(path_prefix.size());
		return g.diff_to_file(a_id, path);
	} else {
		return g.diff(a_id, b_id);
	}
}

std::string GitDiffManager::diffFiles(GitRunner g, std::string const &a_path, std::string const &b_path)
{
	return g.diff_file(a_path, b_path);
}

GitDiff GitDiffManager::parseDiff(std::string const &s, GitDiff const *info)
{
	GitDiff out;

	std::vector<std::string_view> lines = misc::splitLinesV(s, false);

	out.diff = "diff --git " + ("a/" + info->path) + ' ' + ("b/" + info->path);
	out.index = "index " + info->blob.a_id_or_path + ".." + info->blob.b_id_or_path + ' ' + info->mode;
	out.path = info->path;
	out.blob = info->blob;
	out.a_submodule.item = info->a_submodule.item;
	out.a_submodule.commit = info->a_submodule.commit;
	out.b_submodule.item = info->b_submodule.item;
	out.b_submodule.commit = info->b_submodule.commit;
	out.mode = "0";

	bool atat = false;
	for (std::string_view const &s : lines) {
		std::string line = std::string{s};
		int c = line[0] & 0xff;
		if (c == '@') {
			if (strncmp(line.c_str(), "@@ ", 3) == 0) {
				out.hunks.push_back(GitHunk());
				out.hunks.back().at = line;
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
					if (!out.hunks.isEmpty()) {
						out.hunks.back().lines.push_back(line);
					}
				}
			}
		}
	}

	return out;
}

/**
 * @brief コミットの差分を取得する
 * @param id コミットID
 * @param out
 * @return
 */
std::vector<GitDiff> GitDiffManager::diff(GitRunner g, GitHash const &id, const std::vector<GitSubmoduleItem> &submodules)
{
	std::vector<GitDiff> diffs;

	if (id.isValid()) {

		{ // diff_raw
			GitTreeItemList files;
			GitCommit newer_commit;
			GitCommit::parseCommit(g, objcache_, id, &newer_commit);
			parseGitTreeObject(g, objcache_, newer_commit.tree_id, {}, &files);

			if (newer_commit.parents.empty()) { // 親がないなら最古のコミット
				auto F = [&](auto self, GitRunner g, std::string const &dir, GitTreeItemList const *files, std::vector<GitDiff> *diffs)-> void {
					for (GitTreeItem const &d : *files) {
						std::string path = misc::joinWithSlash(dir, d.name);
						if (d.type == GitTreeItem::BLOB) {
							GitDiff diff(d.id, path, d.mode);
							diffs->push_back(diff);
						} else if (d.type == GitTreeItem::TREE) {
							GitTreeItemList files2;
							parseGitTreeObject(g, objcache_, d.id, {}, &files2);
							self(self, g, path, &files2, diffs); // 再帰
						}
					}
				};
				F(F, g, {}, &files, &diffs); // ツリー全体を取得
			} else {
				std::map<std::string, GitDiff> diffmap;

				std::set<std::string> deleted_set;
				std::set<std::string> renamed_set;

				QList<GitDiffRaw> list;
				for (std::string const &parent : newer_commit.parents) {
					std::vector<GitDiffRaw> l = g.diff_raw(GitHash(parent), id);
					for (GitDiffRaw const &item : l) {
						if (misc::starts_with(item.state, 'D')) {
							deleted_set.insert(item.a.id);
						}
						list.push_back(item);
					}
				}
				for (GitDiffRaw const &item : list) {
					if (misc::starts_with(item.state, 'A') || misc::starts_with(item.state, 'C')) { // 追加されたファイル
						auto it = deleted_set.find(item.b.id); // 同じオブジェクトIDが削除リストに載っているなら
						if (it != deleted_set.end()) {
							renamed_set.insert(item.b.id); // 名前変更とみなす
						}
					} else if (misc::starts_with(item.state, 'R')) { // 名前変更されたファイル
						renamed_set.insert(item.b.id);
					}
				}
				for (GitDiffRaw const &item : list) {
					std::string file;
					if (!item.files.empty()) {
						file = item.files.back(); // 名前変更された場合はリストの最後が新しい名前
					}
					GitDiff diff;
					diff.diff = (QS)fmt("diff --git a/%s b/%s")(file)(file);
					diff.index = (QS)fmt("index %s..%s %s")(item.a.id)(item.b.id)(item.b.mode);
					diff.path = (QS)file;
					diff.mode = (QS)item.b.mode;
					if (GitHash::isValidID(item.a.id)) diff.blob.a_id_or_path = (QS)item.a.id;
					if (GitHash::isValidID(item.b.id)) diff.blob.b_id_or_path = (QS)item.b.id;

					diff.type = GitDiff::Type::Unknown;
					char state = item.state[0];
					switch (state) {
					case 'A': diff.type = GitDiff::Type::Create;   break;
					case 'C': diff.type = GitDiff::Type::Copy;     break;
					case 'D': diff.type = GitDiff::Type::Delete;   break;
					case 'M': diff.type = GitDiff::Type::Modify;   break;
					case 'R': diff.type = GitDiff::Type::Rename;   break;
					case 'T': diff.type = GitDiff::Type::ChType;   break;
					case 'U': diff.type = GitDiff::Type::Unmerged; break;
					}

					if (diff.type != GitDiff::Type::Unknown) {
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

		GitHash head_id = g.rev_parse("HEAD");
		std::vector<GitFileStatus> stats = g.status_s(); // git status // TODO: 巨大リポジトリで遅い

		GitCommitTree head_tree(objcache_);
		head_tree.parseCommit(g, head_id); // HEADが親

		std::string zeros(GIT_ID_LENGTH, '0');

		for (GitFileStatus const &fs : stats) {
			std::string path = fs.path1();
			GitDiff item;

			GitTreeItem treeitem;
			if (head_tree.lookup(g, (QS)path, &treeitem)) {
				item.blob.a_id_or_path = (QS)treeitem.id; // HEADにおけるこのファイルのID
				if (fs.isDeleted()) { // 削除されてる
					item.blob.b_id_or_path = zeros; // 削除された
				} else {
					item.blob.b_id_or_path = prependPathPrefix((QS)path); // IDの代わりに実在するファイルパスを入れる
				}
				item.mode = (QS)treeitem.mode;
			} else {
				item.blob.a_id_or_path = zeros;
				item.blob.b_id_or_path = prependPathPrefix((QS)path); // 実在するファイルパス
			}

			item.diff = (QS)fmt("diff --git a/%s b/%s")(path)(path);
			item.index = (QS)fmt("index %s..%s %s")(item.blob.a_id_or_path)(zeros)(item.mode);
			item.path = (QS)path;

			diffs.push_back(item);
		}
	}

	// get submodule details
	// TraceLogger trace("debug", "get submodule details");
	constexpr int num_threads = 8;
	std::atomic_size_t diffs_index(0);
	std::vector<std::thread> threads(num_threads);
	for (size_t thread_index = 0; thread_index < threads.size(); thread_index++) {
		threads[thread_index] = std::thread([&](){
			while (1) {
				size_t i = diffs_index++;
				if ((int)i >= diffs.size()) break; // 終了
				GitDiff *diff = &diffs[i];
				if (!diff->isSubmodule()) continue;

				auto Do = [this, &g, &submodules](GitDiff *diff){
					for (auto j = 0; j < submodules.size(); j++) {
						GitSubmoduleItem const &submod = submodules[j];
						if (submod.path != diff->path) continue;
						auto GetSubmoduleDetail = [&](GitRunner g, std::string const &id){
							GitDiff::SubmoduleDetail out;
							if (misc::starts_with(id, PATH_PREFIX)) {
								out.item = submod;
								out.item.id = g.rev_parse("HEAD");
								auto commit = g.queryCommitItem(out.item.id);
								if (commit) {
									out.commit = *commit;
								}
							} else if (GitHash::isValidID(id)) {
								out.item = submod;
								out.item.id = GitHash(id);
								auto commit = g.queryCommitItem(out.item.id);
								if (commit) {
									out.commit = *commit;
								}
							}
							return out;
						};
						auto a = std::async(std::launch::async, GetSubmoduleDetail, git_for_submodule(g, submod), diff->blob.a_id_or_path);
						auto b = std::async(std::launch::async, GetSubmoduleDetail, git_for_submodule(g, submod), diff->blob.b_id_or_path);
						diff->a_submodule = a.get();
						diff->b_submodule = b.get();
						break;
					}

				};

				Do(diff);
			}
		});
	}
	for (size_t thread_index = 0; thread_index < threads.size(); thread_index++) {
		threads[thread_index].join();
	}

	std::sort(diffs.begin(), diffs.end(), [](GitDiff const &left, GitDiff const &right){
		return misc::stricmp(left.path, right.path) < 0;
	});
	// trace.end();

	return diffs;
}

std::vector<GitDiff> GitDiffManager::diff_uncommited(GitRunner g, const std::vector<GitSubmoduleItem> &submodules)
{
	return diff(g, {}, submodules);
}

// GitCommitTree

GitCommitTree::GitCommitTree(GitObjectCache *objcache)
	: objcache(objcache)
{
}

QString GitCommitTree::lookup_(GitRunner g, QString const &file, GitTreeItem *out)
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
				tree_id = lookup_(g, subdir, out);
			}
		}
		GitTreeItemList list;
		if (parseGitTreeObject(g, objcache, tree_id.toStdString(), {}, &list)) {
			QString return_id;
			for (GitTreeItem const &d : list) {
				QString itemname = (QS)d.name;
				if (itemname == name) {
					return_id = (QS)d.id;
				}
				QString path = misc::joinWithSlash(subdir, itemname);
				if (d.type == GitTreeItem::BLOB) {
					if (out && itemname == name) {
						*out = d;
					}
					blob_map[path] = d;
				} else if (d.type == GitTreeItem::TREE) {
					tree_id_map[path] = (QS)d.id;
				}
			}
			return return_id;
		}
	} else {
		QString return_id;
		for (GitTreeItem const &d : root_item_list) {
			QString itemname = (QS)d.name;
			if (itemname == file) {
				return_id = (QS)d.id;
			}
			if (d.type == GitTreeItem::BLOB) {
				if (out && itemname == file) {
					*out = d;
				}
				blob_map[itemname] = d;
			} else if (d.type == GitTreeItem::TREE) {
				tree_id_map[itemname] = (QS)d.id;
			}
		}
		return return_id;
	}
	return QString();
}

QString GitCommitTree::lookup(GitRunner g, QString const &file)
{
	auto it = blob_map.find(file);
	if (it != blob_map.end()) {
		return (QS)it->second.id;
	}
	return lookup_(g, file, nullptr);
}

bool GitCommitTree::lookup(GitRunner g, QString const &file, GitTreeItem *out)
{
	*out = GitTreeItem();
	auto it = blob_map.find(file);
	if (it != blob_map.end()) {
		*out = it->second;
		return true;
	}
	return !lookup_(g, file, out).isEmpty();
}

void GitCommitTree::parseTree(GitRunner g, QString const &tree_id)
{
	parseGitTreeObject(g, objcache, (QS)tree_id, {}, &root_item_list);
}

QString GitCommitTree::parseCommit(GitRunner g, GitHash const &commit_id)
{
	GitCommit commit;
	GitCommit::parseCommit(g, objcache, commit_id, &commit);
	parseTree(g, (QS)commit.tree_id);
	return (QS)commit.tree_id;
}

//

/**
 * @brief 指定されたコミットに属するファイルのIDを求める
 * @param objcache
 * @param commit_id
 * @param file
 * @return
 */
QString lookupFileID(GitRunner g, GitObjectCache *objcache, GitHash const &commit_id, QString const &file)
{
	GitCommitTree commit_tree(objcache);
	commit_tree.parseCommit(g, commit_id);
	return commit_tree.lookup(g, file);
}
