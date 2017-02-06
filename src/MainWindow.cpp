#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "AboutDialog.h"
#include "CheckoutBranchDialog.h"
#include "CloneDialog.h"
#include "CommitPropertyDialog.h"
#include "ConfigCredentialHelperDialog.h"
#include "FileUtil.h"
#include "Git.h"
#include "GitDiff.h"
#include "joinpath.h"
#include "LibGit2.h"
#include "main.h"
#include "MergeBranchDialog.h"
#include "misc.h"
#include "MySettings.h"
#include "NewBranchDialog.h"
#include "Terminal.h"
#include "PushDialog.h"
#include "RepositoryData.h"
#include "SelectCommandDialog.h"
#include "SettingsDialog.h"
#include "TextEditDialog.h"
#include "RepositoryPropertyDialog.h"
#include "EditTagDialog.h"
#include "DeleteTagsDialog.h"
#include "FileHistoryWindow.h"
#include "FilePropertyDialog.h"
#include <deque>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QStandardPaths>
#include <QKeyEvent>
#include <set>
#include <QProcess>
#include <QDirIterator>
#include <QThread>

#ifdef Q_OS_WIN
#include "win32/win32.h"
#endif

FileDiffWidget::DrawData::DrawData()
{
	bgcolor_text = QColor(255, 255, 255);
	bgcolor_gray = QColor(224, 224, 224);
	bgcolor_add = QColor(192, 240, 192);
	bgcolor_del = QColor(255, 224, 224);
	bgcolor_add_dark = QColor(64, 192, 64);
	bgcolor_del_dark = QColor(240, 64, 64);
}

enum {
	IndexRole = Qt::UserRole,
	FilePathRole,
	DiffIndexRole,
	HunkIndexRole,
};

enum {
	GroupItem = -1,
};

static inline bool isGroupItem(QTreeWidgetItem *item)
{
	if (item) {
		int index = item->data(0, IndexRole).toInt();
		if (index == GroupItem) {
			return true;
		}
	}
	return false;
}

static inline QString getFilePath(QListWidgetItem *item)
{
	if (!item) return QString();
	return item->data(FilePathRole).toString();
}

static inline int indexOfLog(QListWidgetItem *item)
{
	if (!item) return -1;
	return item->data(IndexRole).toInt();
}

static inline int indexOfDiff(QListWidgetItem *item)
{
	if (!item) return -1;
	return item->data(DiffIndexRole).toInt();
}

static inline int getHunkIndex(QListWidgetItem *item)
{
	if (!item) return -1;
	return item->data(HunkIndexRole).toInt();
}

struct MainWindow::Private {
	Git::Context gcx;
	ApplicationSettings appsettings;
	QString file_command;
	QList<RepositoryItem> repos;
	RepositoryItem current_repo;
	Git::Branch current_branch;
	QString head_id;
	struct Diff {
		QList<Git::Diff> result;
		std::shared_ptr<QThread> thread;
		QList<std::shared_ptr<QThread>> garbage;
	} diff;
	std::map<QString, Git::Diff> diff_cache;
	QStringList added;
	Git::CommitItemList logs;
	bool uncommited_changes = false;
	int timer_interval_ms = 0;
	int update_files_list_counter = 0;
	int interval_250ms_counter = 0;
	QImage graph_color;
	std::map<QString, QList<Git::Branch>> branch_map;
	std::map<QString, QList<Git::Tag>> tag_map;
	QString repository_filter_text;
	QPixmap digits;
	QIcon repository_icon;
	QIcon folder_icon;
	unsigned int temp_file_counter = 0;
	GitObjectCache objcache;
	QPixmap transparent_pixmap;
};

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	pv = new Private();
	ui->setupUi(this);
	ui->splitter_v->setSizes({100, 400});
	ui->splitter_h->setSizes({200, 100, 200});

	ui->widget_diff_view->bind(this);

	ui->treeWidget_repos->installEventFilter(this);
	ui->tableWidget_log->installEventFilter(this);
	ui->listWidget_staged->installEventFilter(this);
	ui->listWidget_unstaged->installEventFilter(this);

	showFileList(FilesListType::SingleList);

	pv->digits.load(":/image/digits.png");
	pv->graph_color.load(":/image/graphcolor.png");

	pv->repository_icon = QIcon(":/image/repository.png");
	pv->folder_icon = QIcon(":/image/folder.png");

	prepareLogTableWidget();

#ifdef Q_OS_WIN
	{
		QFont font;

		font = ui->label_repo_name->font();
		font.setFamily("Meiryo");
		ui->label_repo_name->setFont(font);

		font = ui->label_branch_name->font();
		font.setFamily("Meiryo");
		ui->label_branch_name->setFont(font);
	}
#endif

	SettingsDialog::loadSettings(&pv->appsettings);
	{
		MySettings s;
		s.beginGroup("Global");
		pv->gcx.git_command = s.value("GitCommand").toString();
		pv->file_command = s.value("FileCommand").toString();
		s.endGroup();
	}

#if USE_LIBGIT2
	LibGit2::init();
	//	LibGit2::test();
#endif

	connect(ui->treeWidget_repos, SIGNAL(dropped()), this, SLOT(onRepositoriesTreeDropped()));;

	QString path = getBookmarksFilePath();
	pv->repos = RepositoryBookmark::load(path);
	updateRepositoriesList();

	pv->timer_interval_ms = 20;
	pv->update_files_list_counter = 0;
	pv->interval_250ms_counter = 0;
	startTimer(pv->timer_interval_ms);
}

MainWindow::~MainWindow()
{
#if USE_LIBGIT2
	LibGit2::shutdown();
#endif
	cleanupDiffThread();
	deleteTempFiles();
	delete pv;
	delete ui;
}

QString MainWindow::getObjectID(QListWidgetItem *item)
{
	int i = indexOfDiff(item);
	if (i >= 0 && i < pv->diff.result.size()) {
		Git::Diff const &diff = pv->diff.result[i];
		return diff.blob.a_id;
	}
	return QString();
}

bool MainWindow::saveRepositoryBookmarks() const
{
	QString path = getBookmarksFilePath();
	return RepositoryBookmark::save(path, &pv->repos);
}

void MainWindow::saveRepositoryBookmark(RepositoryItem item)
{
	if (item.local_dir.isEmpty()) return;

	if (item.name.isEmpty()) {
		item.name = tr("Unnamed");
	}

	bool done = false;
	for (int i = 0; i < pv->repos.size(); i++) {
		RepositoryItem *p = &pv->repos[i];
		if (item.local_dir == p->local_dir) {
			*p = item;
			done = true;
			break;
		}
	}
	if (!done) {
		pv->repos.push_back(item);
	}
	saveRepositoryBookmarks();
	updateRepositoriesList();
}

void MainWindow::buildRepoTree(QString const &group, QTreeWidgetItem *item, QList<RepositoryItem> *repos)
{
	QString name = item->text(0);
	if (isGroupItem(item)) {
		int n = item->childCount();
		for (int i = 0; i < n; i++) {
			QTreeWidgetItem *child = item->child(i);
			QString sub = group / name;
			buildRepoTree(sub, child, repos);
		}
	} else {
		RepositoryItem const *repo = repositoryItem(item);
		if (repo) {
			RepositoryItem newrepo = *repo;
			newrepo.name = name;
			newrepo.group = group;
			item->setData(0, IndexRole, repos->size());
			repos->push_back(newrepo);
		}
	}
}

void MainWindow::refrectRepositories()
{
	QList<RepositoryItem> newrepos;
	int n = ui->treeWidget_repos->topLevelItemCount();
	for (int i = 0; i < n; i++) {
		QTreeWidgetItem *item = ui->treeWidget_repos->topLevelItem(i);
		buildRepoTree(QString(), item, &newrepos);
	}
	pv->repos = std::move(newrepos);
	saveRepositoryBookmarks();
}

void MainWindow::onRepositoriesTreeDropped()
{
	refrectRepositories();
	QTreeWidgetItem *item = ui->treeWidget_repos->currentItem();
	if (item) item->setExpanded(true);
}

const QPixmap &MainWindow::digitsPixmap() const
{
	return pv->digits;
}

int MainWindow::digitWidth() const
{
	return 5;
}

int MainWindow::digitHeight() const
{
	return 7;
}

void MainWindow::drawDigit(QPainter *pr, int x, int y, int n) const
{
	int w = digitWidth();
	int h = digitHeight();
	pr->drawPixmap(x, y, w, h, pv->digits, n * w, 0, w, h);
}

QString MainWindow::defaultWorkingDir() const
{
	return pv->appsettings.default_working_dir;
}

QColor MainWindow::color(unsigned int i)
{
	unsigned int n = pv->graph_color.width();
	if (n > 0) {
		n--;
		if (i > n) i = n;
		QRgb const *p = (QRgb const *)pv->graph_color.scanLine(0);
		return QColor(qRed(p[i]), qGreen(p[i]), qBlue(p[i]));
	}
	return Qt::black;
}

bool MainWindow::isThereUncommitedChanges() const
{
	return pv->uncommited_changes;
}

Git::CommitItemList const *MainWindow::logs() const
{
	return &pv->logs;
}

