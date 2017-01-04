#include "GitDiff.h"

#include <QThread>
#include "MainWindow.h"

// CommitData

struct GitDiff::CommitData {
	enum Type {
		UNKNOWN,
		TREE,
		BLOB,
	};
	Type type = UNKNOWN;
	QString path;
	QString id;
	QString mode;
	QString parent;
};

// IndexMap

class GitDiff::IndexMap {
private:
	std::map<QString, QString> map;
public:

	typedef std::map<QString, QString>::const_iterator const_iterator;

	void store(QString const &path, QString const &index)
	{
		map[path] = index;
	}

	void store(QList<CommitData> const &files)
	{
		for (CommitData const &cd : files) {
			store(cd.path, cd.id);
		}
	}

	const_iterator find(QString const &path) const
	{
		return map.find(path);
	}

	const_iterator end() const
	{
		return map.end();
	}
};

// CommitList

class GitDiff::CommitList {
public:
	QList<CommitData> files;
	QStringList parents;

	bool parseTree(GitPtr g, QString const &index, QString const &dir, bool append);
	bool parseCommit(GitPtr g, QString const &index, bool append);
};

bool GitDiff::CommitList::parseTree(GitPtr g, const QString &index, const QString &dir, bool append)
{
	if (!append) {
		files.clear();
	}
	if (g && !index.isEmpty()) {
		QByteArray ba;
		if (g->cat_file(index, &ba)) {
			QString s = QString::fromUtf8(ba);
			QStringList lines = misc::splitLines(s);
			for (QString const &line : lines) {
				int tab = line.indexOf('\t');
				if (tab > 0) {
					QString stat = line.mid(0, tab);
					QStringList vals = misc::splitWords(stat);
					if (vals.size() >= 3) {
						CommitData data;
						data.mode = vals[0];
						data.id = vals[2];
						QString type = vals[1];
						QString path = line.mid(tab + 1);
                        path = Git::trimPath(path);
						data.path = misc::joinWithSlash(dir, path);
						if (type == "tree") {
							data.type = CommitData::TREE;
							files.push_back(data);
						} else if (type == "blob") {
							data.type = CommitData::BLOB;
							files.push_back(data);
						}
					}
				}
			}
			return true;
		}
	}
	return false;
}

bool GitDiff::CommitList::parseCommit(GitPtr g, const QString &index, bool append)
{
	if (!append) {
		files.clear();
		parents.clear();
	}
	if (g && !index.isEmpty()) {
		QString tree;
		QStringList parents;
		{
			QByteArray ba;
			if (g->cat_file(index, &ba)) {
				QStringList lines = misc::splitLines(QString::fromUtf8(ba));
				for (QString const &line : lines) {
					int i = line.indexOf(' ');
					if (i < 1) break;
					QString key = line.mid(0, i);
					QString val = line.mid(i + 1).trimmed();
					if (key == "tree") {
						tree = val;
					} else if (key == "parent") {
						parents.push_back(val);
					}
				}
			}
		}
		if (!tree.isEmpty()) {
			CommitList commit;
			if (commit.parseTree(g, tree, QString(), true)) {
				this->files.append(commit.files);
				this->parents.append(parents);
				return true;
			}
		}
	}
	return false;
}

// GitDiff

class CommitListThread : public QThread {
private:
	struct Data {
		GitPtr g;
		QString index;
		QString dir;
	};
	Data d;
	void run()
	{
		commit.parseTree(d.g, d.index, d.dir, false);
	}
public:
	GitDiff::CommitList commit;
	CommitListThread(GitPtr g, const QString &index, const QString &dir)
	{
		d.g = g;
		d.index = index;
		d.dir = dir;
	}
};

void GitDiff::diff_tree_(GitPtr g, const QString &dir, QString older_index, QString newer_index, QList<Git::Diff> *diffs)
{
	CommitListThread older(g->dup(), older_index, dir);
	CommitListThread newer(g->dup(), newer_index, dir);
	older.start();
	newer.start();
	older.wait();
	newer.wait();

	MapList diffmap;

	diffmap.push_back(IndexMap());
	IndexMap &map = diffmap.front();
	for (CommitData const &cd : older.commit.files) {
		map.store(cd.path, cd.id);
	}

	commit_into_map(g, dir, newer.commit, &diffmap, diffs);
}

QString GitDiff::makeKey(Git::Diff::BLOB_AB const &ab)
{
	return  ab.a.id + ".." + ab.b.id;

}

void GitDiff::AddItem(Git::Diff *item, QList<Git::Diff> *diffs)
{
	item->blob.a.path = item->path;
	item->blob.b.path = item->path;
	item->diff = QString("diff --git ") + ("a/" + item->path) + ' ' + ("b/" + item->path);
	item->index = QString("index ") + makeKey(item->blob) + ' ' + item->mode;
	diffs->push_back(*item);
}

