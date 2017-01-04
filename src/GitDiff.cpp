#include "GitDiff.h"

#include <QDebug>
#include <QThread>
#include "MainWindow.h"

// CommitData

struct CommitData {
	enum Type {
		UNKNOWN,
		TREE,
		BLOB,
	};
	Type type = UNKNOWN;
	QString path;
	QString id;
	QString mode;

	QString to_string_() const
	{
		QString t;
		switch (type) {
		case TREE: t = "TREE"; break;
		case BLOB: t = "BLOB"; break;
		}
		return QString("CommitData:{ %1 %2 %3 %4 }").arg(t).arg(id).arg(mode).arg(path);
	}
};

// PathToIdMap

class GitDiff::LookupTable {
private:
	std::map<QString, QString> path_to_id_map;
	std::map<QString, QString> id_to_path_map;
public:

	typedef std::map<QString, QString>::const_iterator const_iterator;

	void store(QString const &path, QString const &id)
	{
		path_to_id_map[path] = id;
		id_to_path_map[id] = path;
	}

	void store(QList<CommitData> const &files)
	{
		for (CommitData const &cd : files) {
			store(cd.path, cd.id);
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

// CommitList

class GitDiff::CommitList {
public:
	QList<CommitData> files;
	QStringList parents;

	bool parseTree(GitPtr g, QString const &id, QString const &dir, bool append);
	bool parseCommit(GitPtr g, QString const &id, bool append);

	void dump_() const
	{
		qDebug() << "CommitList:";
		qDebug() << "parents: " << parents;
		for (CommitData const &d : files) {
			qDebug() << d.to_string_();
		}
	}
};

bool GitDiff::CommitList::parseTree(GitPtr g, const QString &id, const QString &dir, bool append)
{
	if (!append) {
		files.clear();
	}
	if (g && !id.isEmpty()) {
		QByteArray ba;
		if (g->cat_file(id, &ba)) { // 内容を取得
			QString s = QString::fromUtf8(ba);
			QStringList lines = misc::splitLines(s);
			for (QString const &line : lines) {
				int tab = line.indexOf('\t'); // タブより後ろにパスがある
				if (tab > 0) {
					QString stat = line.mid(0, tab); // タブの手前まで
					QStringList vals = misc::splitWords(stat); // 空白で分割
					if (vals.size() >= 3) {
						CommitData data;
						data.mode = vals[0]; // ファイルモード
						data.id = vals[2]; // id（ハッシュ値）
						QString type = vals[1]; // 種類（tree/blob）
						QString path = line.mid(tab + 1); // パス
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

bool GitDiff::CommitList::parseCommit(GitPtr g, const QString &id, bool append)
{
	if (!append) {
		files.clear();
		parents.clear();
	}
	if (g && !id.isEmpty()) {
		QString tree;
		QStringList parents;
		{
			QByteArray ba;
			if (g->cat_file(id, &ba)) {
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
		if (!tree.isEmpty()) { // サブディレクトリ
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

QString GitDiff::makeKey(Git::Diff::BLOB_AB const &ab)
{
	return  ab.a.id + ".." + ab.b.id;

}

class CommitListThread : public QThread {
private:
	struct Data {
		GitPtr g;
		QString id;
		QString dir;
	};
	Data d;
	void run()
	{
		commit.parseTree(d.g, d.id, d.dir, false);
	}
public:
	GitDiff::CommitList commit;
	CommitListThread(GitPtr g, const QString &id, const QString &dir)
	{
		d.g = g;
		d.id = id;
		d.dir = dir;
	}
};

void GitDiff::diff_tree_(GitPtr g, const QString &dir, QString older_id, QString newer_id, QList<Git::Diff> *diffs)
{
	CommitListThread older(g->dup(), older_id, dir);
	CommitListThread newer(g->dup(), newer_id, dir);
	older.start();
	newer.start();
	older.wait();
	newer.wait();

	MapList diffmap;

	diffmap.push_back(LookupTable());
	LookupTable &map = diffmap.front();
	for (CommitData const &cd : older.commit.files) {
		map.store(cd.path, cd.id);
	}

	commit_into_map(g, dir, newer.commit, &diffmap, diffs); // recursive
}

void GitDiff::AddItem(Git::Diff *item, QList<Git::Diff> *diffs)
{
	item->diff = QString("diff --git a/%1 b/%2").arg(item->path).arg(item->path);
	item->index = QString("index %1..%2 %3").arg(item->blob.a.id).arg(item->blob.b.id).arg(item->mode);
	diffs->push_back(*item);
}

void GitDiff::commit_into_map(GitPtr g, const QString &dir, const GitDiff::CommitList &commit, const GitDiff::MapList *diffmap, QList<Git::Diff> *diffs)
{
	for (CommitData const &cd : commit.files) {
		bool found = false;
		for (LookupTable const &map : *diffmap) { // map（新しいコミット）の中を探す
			auto it = map.find_path(cd.path);
			if (it != map.end_path()) {
				found = true; // 見つかった
				if (cd.id != it->second) { // 変更されている
					if (cd.type == CommitData::TREE) { // ディレクトリ
						QString path = misc::joinWithSlash(dir, cd.path); // 子ディレクトリ名
						QString older_id = it->second;
						QString newer_id = cd.id;
						diff_tree_(g, path, older_id, newer_id, diffs); // 子ディレクトリを探索
					} else if (cd.type == CommitData::BLOB) { // ファイル
						Git::Diff item;
						item.path = cd.path;
						item.mode = cd.mode;
						item.blob.a.id = it->second;
						item.blob.b.id = cd.id;
						AddItem(&item, diffs);
					}
					break;
				}
			}
		}
		if (!found) { // 新しく追加されたファイル
			Git::Diff item;
			item.path = cd.path;
			item.mode = cd.mode;
			item.blob.b.id = cd.id;
			AddItem(&item, diffs);
		}
	}
}

void GitDiff::parse_tree(GitPtr g, const QString &dir, const QString &id, std::set<QString> *dirset, GitDiff::MapList *path_to_id_map)
{
	if (!dir.isEmpty()) {
		auto it = dirset->find(dir);
		if (it != dirset->end()) {
			return;
		}
		dirset->insert(dir);
	}

	CommitList commit;
	commit.parseTree(g, id, dir, false);
	path_to_id_map->push_back(LookupTable());
	LookupTable &map = path_to_id_map->front();
	for (CommitData const &cd : commit.files) {
		map.store(cd.path, cd.id);
	}
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

void GitDiff::parseDiff(QString const &s, Git::Diff const *ref, Git::Diff *out)
{
	QStringList lines = misc::splitLines(s);

	out->diff = QString("diff --git ") + ("a/" + ref->path) + ' ' + ("b/" + ref->path);
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

void GitDiff::diff(GitPtr g, QString id, QList<Git::Diff> *out)
{
	if (!g) return;

	out->clear();

	auto MakeDiffMapList = [](GitPtr g, QStringList const &parents, MapList *path_to_id_map){
		for (QString parent : parents) {
			CommitList list;
			list.parseCommit(g, parent, false);
			path_to_id_map->push_back(LookupTable());
			LookupTable &map = path_to_id_map->front();
			for (CommitData const &cd : list.files) {
				map.store(cd.path, cd.id);
			}
		}
	};

	if (id == "HEAD") {

		QString parent = g->rev_parse_HEAD(); // HEADのインデックスを取得
		QStringList parents;
		parents.push_back(parent);


		MapList path_to_id_map;
		MakeDiffMapList(g, parents, &path_to_id_map);

		std::set<QString> dirset;

		Git::FileStatusList stats = g->status(); // git status

		for (Git::FileStatus const &s : stats) {
			auto DoIt = [&](QString const &file){
				QStringList list = file.split('/');
				QString path;
				for (int i = 0; i + 1 < list.size(); i++) {
					QString const &s = list[i];
					path = misc::joinWithSlash(path, s);
					for (LookupTable const &map : path_to_id_map) {
						auto it = map.find_path(path);
						if (it != map.end_path()) {
							QString id = it->second;
							parse_tree(g, path, id, &dirset, &path_to_id_map);
						}
					}
				}
			};
			DoIt(s.path1());
//			if (s.code() == Git::FileStatusCode::RenamedInIndex) {
//				DoIt(s.path2());
//			}
		}

//		file_into_map(stats, &diffmaplist, &diffs);
		for (Git::FileStatus const &st : stats) {
			for (LookupTable const &map : path_to_id_map) {
				auto it = map.find_path(st.path1());
				if (it != map.end_path()) {
					Git::Diff item;
					item.blob.a.id = it->second;
					item.blob.b.id = PATH_PREFIX + st.path1();
					item.path = st.path1();
					if (st.code() == Git::FileStatusCode::RenamedInIndex) {
						item.blob.b.id = PATH_PREFIX + st.path2();
						item.path = st.path2();
					} //@
					AddItem(&item, &diffs);
					break;
				}
			}
		}

	} else {

		CommitList newcommit;
		newcommit.parseCommit(g, id, false);

//		newcommit.dump_();

		MapList diffmaplist;
		MakeDiffMapList(g, newcommit.parents, &diffmaplist);

		commit_into_map(g, QString(), newcommit, &diffmaplist, &diffs);
	}

	*out = std::move(diffs);

	std::sort(out->begin(), out->end(), [](Git::Diff const &left, Git::Diff const &right){
		return left.path.compare(right.path, Qt::CaseInsensitive) < 0;
	});
}