QString MainWindow::currentWorkingCopyDir() const
{
	return pv->current_repo.local_dir;
}

GitPtr MainWindow::git(QString const &dir)
{
	return dir.isEmpty() ? std::shared_ptr<Git>() : std::shared_ptr<Git>(new Git(pv->gcx, dir));
}

GitPtr MainWindow::git()
{
	return git(currentWorkingCopyDir());
}

QString MainWindow::makeRepositoryName(QString const &loc)
{
	int i = loc.lastIndexOf('/');
	int j = loc.lastIndexOf('\\');
	if (i < j) i = j;
	if (i >= 0) {
		i++;
		j = loc.size();
		if (loc.endsWith(".git")) {
			j -= 4;
		}
		return loc.mid(i, j - i);
	}
	return QString();
}

int MainWindow::repositoryIndex_(QTreeWidgetItem *item)
{
	if (item) {
		int i = item->data(0, IndexRole).toInt();
		if (i >= 0 && i < pv->repos.size()) {
			return i;
		}
	}
	return -1;
}

RepositoryItem const *MainWindow::repositoryItem(QTreeWidgetItem *item)
{
	int row = repositoryIndex_(item);
	if (row >= 0 && row < pv->repos.size()) {
		return &pv->repos[row];
	}
	return nullptr;
}

static QTreeWidgetItem *newQTreeWidgetItem()
{
	QTreeWidgetItem *item = new QTreeWidgetItem();
	item->setSizeHint(0, QSize(20, 20));
	return item;
}

QTreeWidgetItem *MainWindow::newQTreeWidgetFolderItem(QString const &name)
{
	QTreeWidgetItem *item = newQTreeWidgetItem();
	item->setText(0, name);
	item->setData(0, IndexRole, GroupItem);
	item->setIcon(0, pv->folder_icon);
	return item;
}

void MainWindow::updateRepositoriesList()
{
	QString path = getBookmarksFilePath();

	pv->repos = RepositoryBookmark::load(path);

	QString filter = pv->repository_filter_text;

	ui->treeWidget_repos->clear();

	std::map<QString, QTreeWidgetItem *> parentmap;

	for (int i = 0; i < pv->repos.size(); i++) {
		RepositoryItem const &repo = pv->repos[i];
		if (!filter.isEmpty() && repo.name.indexOf(filter, 0, Qt::CaseInsensitive) < 0) {
			continue;
		}
		QTreeWidgetItem *parent = nullptr;
		{
			QString group = repo.group;
			if (group.startsWith('/')) {
				group = group.mid(1);
			}
			auto it = parentmap.find(group);
			if (it != parentmap.end()) {
				parent = it->second;
			}
			if (!parent) {
				QStringList list = group.split('/', QString::SkipEmptyParts);
				if (list.isEmpty()) {
					list.push_back(tr("Default"));
				}
				for (QString const &name : list) {
					if (name.isEmpty()) continue;
					if (!parent) {
						auto it = parentmap.find(name);
						if (it != parentmap.end()) {
							parent = it->second;
						} else {
							parent = newQTreeWidgetFolderItem(name);
							ui->treeWidget_repos->addTopLevelItem(parent);
						}
					} else {
						QTreeWidgetItem *child = newQTreeWidgetFolderItem(name);
						parent->addChild(child);
						parent = child;
					}
					parent->setExpanded(true);
				}
				Q_ASSERT(parent);
				parentmap[group] = parent;
			}
			parent->setData(0, FilePathRole, "");
		}
		QTreeWidgetItem *child = newQTreeWidgetItem();
		child->setText(0, repo.name);
		child->setData(0, IndexRole, i);
		child->setIcon(0, pv->repository_icon);
		child->setFlags(child->flags() & ~Qt::ItemIsDropEnabled);
		parent->addChild(child);
		parent->setExpanded(true);
	}
}

void MainWindow::showFileList(FilesListType files_list_type)
{
	switch (files_list_type) {
	case FilesListType::SingleList:
		ui->stackedWidget->setCurrentWidget(ui->page_1);
		break;
	case FilesListType::SideBySide:
		ui->stackedWidget->setCurrentWidget(ui->page_2);
		break;
	}
}

void MainWindow::clearLog()
{
	pv->logs.clear();
	pv->uncommited_changes = false;
	ui->tableWidget_log->clearContents();
	ui->tableWidget_log->scrollToTop();
}

void MainWindow::clearFileList()
{
	showFileList(FilesListType::SingleList);
	ui->listWidget_unstaged->clear();
	ui->listWidget_staged->clear();
	ui->listWidget_files->clear();
}

void MainWindow::clearDiffView()
{
	ui->widget_diff_view->clearDiffView();
}

void MainWindow::clearRepositoryInfo()
{
	pv->head_id = QString();
	pv->current_branch = Git::Branch();
	ui->label_repo_name->setText(QString());
	ui->label_branch_name->setText(QString());
}

class DiffThread : public QThread {
private:
	struct Data {
		GitPtr g;
		GitObjectCache *objcache;
		QString id;
		bool uncommited;
		QList<Git::Diff> result;
	} d;
public:
	DiffThread(GitPtr g, GitObjectCache *objcache, QString const &id, bool uncommited)
	{
		d.g = g;
		d.objcache = objcache;
		d.id = id;
		d.uncommited = uncommited;
	}
	void run()
	{
		GitDiff dm(d.g, d.objcache);
		if (d.uncommited) {
			dm.diff_uncommited(&d.result);
		} else {
			dm.diff(d.id, &d.result);
		}
	}
	void interrupt()
	{
		requestInterruption();
	}
	QString id() const
	{
		return d.id;
	}
	void take(QList<Git::Diff> *out)
	{
		*out = std::move(d.result);
	}
};

void MainWindow::cleanupDiffThread()
{ // 全てのスレッドが終了するまで待つ
	if (pv->diff.thread) {
		pv->diff.thread->requestInterruption();
		pv->diff.garbage.push_back(pv->diff.thread);
		pv->diff.thread.reset();
	}
	for (auto ptr : pv->diff.garbage) {
		if (ptr) {
			ptr->wait();
		}
	}
	pv->diff.garbage.clear();
}

void MainWindow::stopDiff()
{
	if (pv->diff.thread) {
		pv->diff.thread->requestInterruption(); // 停止要求
		pv->diff.garbage.push_back(pv->diff.thread); // ゴミリストに投入
		pv->diff.thread.reset(); // ポインタをクリア
	}

	// 終了したスレッドをリストから除外する
	QList<std::shared_ptr<QThread>> garbage;
	for (auto ptr : pv->diff.garbage) {
		if (ptr && ptr->isRunning()) {
			garbage.push_back(ptr); // 実行中
		}
	}
	pv->diff.garbage = std::move(garbage); // リストを差し替える
}

void MainWindow::startDiff(GitPtr g, QString id)
{ // diffスレッドを開始する
	if (!isValidWorkingCopy(g)) return;

	stopDiff();

	Git::FileStatusList list = g->status();
	pv->uncommited_changes = !list.empty();

	if (id.isEmpty()) {
		if (isThereUncommitedChanges()) {
			id = QString(); // uncommited changes
		} else {
			id = g->rev_parse_HEAD();
		}
	}

	bool uncommited = (id.isEmpty() && isThereUncommitedChanges());

	DiffThread *th = new DiffThread(g->dup(), &pv->objcache, id, uncommited);
	pv->diff.thread = std::shared_ptr<QThread>(th);
	th->start();
}

bool MainWindow::makeDiff(QString const &id, QList<Git::Diff> *out, bool uncommited)
{ // diffリストを取得する
#if SINGLE_THREAD
	GitPtr g = git();
	if (isValidWorkingCopy(g)) {
		GitDiff dm(g, &pv->objcache);
		if (dm.diff(id, out, uncommited)) {
			return true;
		}
	}
#else // multi thread
	if (pv->diff.thread) {
		DiffThread *th = dynamic_cast<DiffThread *>(pv->diff.thread.get());
		Q_ASSERT(th);
		if (id == th->id() && !th->isInterruptionRequested()) { // IDが一致して中断されていない
			th->wait(); // 終了まで待つ
			th->take(out); // 結果を取得
			return true; // success
		}
	}
#endif
	qDebug() << "failed to makeDiff";
	return false;
}