void GitDiff::commit_into_map(GitPtr g, const QString &dir, const GitDiff::CommitList &commit, const GitDiff::MapList *diffmap, QList<Git::Diff> *diffs)
{
	for (CommitData const &cd : commit.files) {
		for (IndexMap const &map : *diffmap) {
			auto it = map.find(cd.path);
			if (it != map.end()) {
				if (cd.id != it->second) {
					if (cd.type == CommitData::TREE) {
						QString path = misc::joinWithSlash(dir, cd.path);
						diff_tree_(g, path, it->second, cd.id, diffs);
					} else if (cd.type == CommitData::BLOB) {
						Git::Diff item;
						item.path = cd.path;
						item.mode = cd.mode;
						item.blob.a.id = it->second;
						item.blob.b.id = cd.id;
						AddItem(&item, diffs);
					}
					break;
				}
			} else {
				Git::Diff item;
				item.path = cd.path;
				item.mode = cd.mode;
				item.blob.b.id = cd.id;
				AddItem(&item, diffs);
			}
		}
	}
}

void GitDiff::file_into_map(const Git::FileStatusList &stats, const GitDiff::MapList *diffmap, QList<Git::Diff> *diffs)
{
	for (Git::FileStatus const &st : stats) {
		for (IndexMap const &map : *diffmap) {
			auto it = map.find(st.path1());
			if (it != map.end()) {
				Git::Diff item;
				item.blob.a.id = it->second;
				item.blob.b.id = PATH_PREFIX + st.path1();
				item.path = st.path1();
				AddItem(&item, diffs);
				break;
			}
		}
	}
}

void GitDiff::parse_commit(GitPtr g, const QString &dir, const QString &index, GitDiff::MapList *diffmaplist)
{
	CommitList oldcommit;
	oldcommit.parseCommit(g, index, true);
	diffmaplist->push_back(IndexMap());
	IndexMap &map = diffmaplist->front();
	for (CommitData const &cd : oldcommit.files) {
		QString path = misc::joinWithSlash(dir, cd.path);
		map.store(path, cd.id);
	}
}

void GitDiff::parse_tree(GitPtr g, const QString &dir, const QString &index, std::set<QString> *dirset, GitDiff::MapList *diffmaplist)
{
	if (!dir.isEmpty()) {
		auto it = dirset->find(dir);
		if (it != dirset->end()) {
			return;
		}
		dirset->insert(dir);
	}

	CommitList commit;
	commit.parseTree(g, index, dir, false);
	diffmaplist->push_back(IndexMap());
	IndexMap &map = diffmaplist->front();
	for (CommitData const &cd : commit.files) {
		map.store(cd.path, cd.id);
	}
}

QString GitDiff::diffFile(GitPtr g, QString const &a_id, QString const &b_id)
{
	QString path_prefix = PATH_PREFIX;
	if (b_id.startsWith(path_prefix)) {
		return g->diff_to_file(a_id, b_id.mid(path_prefix.size()));
	} else {
		return g->diff(a_id, b_id);
	}
}

void GitDiff::parseDiff(QString const &s, Git::Diff const *ref, Git::Diff *out)
{
	QStringList lines = misc::splitLines(s);

	out->diff = QString("diff --git ") + ("a/" + ref->blob.a.path) + ' ' + ("b/" + ref->blob.b.path);
	out->index = QString("index ") + ref->blob.a.id + ".." + ref->blob.b.id + ' ' + ref->mode;
	out->path = ref->path;
	out->blob = ref->blob;

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

void GitDiff::diff(GitPtr g, QString index, QList<Git::Diff> *out)
{
	if (!g) return;

	out->clear();

	if (index == "HEAD") {

		QString parent = g->rev_parse_HEAD(); // HEADのインデックスを取得

		Git::FileStatusList stats = g->status(); // git status

		MapList diffmaplist;

		parse_commit(g, QString(), parent, &diffmaplist);

		std::set<QString> dirset;

		for (Git::FileStatus const &s : stats) {
			QStringList list = s.path1().split('/');
			QString path;
			for (int i = 0; i + 1 < list.size(); i++) {
				QString const &s = list[i];
				path = misc::joinWithSlash(path, s);
				for (IndexMap const &map : diffmaplist) {
					auto it = map.find(path);
					if (it != map.end()) {
						QString index = it->second;
						parse_tree(g, path, index, &dirset, &diffmaplist);
					}
				}

			}
		}

		file_into_map(stats, &diffmaplist, &diffs);

	} else {

		CommitList newcommit;
		newcommit.parseCommit(g, index, false);

		MapList diffmaplist;

		CommitList oldcommit;
		for (QString parent : newcommit.parents) {
			oldcommit.parseCommit(g, parent, true);
			diffmaplist.push_back(IndexMap());
			IndexMap &map = diffmaplist.front();
			for (CommitData const &cd : oldcommit.files) {
				map.store(cd.path, cd.id);
			}
		}

		commit_into_map(g, QString(), newcommit, &diffmaplist, &diffs);
	}

	*out = std::move(diffs);

	std::sort(out->begin(), out->end(), [](Git::Diff const &left, Git::Diff const &right){
		return left.path.compare(right.path, Qt::CaseInsensitive) < 0;
	});

}