void MainWindow::updateFilesList(QString id)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	clearFileList();

	Git::FileStatusList stats = g->status();
	pv->uncommited_changes = !stats.empty();

	FilesListType files_list_type = FilesListType::SingleList;

	auto AddItem = [&](QString const &filename, QString header, int idiff, bool staged){
		if (header.isEmpty()) {
			header = "(??\?) "; // damn trigraph
		}
		QListWidgetItem *item = new QListWidgetItem(header + filename);
		item->setData(FilePathRole, filename);
		item->setData(DiffIndexRole, idiff);
		item->setData(HunkIndexRole, -1);
		switch (files_list_type) {
		case FilesListType::SingleList:
			ui->listWidget_files->addItem(item);
			break;
		case FilesListType::SideBySide:
			if (staged) {
				ui->listWidget_staged->addItem(item);
			} else {
				ui->listWidget_unstaged->addItem(item);
			}
			break;
		}
	};

	if (id.isEmpty()) {
		bool uncommited = isThereUncommitedChanges();
		if (uncommited) {
			files_list_type = FilesListType::SideBySide;
		}
		if (!makeDiff(id, &pv->diff.result, uncommited)) {
			return;
		}

		std::map<QString, int> diffmap;

		for (int idiff = 0; idiff < pv->diff.result.size(); idiff++) {
			Git::Diff const &diff = pv->diff.result[idiff];
			QString filename = diff.path;
			if (!filename.isEmpty()) {
				diffmap[filename] = idiff;
			}
		}

		showFileList(files_list_type);

		for (Git::FileStatus const &s : stats) {
			bool staged = (s.isStaged() && s.code_y() == ' ');
			int idiff = -1;
			QString header;
			auto it = diffmap.find(s.path1());
			if (it != diffmap.end()) {
				idiff = it->second;
			}
			if (s.code() == Git::FileStatusCode::Unknown) {
				qDebug() << "something wrong...";
			} else if (s.code() == Git::FileStatusCode::Untracked) {
				// nop
			} else if (s.code() == Git::FileStatusCode::AddedToIndex) {
				header = "(add) ";
			} else if (s.code_x() == 'D' || s.code_y() == 'D' || s.code() == Git::FileStatusCode::DeletedFromIndex) {
				header = "(del) ";
			} else if (s.code_x() == 'R' || s.code() == Git::FileStatusCode::RenamedInIndex) {
				header = "(ren) ";
			} else if (s.code_x() == 'M' || s.code_y() == 'M') {
				header = "(chg) ";
			}
			AddItem(s.path1(), header, idiff, staged);
		}
	} else {
		if (!makeDiff(id, &pv->diff.result, false)) {
			return;
		}

		showFileList(files_list_type);

		for (int idiff = 0; idiff < pv->diff.result.size(); idiff++) {
			Git::Diff const &diff = pv->diff.result[idiff];
			QString header;

			switch (diff.type) {
			case Git::Diff::Type::Added:   header = "(add) "; break;
			case Git::Diff::Type::Deleted: header = "(del) "; break;
			case Git::Diff::Type::Changed: header = "(chg) "; break;
			case Git::Diff::Type::Renamed: header = "(ren) "; break;
			}

			AddItem(diff.path, header, idiff, false);
		}
	}

	for (Git::Diff const &diff : pv->diff.result) {
		QString key = GitDiff::makeKey(diff);
		pv->diff_cache[key] = diff;
	}
}

void MainWindow::updateCurrentFilesList()
{
	QTableWidgetItem *item = ui->tableWidget_log->item(selectedLogIndex(), 0);
	if (!item) return;
	int row = item->data(IndexRole).toInt();
	int logs = (int)pv->logs.size();
	if (row < logs) {
		if (Git::isUncommited(pv->logs[row])) {
			updateFilesList(QString());
		} else {
			QString id = pv->logs[row].commit_id;
			updateFilesList(id);
		}
	}
}

void MainWindow::updateHeadFilesList(bool wait)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	startDiff(g, QString()); // diffスレッドを開始

	if (wait) updateCurrentFilesList(); // スレッド終了を待ってリストを更新
}

void MainWindow::prepareLogTableWidget()
{
	QStringList cols = {
		tr("Graph"),
		tr("Commit"),
		tr("Date"),
		tr("Author"),
		tr("Description"),
	};
	int n = cols.size();
	ui->tableWidget_log->setColumnCount(n);
	ui->tableWidget_log->setRowCount(0);
	for (int i = 0; i < n; i++) {
		QString const &text = cols[i];
		QTableWidgetItem *item = new QTableWidgetItem(text);
		ui->tableWidget_log->setHorizontalHeaderItem(i, item);
	}

	updateCommitGraph(); // コミットグラフを更新
}

QString MainWindow::currentRepositoryName() const
{
	return pv->current_repo.name;
}

Git::Branch MainWindow::currentBranch() const
{
	return pv->current_branch;
}

bool MainWindow::isValidWorkingCopy(GitPtr const &g) const
{
	return g && g->isValidWorkingCopy();
}

int MainWindow::limitLogCount() const
{
	return 10000;
}

QDateTime MainWindow::limitLogTime() const
{
	QDateTime t = QDateTime::currentDateTime();
	t = t.addYears(-3);
	return t;
}

void MainWindow::queryBranches(GitPtr g)
{
	Q_ASSERT(g);
	pv->branch_map.clear();
	QList<Git::Branch> branches = g->branches();
	for (Git::Branch const &b : branches) {
		if (b.flags & Git::Branch::Current) {
			pv->current_branch = b;
		}
		pv->branch_map[b.id].append(b);
	}
}

void MainWindow::queryTags(GitPtr g)
{
	Q_ASSERT(g);
	pv->tag_map.clear();
	QList<Git::Tag> tags = g->tags();
	for (Git::Tag const &t : tags) {
		pv->tag_map[t.id].push_back(t);
	}
}

QString MainWindow::abbrevCommitID(Git::CommitItem const &commit)
{
	return commit.commit_id.mid(0, 7);
}

QString MainWindow::findFileID(GitPtr g, const QString &commit_id, const QString &file)
{
	return lookupFileID(&pv->objcache, commit_id, file);
}

void MainWindow::openRepository_(GitPtr g)
{
	clearLog();
	clearRepositoryInfo();

	pv->objcache.setup(g);

	if (isValidWorkingCopy(g)) {
		startDiff(g, QString());
		updateFilesList(QString());

		pv->head_id = g->rev_parse_HEAD();

		// ログを取得
		pv->logs = g->log(limitLogCount(), limitLogTime());

		// ブランチとタグを取得
		queryBranches(g);
		queryTags(g);

		QString branch_name = currentBranch().name;
		if (currentBranch().flags & Git::Branch::HeadDetached) {
			branch_name += " (HEAD detached)";
		}

		ui->label_repo_name->setText(currentRepositoryName());
		ui->label_branch_name->setText(branch_name);
	} else {
		QString name = currentRepositoryName();
		ui->label_repo_name->setText(name.isEmpty() ? tr("Unknown") : name);
		ui->label_branch_name->setText("NOT FOUND");
	}

	if (isThereUncommitedChanges()) {
		Git::CommitItem item;
		item.parent_ids.push_back(pv->current_branch.id);
		item.message = tr("Uncommited changes");
		pv->logs.insert(pv->logs.begin(), item);
	}

	prepareLogTableWidget();

	int index = 0;
	int count = pv->logs.size();

	ui->tableWidget_log->setRowCount(count);

	for (int row = 0; row < count; row++) {
		Git::CommitItem const *commit = &pv->logs[index];
		{
			QTableWidgetItem *item = new QTableWidgetItem();
			item->setData(IndexRole, index);
			ui->tableWidget_log->setItem(row, 0, item);
		}
		int col = 1; // カラム0はコミットグラフなので、その次から
		auto AddColumn = [&](QString const &text, bool bold){
			QTableWidgetItem *item = new QTableWidgetItem(text);
			if (bold) {
				QFont font = item->font();
				font.setBold(true);
				item->setFont(font);
			}
			ui->tableWidget_log->setItem(row, col, item);
			col++;
		};
		QString commit_id;
		QString datetime;
		QString author;
		QString message;
		bool ishead = commit->commit_id == pv->head_id;
		bool bold = false;
		{
			if (Git::isUncommited(*commit)) { // 未コミットの時
				bold = true; // 太字
			} else {
				if (ishead && !isThereUncommitedChanges()) { // HEADで、未コミットがないとき
					bold = true; // 太字
				}
				commit_id = abbrevCommitID(*commit);
			}
			datetime = misc::makeDateTimeString(commit->commit_date);
			author = commit->author;
			message = commit->message;

			{ // branch
				QList<Git::Branch> list = findBranch(commit->commit_id);
				auto AbbrevName = [](QString const &name){
					QStringList sl = name.split('/');
					if (sl.size() == 1) return sl[0];
					QString newname;
					for (int i = 0; i < sl.size(); i++) {
						QString s = sl[i];
						if (i + 1 < sl.size()) {
							s = s.mid(0, 1);
						}
						if (i > 0) {
							newname += '/';
						}
						newname += s;
					}
					return newname;
				};
				for (Git::Branch const &b : list) {
					message += " {";
					message += AbbrevName(b.name);
					if (b.ahead > 0) {
						message += tr(", %1 ahead").arg(b.ahead);
					}
					if (b.behind > 0) {
						message += tr(", %1 behind").arg(b.behind);
					}
					message += '}';
				}
			}
			{ // tag
				QList<Git::Tag> list = findTag(commit->commit_id);
				for (Git::Tag const &t : list) {
					message += QString(" {t:%1}").arg(t.name);
				}
			}
		}
		if (false) { // メッセージ欄に、親コミットIDを表示する（デバッグ用）
			message.clear();
			if (true) {
				message += QString::number(row);
			} else {
				for (QString id : commit->parent_ids) {
					if (!message.isEmpty()) {
						message += ' ';
					}
					message += id;
				}
			}
		}
		AddColumn(commit_id, false);
		AddColumn(datetime, false);
		AddColumn(author, false);
		AddColumn(message, bold);
		ui->tableWidget_log->setRowHeight(row, 24);
		index++;
	}
	ui->tableWidget_log->resizeColumnsToContents();
	ui->tableWidget_log->horizontalHeader()->setStretchLastSection(false);
	ui->tableWidget_log->horizontalHeader()->setStretchLastSection(true);

	ui->tableWidget_log->setFocus();
	ui->tableWidget_log->setCurrentCell(0, 0);

	udpateButton();
}

void MainWindow::openRepository(bool waitcursor)
{
	if (waitcursor) {
		OverrideWaitCursor;
		openRepository(false);
		return;
	}

	GitPtr g = git(); // ポインタの有効性チェックはしない（nullptrでも続行）
	openRepository_(g);
}

void MainWindow::reopenRepository(std::function<void(GitPtr g)> callback)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	OverrideWaitCursor;
	callback(g);
	openRepository_(g);
}


void MainWindow::udpateButton()
{
	Git::Branch b = currentBranch();

	int n;

	n = b.ahead > 0 ? b.ahead : -1;
	ui->toolButton_push->setNumber(n);

	n = b.behind > 0 ? b.behind : -1;
	ui->toolButton_pull->setNumber(n);
}

void MainWindow::autoOpenRepository(QString dir)
{
	dir = QDir(dir).absolutePath();

	auto Open = [&](RepositoryItem const &item){
		pv->current_repo = item;
		openRepository();
	};

	for (RepositoryItem const &item : pv->repos) {
		Qt::CaseSensitivity cs = Qt::CaseSensitive;
#ifdef Q_OS_WIN
		cs = Qt::CaseInsensitive;
#endif
		if (dir.compare(item.local_dir, cs) == 0) {
			Open(item);
			return;
		}
	}

	RepositoryItem newitem;
	GitPtr g = git(dir);
	if (isValidWorkingCopy(g)) {
		ushort const *left = dir.utf16();
		ushort const *right = left + dir.size();
		if (right[-1] == '/' || right[-1] == '\\') {
			right--;
		}
		ushort const *p = right;
		while (left + 1 < p && !(p[-1] == '/' || p[-1] == '\\')) p--;
		if (p < right) {
			newitem.local_dir = dir;
			newitem.name = QString::fromUtf16(p, right - p);
			saveRepositoryBookmark(newitem);
			Open(newitem);
			return;
		}
	}
}

void MainWindow::openSelectedRepository()
{
	QTreeWidgetItem *ite = ui->treeWidget_repos->currentItem();
	RepositoryItem const *item = repositoryItem(ite);
	if (item) {
		pv->current_repo = *item;
		openRepository();
	}
}



QString MainWindow::getBookmarksFilePath() const
{
	return application_data_dir / "bookmarks.xml";
}

void MainWindow::on_action_add_all_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	g->git("add --all");
	updateHeadFilesList(true);
}

void MainWindow::commit(bool amend)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	while (1) {
		TextEditDialog dlg;
		dlg.setWindowTitle(tr("Commit"));
		if (amend) {
			dlg.setText(pv->logs[0].message);
		}
		if (dlg.exec() == QDialog::Accepted) {
			QString text = dlg.text();
			if (text.isEmpty()) {
				QMessageBox::warning(this, tr("Commit"), tr("Commit message can not be omitted."));
				continue;
			}
			if (amend) {
				g->commit_amend_m(text);
			} else {
				g->commit(text);
			}
			openRepository();
			break;
		} else {
			break;
		}
	}
}

void MainWindow::commit_amend()
{
	commit(true);
}


void MainWindow::on_action_commit_triggered()
{
	commit();
}

void MainWindow::on_action_fetch_triggered()
{
	reopenRepository([](GitPtr g){
		g->fetch();
	});
}

void MainWindow::on_action_push_triggered()
{
	reopenRepository([](GitPtr g){
		g->push();
	});
}

void MainWindow::on_action_pull_triggered()
{
	reopenRepository([](GitPtr g){
		g->pull();
	});
}

void MainWindow::on_toolButton_push_clicked()
{
	ui->action_push->trigger();
}

void MainWindow::on_toolButton_pull_clicked()
{
	ui->action_pull->trigger();
}

void MainWindow::on_action_set_remote_origin_url_triggered()
{
	QString newurl;
	Git git(pv->gcx, currentWorkingCopyDir());
	git.git("remote set-url origin " + newurl);
}

void MainWindow::on_action_config_global_credential_helper_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	g->git("config --global credential.helper");
	QString text = g->resultText().trimmed();
	ConfigCredentialHelperDialog dlg(this);
	dlg.setHelper(text);
	if (dlg.exec() == QDialog::Accepted) {
		text = dlg.helper();
		bool ok = true;
		if (text.isEmpty()) {
			text = "\"\"";
		} else {
			auto isValidChar = [](ushort c){
				if (QChar::isLetterOrNumber(c)) return true;
				if (c == '_') return true;
				return false;
			};
			ushort const *begin = text.utf16();
			ushort const *end = begin + text.size();
			ushort const *ptr = begin;
			while (ptr < end) {
				if (!isValidChar(*ptr)) {
					ok = false;
					break;
				}
				ptr++;
			}
		}
		if (!ok) {
			QMessageBox::warning(this, tr("Config credential helper"), tr("Invalid credential helper name"));
			return;
		}
		g->git("config --global credential.helper " + text);
	}
}

void MainWindow::on_treeWidget_repos_itemDoubleClicked(QTreeWidgetItem * /*item*/, int /*column*/)
{
	openSelectedRepository();
}

void MainWindow::execCommitPropertyDialog(Git::CommitItem const *commit)
{
	CommitPropertyDialog dlg(this, *commit);
	dlg.exec();
}

QAction *MainWindow::addMenuActionProperties(QMenu *menu)
{
	return menu->addAction(tr("&Properties"));
}

void MainWindow::on_treeWidget_repos_customContextMenuRequested(const QPoint &pos)
{
	QTreeWidgetItem *treeitem = ui->treeWidget_repos->currentItem();
	RepositoryItem const *repo = repositoryItem(treeitem);
	int index = treeitem->data(0, IndexRole).toInt();
	if (isGroupItem(treeitem)) { // group item
		QMenu menu;
		QAction *a_add_new_group = menu.addAction(tr("&Add new group"));
		QAction *a_delete_group = menu.addAction(tr("&Delete group"));
		QPoint pt = ui->treeWidget_repos->mapToGlobal(pos);
		QAction *a = menu.exec(pt + QPoint(8, -8));
		if (a) {
			if (a == a_add_new_group) {
				QTreeWidgetItem *child = newQTreeWidgetFolderItem(tr("New folder"));
				treeitem->addChild(child);
				child->setFlags(child->flags() | Qt::ItemIsEditable);
				ui->treeWidget_repos->setCurrentItem(child);
				return;
			}
			if (a == a_delete_group) {
				QTreeWidgetItem *parent = treeitem->parent();
				if (parent) {
					int i = parent->indexOfChild(treeitem);
					delete parent->takeChild(i);
				} else {
					int i = ui->treeWidget_repos->indexOfTopLevelItem(treeitem);
					delete ui->treeWidget_repos->takeTopLevelItem(i);
				}
				refrectRepositories();
				return;
			}
		}
	} else if (repo) { // repository item
		QString open_terminal = tr("Open &terminal");
		QString open_commandprompt = tr("Open command promp&t");
		QMenu menu;
		QAction *a_open = menu.addAction(tr("&Open"));
		menu.addSeparator();
#ifdef Q_OS_WIN
		QAction *a_open_terminal = menu.addAction(open_commandprompt);
		(void)open_terminal;
#else
		QAction *a_open_terminal = menu.addAction(open_terminal);
		(void)open_commandprompt;
#endif
		QAction *a_open_folder = menu.addAction(tr("Open &folder"));
		menu.addSeparator();
		QAction *a_remove = menu.addAction(tr("&Remove"));
		menu.addSeparator();
		QAction *a_properties = addMenuActionProperties(&menu);

		QPoint pt = ui->treeWidget_repos->mapToGlobal(pos);
		QAction *a = menu.exec(pt + QPoint(8, -8));
		if (a) {
			if (a == a_open) {
				openSelectedRepository();
				return;
			}
			if (a == a_open_folder) {
				QDesktopServices::openUrl(repo->local_dir);
				return;
			}
			if (a == a_open_terminal) {
				Terminal::open(repo->local_dir);
				return;
			}
			if (a == a_remove) {
				pv->repos.erase(pv->repos.begin() + index);
				saveRepositoryBookmarks();
				updateRepositoriesList();
				return;
			}
			if (a == a_properties) {
				RepositoryPropertyDialog dlg(this, *repo);
				dlg.exec();
				return;
			}
		}
	}
}

void MainWindow::on_tableWidget_log_customContextMenuRequested(const QPoint &pos)
{
	Git::CommitItem const *commit = selectedCommitItem();
	if (commit) {
		int row = selectedLogIndex();
		QMenu menu;
		QAction *a_properties = addMenuActionProperties(&menu);
		QAction *a_edit_comment = nullptr;
		if (row == 0 && currentBranch().ahead > 0) {
			a_edit_comment = menu.addAction(tr("Edit comment..."));
		}
		QAction *a_add_tag = menu.addAction(tr("Add a tag..."));
		QAction *a_delete_tags = nullptr;
		if (pv->tag_map.find(commit->commit_id) != pv->tag_map.end()) {
			a_delete_tags = menu.addAction(tr("Delete tags..."));
		}
		QAction *a = menu.exec(ui->tableWidget_log->viewport()->mapToGlobal(pos) + QPoint(8, -8));
		if (a) {
			if (a == a_properties) {
				execCommitPropertyDialog(commit);
				return;
			}
			if (a == a_edit_comment) {
				commit_amend();
				return;
			}
			if (a == a_add_tag) {
				addTag();
				return;
			}
			if (a == a_delete_tags) {
				deleteSelectedTags();
				return;
			}
		}
	}
}

void MainWindow::on_listWidget_files_customContextMenuRequested(const QPoint &pos)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QMenu menu;
	QAction *a_history = menu.addAction(tr("History"));
	QAction *a_properties = addMenuActionProperties(&menu);

	QPoint pt = ui->listWidget_unstaged->mapToGlobal(pos) + QPoint(8, -8);
	QAction *a = menu.exec(pt);
	if (a) {
		QListWidgetItem *item = ui->listWidget_files->currentItem();
		if (a == a_history) {
			execFileHistory(item);
		} else if (a == a_properties) {
			execFilePropertyDialog(item);
		}
	}
}

void MainWindow::on_listWidget_unstaged_customContextMenuRequested(const QPoint &pos)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QList<QListWidgetItem *> items = ui->listWidget_unstaged->selectedItems();
	if (!items.isEmpty()) {
		QMenu menu;
		QAction *a_stage = menu.addAction(tr("Stage"));
		QAction *a_revert = menu.addAction(tr("Revert"));
		QAction *a_ignore = menu.addAction(tr("Ignore"));
		QAction *a_delete = menu.addAction(tr("Delete"));
		QAction *a_history = menu.addAction(tr("History"));
		QAction *a_properties = addMenuActionProperties(&menu);
		QPoint pt = ui->listWidget_unstaged->mapToGlobal(pos) + QPoint(8, -8);
		QAction *a = menu.exec(pt);
		if (a) {
			QListWidgetItem *item = ui->listWidget_unstaged->currentItem();
			if (a == a_stage) {
				for_each_selected_unstaged_files([&](QString const &path){
					g->stage(path);
				});
				updateHeadFilesList(true);
			} else if (a == a_revert) {
				QStringList paths;
				for_each_selected_unstaged_files([&](QString const &path){
					paths.push_back(path);
				});
				revertFile(paths);
			} else if (a == a_ignore) {
				QString append;
				for_each_selected_unstaged_files([&](QString const &path){
					if (path == ".gitignore") {
						// skip
					} else {
						append += path + '\n';
					}
				});
				QString gitignore_path = currentWorkingCopyDir() / ".gitignore";
				if (TextEditDialog::editFile(this, gitignore_path, ".gitignore", append)) {
					updateHeadFilesList(true);
				}
			} else if (a == a_delete) {
				if (askAreYouSureYouWantToRun("Delete", "Delete selected files.")) {
					for_each_selected_unstaged_files([&](QString const &path){
						g->removeFile(path);
						g->chdirexec([&](){
							QFile(path).remove();
							return true;
						});
					});
					openRepository();
				}
			} else if (a == a_history) {
				execFileHistory(item);
			} else if (a == a_properties) {
				execFilePropertyDialog(item);
			}
		}
	}
}

void MainWindow::on_listWidget_staged_customContextMenuRequested(const QPoint &pos)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QListWidgetItem *item = ui->listWidget_staged->currentItem();
	if (item) {
		QString path = getFilePath(item);
		QString fullpath = currentWorkingCopyDir() / path;
		if (QFileInfo(fullpath).isFile()) {
			QMenu menu;
			QAction *a_unstage = menu.addAction(tr("Unstage"));
			QAction *a_history = menu.addAction(tr("History"));
			QAction *a_properties = addMenuActionProperties(&menu);
			QPoint pt = ui->listWidget_staged->mapToGlobal(pos) + QPoint(8, -8);
			QAction *a = menu.exec(pt);
			if (a) {
				QListWidgetItem *item = ui->listWidget_unstaged->currentItem();
				if (a == a_unstage) {
					g->unstage(path);
					updateHeadFilesList(true);
				} else if (a == a_history) {
					execFileHistory(item);
				} else if (a == a_properties) {
					execFilePropertyDialog(item);
				}
			}
		}
	}
}

bool MainWindow::askAreYouSureYouWantToRun(QString const &title, QString const &command)
{
	QString message = tr("Are you sure you want to run the following command ?");
	QString text = "%1\n\n%2";
	text = text.arg(message).arg(command);
	return QMessageBox::warning(this, title, text, QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok;
}

void MainWindow::revertFile(QStringList const &paths)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	if (paths.isEmpty()) {
		// nop
	} else {
		QString cmd = "git checkout -- \"" + paths[0] + '\"';
		if (askAreYouSureYouWantToRun(tr("Revert a file"), "> " + cmd)) {
			for (QString const &path : paths) {
				g->revertFile(path);
			}
			openRepository();
		}
	}
}

void MainWindow::revertAllFiles()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QString cmd = "git reset --hard HEAD";
	if (askAreYouSureYouWantToRun(tr("Revert all files"), "> " + cmd)) {
		g->revertAllFiles();
		openRepository();
	}
}

QStringList MainWindow::selectedStagedFiles() const
{
	QStringList list;
	QList<QListWidgetItem *> items = ui->listWidget_staged->selectedItems();
	for (QListWidgetItem *item : items) {
		QString path = getFilePath(item);
		list.push_back(path);
	}
	return list;
}


QStringList MainWindow::selectedUnstagedFiles() const
{
	QStringList list;
	QList<QListWidgetItem *> items = ui->listWidget_unstaged->selectedItems();
	for (QListWidgetItem *item : items) {
		QString path = getFilePath(item);
		list.push_back(path);
	}
	return list;
}

void MainWindow::for_each_selected_staged_files(std::function<void(QString const&)> fn)
{
	for (QString path : selectedStagedFiles()) {
		fn(path);
	}
}

void MainWindow::for_each_selected_unstaged_files(std::function<void(QString const&)> fn)
{
	for (QString path : selectedUnstagedFiles()) {
		fn(path);
	}
}

void MainWindow::execFileHistory(QString const &path)
{
	if (path.isEmpty()) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	FileHistoryWindow dlg(this);
	dlg.prepare(g, path);
	dlg.exec();
}

void MainWindow::execFileHistory(QListWidgetItem *item)
{
	if (item) {
		QString path = getFilePath(item);
		if (!path.isEmpty()) {
			execFileHistory(path);
		}
	}
}







void MainWindow::on_action_open_existing_working_copy_triggered()
{
	QString dir = defaultWorkingDir();
	dir = QFileDialog::getExistingDirectory(this, tr("Add existing working copy"), dir);

	RepositoryItem item;
	item.local_dir = dir;
	item.name = makeRepositoryName(dir);
	saveRepositoryBookmark(item);
}

void MainWindow::on_action_view_refresh_triggered()
{
	openRepository();
}

void MainWindow::on_tableWidget_log_currentItemChanged(QTableWidgetItem * /*current*/, QTableWidgetItem * /*previous*/)
{
	stopDiff();

	clearFileList();

	QTableWidgetItem *item = ui->tableWidget_log->item(selectedLogIndex(), 0);
	if (!item) return;

	int row = item->data(IndexRole).toInt();
	if (row < (int)pv->logs.size()) {
		Git::CommitItem const &commit = pv->logs[row];
		if (Git::isUncommited(commit)) {
			updateHeadFilesList(false);
		} else {
			GitPtr g = git();
			if (isValidWorkingCopy(g)) {
				QString id = commit.commit_id;
				startDiff(g, id);
			}
		}
		pv->update_files_list_counter = 200;
	}
}



void MainWindow::on_toolButton_stage_clicked()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	g->stage(selectedUnstagedFiles());
	updateHeadFilesList(true);
}

void MainWindow::on_toolButton_unstage_clicked()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	g->unstage(selectedStagedFiles());
	updateHeadFilesList(true);
}

void MainWindow::on_toolButton_select_all_clicked()
{
	if (ui->listWidget_unstaged->count() > 0) {
		ui->listWidget_unstaged->setFocus();
		ui->listWidget_unstaged->selectAll();
	} else if (ui->listWidget_staged->count() > 0) {
		ui->listWidget_staged->setFocus();
		ui->listWidget_staged->selectAll();
	}
}

void MainWindow::on_toolButton_commit_clicked()
{
	ui->action_commit->trigger();
}

bool MainWindow::editFile(const QString &path, QString const &title)
{
	return TextEditDialog::editFile(this, path, title);
}

struct Task {
	int index = 0;
	int parent = 0;
	Task()
	{
	}
	Task(int index, int parent)
		: index(index)
		, parent(parent)
	{
	}
};

struct Element {
	int depth = 0;
	std::vector<int> indexes;
};

void MainWindow::updateCommitGraph()
{
	const size_t LogCount = pv->logs.size();
	// 樹形図情報を構築する
	if (LogCount > 0) {
		auto LogItem = [&](size_t i)->Git::CommitItem &{ return pv->logs[i]; };
		enum { // 有向グラフを構築するあいだ CommitItem::marker_depth をフラグとして使用する
			UNKNOWN = 0,
			KNOWN = 1,
		};
		for (Git::CommitItem &item : pv->logs) {
			item.marker_depth = UNKNOWN;
		}
		// コミットハッシュを検索して、親コミットのインデックスを求める
		for (size_t i = 0; i < LogCount; i++) {
			Git::CommitItem *item = &LogItem(i);
			item->parent_lines.clear();
			if (item->parent_ids.empty()) {
				item->resolved = true;
			} else {
				for (int j = 0; j < item->parent_ids.size(); j++) { // 親の数だけループ
					QString const &parent_id = item->parent_ids[j]; // 親のハッシュ値
					for (size_t k = i + 1; k < LogCount; k++) { // 親を探す
						if (LogItem(k).commit_id == parent_id) { // ハッシュ値が一致したらそれが親
							item->parent_lines.push_back(k); // インデックス値を記憶
							LogItem(k).has_child = true;
							LogItem(k).marker_depth = KNOWN;
							item->resolved = true;
							break;
						}
					}
				}
			}
		}
		std::vector<Element> elements; // 線分リスト
		{ // 線分リストを作成する
			std::deque<Task> tasks; // 未処理タスクリスト
			{
				for (size_t i = 0; i < LogCount; i++) {
					Git::CommitItem *item = &LogItem(i);
					if (item->marker_depth == UNKNOWN) {
						int n = item->parent_lines.size(); // 最初のコミットアイテム
						for (int j = 0; j < n; j++) {
							tasks.push_back(Task(i, j)); // タスクを追加
						}
					}
					item->marker_depth = UNKNOWN;
				}
			}
			while (!tasks.empty()) { // タスクが残っているならループ
				Element e;
				Task task;
				{ // 最初のタスクを取り出す
					task = tasks.front();
					tasks.pop_front();
				}
				e.indexes.push_back(task.index); // 先頭のインデックスを追加
				int index = LogItem(task.index).parent_lines[task.parent].index; // 開始インデックス
				while (index > 0 && (size_t)index < LogCount) { // 最後に到達するまでループ
					e.indexes.push_back(index); // インデックスを追加
					int n = LogItem(index).parent_lines.size(); // 親の数
					if (n == 0) break; // 親がないなら終了
					Git::CommitItem *item = &LogItem(index);
					if (item->marker_depth == KNOWN) break; // 既知のアイテムに到達したら終了
					item->marker_depth = KNOWN; // 既知のアイテムにする
					for (int i = 1; i < n; i++) {
						tasks.push_back(Task(index, i)); // タスク追加
					}
					index = LogItem(index).parent_lines[0].index; // 次の親（親リストの先頭の要素）
				}
				if (e.indexes.size() >= 2) {
					elements.push_back(e);
				}
			}
		}
		// 線情報をクリア
		for (Git::CommitItem &item : pv->logs) {
			item.marker_depth = -1;
			item.parent_lines.clear();
		}
		// マークと線の深さを決める
		if (!elements.empty()) {
			{ // 優先順位を調整する
				std::sort(elements.begin(), elements.end(), [](Element const &left, Element const &right){
					int i = 0;
					{ // 長いものを優先して左へ
						int l = left.indexes.back() - left.indexes.front();
						int r = right.indexes.back() - right.indexes.front();
						i = r - l; // 降順
					}
					if (i == 0) {
						// コミットが新しいものを優先して左へ
						int l = left.indexes.front();
						int r = right.indexes.front();
						i = l - r; // 昇順
					}
					return i < 0;
				});
				// 子の無いブランチ（タグ等）が複数連続しているとき、古いコミットを右に寄せる
				{
					for (size_t i = 0; i + 1 < elements.size(); i++) {
						Element *e = &elements[i];
						int index1 = e->indexes.front();
						if (index1 > 0 && !LogItem(index1).has_child) { // 子がない
							// 新しいコミットを探す
							for (size_t j = i + 1; j < elements.size(); j++) { // 現在位置より後ろを探す
								Element *f = &elements[j];
								int index2 = f->indexes.front();
								if (index1 == index2 + 1) { // 一つだけ新しいコミット
									Element t = std::move(*f);
									elements.erase(elements.begin() + j); // 移動元を削除
									elements.insert(elements.begin() + i, std::move(t)); // 現在位置に挿入
								}
							}
							// 古いコミットを探す
							size_t j = 0;
							while (j < i) { // 現在位置より前を探す
								Element *f = &elements[j];
								int index2 = f->indexes.front();
								if (index1 + 1 == index2) { // 一つだけ古いコミット
									Element t = std::move(*f);
									elements.erase(elements.begin() + j); // 移動元を削除
									elements.insert(elements.begin() + i, std::move(t)); // 現在位置の次に挿入
									index1 = index2;
									f = e;
								} else {
									j++;
								}
							}
						}
					}
				}
			}
			{ // 最初の線は深さを0にする
				Element *e = &elements.front();
				for (size_t i = 0; i < e->indexes.size(); i++) {
					int index = e->indexes[i];
					LogItem(index).marker_depth = 0; // マークの深さを設定
					e->depth = 0; // 線の深さを設定
				}
			}
			// 最初以外の線分の深さを決める
			for (size_t i = 1; i < elements.size(); i++) { // 最初以外をループ
				Element *e = &elements[i];
				int depth = 1;
				while (1) { // 失敗したら繰り返し
					for (size_t j = 0; j < i; j++) { // 既に処理済みの線を調べる
						Element const *f = &elements[j]; // 検査対象
						if (e->indexes.size() == 2) { // 二つしかない場合
							int from = e->indexes[0]; // 始点
							int to = e->indexes[1];   // 終点
							if (LogItem(from).has_child) {
								for (size_t k = 0; k + 1 < f->indexes.size(); k++) { // 検査対象の全ての線分を調べる
									int curr = f->indexes[k];
									int next = f->indexes[k + 1];
									if (from > curr && to == next) { // 決定済みの線に直結できるか判定
										e->indexes.back() = from + 1; // 現在の一行下に直結する
										e->depth = elements[j].depth; // 接続先の深さ
										goto DONE; // 決定
									}
								}
							}
						}
						if (depth == f->depth) { // 同じ深さ
							if (e->indexes.back() > f->indexes.front() && e->indexes.front() < f->indexes.back()) { // 重なっている
								goto FAIL; // この深さには線を置けないのでやりなおし
							}
						}
					}
					for (size_t j = 0; j < e->indexes.size(); j++) {
						int index = e->indexes[j];
						Git::CommitItem *item = &LogItem(index);
						if (j == 0 && item->has_child) { // 最初のポイントで子がある場合
							// nop
						} else if ((j > 0 && j + 1 < e->indexes.size()) || item->marker_depth < 0) { // 最初と最後以外、または、未確定の場合
							item->marker_depth = depth; // マークの深さを設定
						}
					}
					e->depth = depth; // 深さを決定
					goto DONE; // 決定
FAIL:;
					depth++; // 一段深くして再挑戦
				}
DONE:;
			}
			// 線情報を生成する
			for (size_t i = 0; i < elements.size(); i++) {
				Element const &e = elements[i];
				auto ColorNumber = [&](){ return e.depth; };
				size_t count = e.indexes.size();
				for (size_t j = 0; j + 1 < count; j++) {
					int curr = e.indexes[j];
					int next = e.indexes[j + 1];
					TreeLine line(next, e.depth);
					line.color_number = ColorNumber();
					line.bend_early = (j + 2 < count || !LogItem(next).resolved);
					if (j + 2 == count) {
						if (count > 2 || !LogItem(curr).has_child) { // 直結ではない、または、子がない
							line.depth = LogItem(next).marker_depth; // 合流する先のアイテムの深さと同じにする
						}
					}
					LogItem(curr).parent_lines.push_back(line); // 線を追加
				}
			}
		}
	}
}

QList<Git::Branch> MainWindow::findBranch(QString const &id)
{
	auto it = pv->branch_map.find(id);
	if (it != pv->branch_map.end()) {
		return it->second;
	}
	return QList<Git::Branch>();
}

QList<Git::Tag> MainWindow::findTag(QString const &id)
{
	auto it = pv->tag_map.find(id);
	if (it != pv->tag_map.end()) {
		return it->second;
	}
	return QList<Git::Tag>();
}

void MainWindow::on_action_edit_global_gitconfig_triggered()
{
	QString dir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
	QString path = dir / ".gitconfig";
	editFile(path, ".gitconfig");
}

void MainWindow::on_action_edit_git_config_triggered()
{
	QString dir = currentWorkingCopyDir();
	QString path = dir / ".git/config";
	editFile(path, ".git/config");
}

void MainWindow::on_action_edit_gitignore_triggered()
{
	QString dir = currentWorkingCopyDir();
	QString path = dir / ".gitignore";
	editFile(path, ".gitignore");
}

int MainWindow::selectedLogIndex() const
{
	int i = ui->tableWidget_log->currentRow();
	if (i >= 0 && i < (int)pv->logs.size()) {
		return i;
	}
	return -1;
}

Git::CommitItem const *MainWindow::selectedCommitItem() const
{
	int i = selectedLogIndex();
	if (i >= 0 && i < (int)pv->logs.size()) {
		return &pv->logs[i];
	}
	return nullptr;
}

bool MainWindow::cat_file_(GitPtr g, QString const &id, QByteArray *out)
{
	out->clear();

	if (!isValidWorkingCopy(g)) return false;

	QString path_prefix = PATH_PREFIX;
	if (id.startsWith(path_prefix)) {
		QString path = g->workingRepositoryDir();
		path = path / id.mid(path_prefix.size());
		QFile file(path);
		if (file.open(QFile::ReadOnly)) {
			*out = file.readAll();
			file.close();
			return true;
		}
	} else if (Git::isValidID(id)) {
		*out = pv->objcache.catFile(id);
		if (!out->isEmpty()) {
			return true;
		}
	}
	return false;
}

bool MainWindow::cat_file(QString const &id, QByteArray *out)
{
	return cat_file_(git(), id, out);
}

void MainWindow::updateDiffView(QListWidgetItem *item)
{
	clearDiffView();

	int idiff = indexOfDiff(item);
	if (idiff >= 0 && idiff < pv->diff.result.size()) {
		Git::Diff const &diff = pv->diff.result[idiff];
		QString key = GitDiff::makeKey(diff);
		auto it = pv->diff_cache.find(key);
		if (it != pv->diff_cache.end()) {
			int row = ui->tableWidget_log->currentRow();
			bool uncommited = (row >= 0 && row < (int)pv->logs.size() && Git::isUncommited(pv->logs[row]));
			ui->widget_diff_view->updateDiffView(it->second, uncommited);
		}
	}
}

void MainWindow::updateUnstagedFileCurrentItem()
{
	updateDiffView(ui->listWidget_unstaged->currentItem());
}

void MainWindow::updateStagedFileCurrentItem()
{
	updateDiffView(ui->listWidget_staged->currentItem());
}

void MainWindow::on_listWidget_unstaged_currentRowChanged(int /*currentRow*/)
{
	updateUnstagedFileCurrentItem();
}

void MainWindow::on_listWidget_staged_currentRowChanged(int /*currentRow*/)
{
	updateStagedFileCurrentItem();
}

void MainWindow::on_listWidget_files_currentRowChanged(int /*currentRow*/)
{
	updateDiffView(ui->listWidget_files->currentItem());
}

void MainWindow::setGitCommand(QString const &path, bool save)
{
	if (save) {
		MySettings s;
		s.beginGroup("Global");
		s.setValue("GitCommand", path);
		s.endGroup();
	}
	pv->gcx.git_command = path;
}

void MainWindow::setFileCommand(QString const &path, bool save)
{
	if (save) {
		MySettings s;
		s.beginGroup("Global");
		s.setValue("FileCommand", path);
		s.endGroup();
	}
	pv->file_command = path;
}

QStringList MainWindow::whichCommand_(QString const &cmdfile)
{
	QStringList list;

	std::vector<std::string> vec;
	FileUtil::which(cmdfile.toStdString(), &vec);

	for (std::string const &s : vec) {
		list.push_back(QString::fromStdString(s));
	}

	return list;
}

QString MainWindow::selectCommand_(QString const &cmdname, QString const &cmdfile, QStringList const &list, QString path, std::function<void(QString const &)> callback)
{
	QString window_title = tr("Select %1 command");
	window_title = window_title.arg(cmdfile);

	SelectCommandDialog dlg(this, cmdname, cmdfile, path, list);
	dlg.setWindowTitle(window_title);
	if (dlg.exec() == QDialog::Accepted) {
		path = dlg.selectedFile();
		path = misc::normalizePathSeparator(path);
		QFileInfo info(path);
		if (info.isExecutable()) {
			callback(path);
			return path;
		}
	}
	return QString();
}

QString MainWindow::selectGitCommand()
{
#ifdef Q_OS_WIN
	char const *exe = "git.exe";
#else
	char const *exe = "git";
#endif
	QString path = pv->gcx.git_command;

	auto fn = [&](QString const &path){
		setGitCommand(path, true);
	};

	QStringList list = whichCommand_(exe);
#ifdef Q_OS_WIN
	{
		QStringList newlist;
		QString suffix1 = "\\bin\\git.exe";
		QString suffix2 = "\\cmd\\git.exe";
		for (QString const &s : list) {
			newlist.push_back(s);
			auto DoIt = [&](QString const &suffix){
				if (s.endsWith(suffix)) {
					QString t = s.mid(0, s.size() - suffix.size());
					QString t1 = t + "\\mingw64\\bin\\git.exe";
					if (QFileInfo(t1).isExecutable()) newlist.push_back(t1);
					QString t2 = t + "\\mingw\\bin\\git.exe";
					if (QFileInfo(t2).isExecutable()) newlist.push_back(t2);
				}
			};
			DoIt(suffix1);
			DoIt(suffix2);
		}
		std::sort(newlist.begin(), newlist.end());
		auto end = std::unique(newlist.begin(), newlist.end());
		list.clear();
		for (auto it = newlist.begin(); it != end; it++) {
			list.push_back(*it);
		}
	}
#endif
	return selectCommand_("Git", exe, list, path, fn);
}

QString MainWindow::selectFileCommand()
{
#ifdef Q_OS_WIN
	char const *exe = "file.exe";
#else
	char const *exe = "file";
#endif
	QString path = pv->file_command;

	auto fn = [&](QString const &path){
		setFileCommand(path, true);
	};

	QStringList list = whichCommand_(exe);

#ifdef Q_OS_WIN
	QString dir = getModuleFileDir();
	QString path1 = dir / "msys/file.exe";
	QString path2;
	int i = dir.lastIndexOf('/');
	int j = dir.lastIndexOf('\\');
	if (i < j) i = j;
	if (i > 0) {
		path2 = dir.mid(0, i) / "msys/file.exe";
	}
	path1 = misc::normalizePathSeparator(path1);
	path2 = misc::normalizePathSeparator(path2);
	if (QFileInfo(path1).isExecutable()) list.push_back(path1);
	if (QFileInfo(path2).isExecutable()) list.push_back(path2);
#endif

	return selectCommand_("File", exe, list, path, fn);
}

void MainWindow::checkGitCommand()
{
	while (1) {
		QFileInfo info(pv->gcx.git_command);
		if (info.isExecutable()) {
			break; // ok
		}
		if (selectGitCommand().isEmpty()) {
			close();
			break;
		}
	}
}

void MainWindow::checkFileCommand()
{
	while (1) {
		QFileInfo info(pv->file_command);
		if (info.isExecutable()) {
			break; // ok
		}
		if (selectFileCommand().isEmpty()) {
			close();
			break;
		}
	}
}

void MainWindow::timerEvent(QTimerEvent *)
{
	pv->interval_250ms_counter += pv->timer_interval_ms;
	if (pv->interval_250ms_counter >= 250) {
		pv->interval_250ms_counter -= 250;

		checkGitCommand();
		checkFileCommand();
	}

	if (pv->update_files_list_counter > 0) {
		if (pv->update_files_list_counter > pv->timer_interval_ms) {
			pv->update_files_list_counter -= pv->timer_interval_ms;
		} else {
			pv->update_files_list_counter = 0;
			updateCurrentFilesList();
		}
	}
}

bool MainWindow::event(QEvent *event)
{
	return QMainWindow::event(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *e = dynamic_cast<QKeyEvent *>(event);
		Q_ASSERT(e);
		int k = e->key();
		if (watched == ui->treeWidget_repos) {
			if (k == Qt::Key_Enter || k == Qt::Key_Return) {
				openSelectedRepository();
				return true;
			}
		} else if (watched == ui->tableWidget_log) {
			if (k == Qt::Key_Home) {
				ui->tableWidget_log->setCurrentCell(0, 0);
				return true;
			}
		}
	} else if (event->type() == QEvent::FocusIn) {
		// ファイルリストがフォーカスを得たとき、diffビューを更新する。（コンテキストメニュー対応）
		if (watched == ui->listWidget_unstaged) {
			updateUnstagedFileCurrentItem();
			return true;
		}
		if (watched == ui->listWidget_staged) {
			updateStagedFileCurrentItem();
			return true;
		}
	}
	return false;
}

bool MainWindow::saveByteArrayAs(QByteArray const &ba, QString const &dstpath)
{
	QFile file(dstpath);
	if (file.open(QFile::WriteOnly)) {
		file.write(ba);
		file.close();
		return true;
	} else {
		QString msg = "Failed to open the file '%1' for write";
		msg = msg.arg(dstpath);
		qDebug() << msg;
	}
	return false;
}

bool MainWindow::saveFileAs(QString const &srcpath, QString const &dstpath)
{
	QFile f(srcpath);
	if (f.open(QFile::ReadOnly)) {
		QByteArray ba = f.readAll();
		if (saveByteArrayAs(ba, dstpath)) {
			return true;
		}
	} else {
		QString msg = "Failed to open the file '%1' for read";
		msg = msg.arg(srcpath);
		qDebug() << msg;
	}
	return false;
}

bool MainWindow::saveBlobAs(QString const &id, QString const &dstpath)
{
	QByteArray ba;
	cat_file(id, &ba);
	if (!ba.isEmpty()) {
		if (saveByteArrayAs(ba, dstpath)) {
			return true;
		}
	} else {
		QString msg = "Failed to get the content of the object '%1'";
		msg = msg.arg(id);
		qDebug() << msg;
	}
	return false;
}

bool MainWindow::saveAs(QString const &id, QString const &dstpath)
{
	if (id.startsWith(PATH_PREFIX)) {
		return saveFileAs(id.mid(1), dstpath);
	} else {
		return saveBlobAs(id, dstpath);
	}
}

QString MainWindow::saveAsTemp(QString const &id)
{
	QString path = newTempFilePath();
	saveAs(id, path);
	return path;
}

void MainWindow::on_action_edit_settings_triggered()
{
	SettingsDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		pv->appsettings = dlg.settings();
		setGitCommand(pv->appsettings.git_command, false);
		setFileCommand(pv->appsettings.file_command, false);
	}
}

void MainWindow::on_action_branch_new_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	NewBranchDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		QString name = dlg.branchName();
		g->createBranch(name);
	}
}

void MainWindow::on_action_branch_checkout_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QList<Git::Branch> branches = g->branches();
	CheckoutBranchDialog dlg(this, branches);
	if (dlg.exec() == QDialog::Accepted) {
		QString name = dlg.branchName();
		if (!name.isEmpty()) {
			g->checkoutBranch(name);
			openRepository();
		}
	}
}

void MainWindow::on_action_branch_merge_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QList<Git::Branch> branches = g->branches();
	MergeBranchDialog dlg(this, branches);
	if (dlg.exec() == QDialog::Accepted) {
		QString name = dlg.branchName();
		if (!name.isEmpty()) {
			g->mergeBranch(name);
		}
	}

}

void MainWindow::on_action_clone_triggered()
{
	GitPtr g = std::shared_ptr<Git>(new Git(pv->gcx, QString()));
	CloneDialog dlg(this, g, defaultWorkingDir());
	if (dlg.exec() == QDialog::Accepted) {
		QString dir = dlg.workingDir();
		RepositoryItem item;
		item.local_dir = dir;
		item.name = makeRepositoryName(dir);
		saveRepositoryBookmark(item);
		pv->current_repo = item;
		openRepository();
	}
}

void MainWindow::on_action_about_triggered()
{
	AboutDialog dlg(this);
	dlg.exec();

}

void MainWindow::on_toolButton_clone_clicked()
{
	ui->action_clone->trigger();
}

void MainWindow::on_toolButton_fetch_clicked()
{
	ui->action_fetch->trigger();
}

void MainWindow::on_comboBox_filter_currentTextChanged(const QString &text)
{
	pv->repository_filter_text = text;
	updateRepositoriesList();
}

void MainWindow::on_toolButton_erase_filter_clicked()
{
	ui->comboBox_filter->clearEditText();
	ui->treeWidget_repos->setFocus();
}

void MainWindow::deleteTags(QStringList const &tagnames)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	if (!tagnames.isEmpty()) {
		reopenRepository([&](GitPtr g){
			for (QString const &name : tagnames) {
				g->delete_tag(name, true);
			}
		});
	}
}

void MainWindow::deleteTags(const Git::CommitItem &commit)
{
	auto it = pv->tag_map.find(commit.commit_id);
	if (it != pv->tag_map.end()) {
		QStringList names;
		QList<Git::Tag> const &tags = it->second;
		for (Git::Tag const &tag : tags) {
			names.push_back(tag.name);
		}
		deleteTags(names);
	}
}

void MainWindow::deleteSelectedTags()
{
	Git::CommitItem const *commit = selectedCommitItem();
	if (commit) {
		QList<Git::Tag> list = findTag(commit->commit_id);
		if (!list.isEmpty()) {
			DeleteTagsDialog dlg(this, list);
			if (dlg.exec() == QDialog::Accepted) {
				QStringList list = dlg.selectedTags();
				deleteTags(list);
			}
		}
	}
}

void MainWindow::addTag()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QString commit_id;

	Git::CommitItem const *commit = selectedCommitItem();
	if (commit && !commit->commit_id.isEmpty()) {
		commit_id = commit->commit_id;
	}

	EditTagDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		reopenRepository([&](GitPtr g){
			g->tag(dlg.text(), commit_id);
			if (dlg.isPushChecked()) {
				g->push(true);
			}
		});
	}
}



void MainWindow::on_action_tag_triggered()
{
	addTag();
}

void MainWindow::on_action_tag_push_all_triggered()
{
	reopenRepository([&](GitPtr g){
		g->push(true);
	});
}

void MainWindow::on_action_tag_delete_triggered()
{
	deleteSelectedTags();
}

QString MainWindow::tempfileHeader() const
{
	QString name = "jp_soramimi_Guitar_%1_";
	return name.arg(QApplication::applicationPid());

}

void MainWindow::deleteTempFiles()
{
	QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
	QString name = tempfileHeader();
	QDirIterator it(dir, { name + "*" });
	while (it.hasNext()) {
		QString path = it.next();
		QFile::remove(path);
		qDebug() << path;
	}
}

QString MainWindow::newTempFilePath()
{
	QString tmpdir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
	QString path = tmpdir / tempfileHeader() + QString::number(pv->temp_file_counter);
	pv->temp_file_counter++;
	return path;
}

QString MainWindow::determinFileType_(QString const &path, bool mime, std::function<void(QString const &cmd, QByteArray *ba)> callback)
{ // ファイルタイプを調べる
	if (QFileInfo(pv->file_command).isExecutable()) {
		QString file = pv->file_command;
		QString mgc;
#ifdef Q_OS_WIN
		int i = file.lastIndexOf('/');
		int j = file.lastIndexOf('\\');
		if (i < j) i = j;
		if (i >= 0) {
			mgc = file.mid(0, i + 1) + "magic.mgc";
			if (QFileInfo(mgc).isReadable()) {
				// ok
			} else {
				mgc = QString();
			}
		}
#endif
		QString cmd;
		if (mgc.isEmpty()) {
			cmd = "\"%1\"";
			cmd = cmd.arg(file);
		} else {
			cmd = "\"%1\" -m \"%2\"";
			cmd = cmd.arg(file).arg(mgc);
		}
		if (mime) {
			cmd += " --mime";
		}
		cmd += QString(" --brief \"%1\"").arg(path);

		// run file command

		QByteArray ba;

		callback(cmd, &ba);

		// parse file type

		if (!ba.isEmpty()) {
			QString s = QString::fromUtf8(ba).trimmed();
			QStringList list = s.split(';', QString::SkipEmptyParts);
			if (!list.isEmpty()) {
				QString mimetype = list[0].trimmed();
				qDebug() << mimetype;
				return mimetype;
			}
		}

		return cmd;

	} else {
		qDebug() << "No executable 'file' command";
	}
	return QString();
}

QString MainWindow::determinFileType(QString const &path, bool mime)
{
	return determinFileType_(path, mime, [](QString const &cmd, QByteArray *ba){
		misc::runCommand(cmd, ba);
	});
}

QString MainWindow::determinFileType(QByteArray const &in, bool mime)
{
	return determinFileType_("-", mime, [&](QString const &cmd, QByteArray *ba){
		misc::runCommand(cmd, &in, ba);
	});
}

void MainWindow::on_tableWidget_log_itemDoubleClicked(QTableWidgetItem *)
{
	Git::CommitItem const *commit = selectedCommitItem();
	if (commit) {
		execCommitPropertyDialog(commit);
	}
}

void MainWindow::execFilePropertyDialog(QListWidgetItem *item)
{
	if (item) {
		QString path = getFilePath(item);
		QString id = getObjectID(item);
		FilePropertyDialog dlg(this);
		dlg.exec(this, path, id);
	}
}


void MainWindow::on_listWidget_unstaged_itemDoubleClicked(QListWidgetItem * item)
{
	execFilePropertyDialog(item);
}

void MainWindow::on_listWidget_staged_itemDoubleClicked(QListWidgetItem *item)
{
	execFilePropertyDialog(item);
}

void MainWindow::on_listWidget_files_itemDoubleClicked(QListWidgetItem *item)
{
	execFilePropertyDialog(item);
}

void MainWindow::on_action_test_triggered()
{
}

QPixmap MainWindow::getTransparentPixmap()
{
	if (pv->transparent_pixmap.isNull()) {
		pv->transparent_pixmap = QPixmap(":/image/transparent.png");
	}
	return pv->transparent_pixmap;
}

