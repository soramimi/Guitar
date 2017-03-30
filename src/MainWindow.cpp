#include "MainWindow.h"
#include "ui_MainWindow.h"

#ifdef Q_OS_WIN
#include "win32/win32.h"
#endif

#include "AboutDialog.h"
#include "CheckoutDialog.h"
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
#include "CommitExploreWindow.h"
#include "SetRemoteUrlDialog.h"
#include "CommitExploreWindow.h"
#include "SetUserDialog.h"
#include "ProgressDialog.h"
#include "JumpDialog.h"
#include "DeleteBranchDialog.h"

#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QStandardPaths>
#include <QKeyEvent>
#include <QProcess>
#include <QDirIterator>
#include <QThread>
#include <QMimeData>
#include <QDragEnterEvent>

#include <deque>
#include <set>



class AsyncExecGitThread_ : public QThread {
private:
	GitPtr g;
	std::function<void(GitPtr g)> callback;
public:
	AsyncExecGitThread_(GitPtr g, std::function<void(GitPtr g)> callback)
		: g(g)
		, callback(callback)
	{
	}
protected:
	void run()
	{
		callback(g);
	}
};


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
	std::map<int, QList<Label>> label_map;
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
	QLabel *status_bar_label;
	bool ui_blocked = false;
};

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	pv = new Private();
	ui->setupUi(this);
	ui->splitter_v->setSizes({100, 400});
	ui->splitter_h->setSizes({200, 100, 200});

	pv->status_bar_label = new QLabel(this);
	ui->statusBar->addWidget(pv->status_bar_label);

	ui->widget_diff_view->bind(this);

	qApp->installEventFilter(this);
	ui->treeWidget_repos->installEventFilter(this);
	ui->tableWidget_log->installEventFilter(this);
	ui->listWidget_staged->installEventFilter(this);
	ui->listWidget_unstaged->installEventFilter(this);

	SettingsDialog::loadSettings(&pv->appsettings);
	{
		MySettings s;
		s.beginGroup("Global");
		pv->gcx.git_command = s.value("GitCommand").toString();
		pv->file_command = s.value("FileCommand").toString();
		s.endGroup();
	}

	ui->widget_log->init(this);
	onLogVisibilityChanged();

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

	writeLog(AboutDialog::appVersion() + '\n');
	logGitVersion();

#if USE_LIBGIT2
	LibGit2::init();
	//	LibGit2::test();
#endif

	connect(ui->dockWidget_log, SIGNAL(visibilityChanged(bool)), this, SLOT(onLogVisibilityChanged()));

	connect(ui->treeWidget_repos, SIGNAL(dropped()), this, SLOT(onRepositoriesTreeDropped()));

	QString path = getBookmarksFilePath();
	pv->repos = RepositoryBookmark::load(path);
	updateRepositoriesList();

	setUnknownRepositoryInfo();

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

void MainWindow::setCurrentLogRow(int row)
{
	ui->tableWidget_log->setCurrentCell(row, 2);
}

bool MainWindow::event(QEvent *event)
{
	QEvent::Type et = event->type();
	if (et == QEvent::KeyPress) {
		QKeyEvent *e = dynamic_cast<QKeyEvent *>(event);
		Q_ASSERT(e);
		int k = e->key();
		if (k == Qt::Key_Escape) {
			emit onEscapeKeyPressed();
		} else if (k == Qt::Key_Delete) {
			if (focusWidget() == ui->treeWidget_repos) {
				removeSelectedRepositoryFromBookmark(true);
				return true;
			}
//		} else if (k == Qt::Key_F2) {
//			if (focusWidget() == ui->treeWidget_repos) {
////				selected
////				int i = indexOfRepository(ui->treeWidget_repos->currentItem());
//				QTreeWidgetItem *item = ui->treeWidget_repos->currentItem();
//				item->setFlags(item->flags() | Qt::ItemIsEditable);
//				ui->treeWidget_repos->editItem(ui->treeWidget_repos->currentItem());

////				removeSelectedRepositoryFromBookmark(true);
//				return true;
//			}
		}
	}

	return QMainWindow::event(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
	QEvent::Type et = event->type();

	if (pv->ui_blocked) {
		if (et == QEvent::KeyPress || et == QEvent::KeyRelease) {
			QKeyEvent *e = dynamic_cast<QKeyEvent *>(event);
			Q_ASSERT(e);
			if (e->key() == Qt::Key_Escape) {
				return false; // default event handling
			}
			return true; // discard event
		}
		switch (et) {
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease:
		case QEvent::MouseButtonDblClick:
		case QEvent::MouseMove:
		case QEvent::Close:
		case QEvent::Drop:
		case QEvent::Wheel:
		case QEvent::ContextMenu:
		case QEvent::InputMethod:
		case QEvent::TabletMove:
		case QEvent::TabletPress:
		case QEvent::TabletRelease:
			return true; // discard event
		}
	}

	if (et == QEvent::KeyPress) {
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
				setCurrentLogRow(0);
				return true;
			}
		}
	} else if (et == QEvent::FocusIn) {
		// ファイルリストがフォーカスを得たとき、diffビューを更新する。（コンテキストメニュー対応）
		if (watched == ui->listWidget_unstaged) {
			updateStatusBarText();
			updateUnstagedFileCurrentItem();
			return true;
		}
		if (watched == ui->listWidget_staged) {
			updateStatusBarText();
			updateStagedFileCurrentItem();
			return true;
		}
	}
	return false;
}

void MainWindow::setStatusBarText(QString const &text)
{
	pv->status_bar_label->setText(text);
}

void MainWindow::clearStatusBarText()
{
	setStatusBarText(QString());
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

void MainWindow::onLogVisibilityChanged()
{
	ui->action_window_log->setChecked(ui->dockWidget_log->isVisible());
}

void MainWindow::writeLog(QString const &str)
{
	ui->widget_log->termWrite(str);

	ui->widget_log->updateControls();
	ui->widget_log->scrollToBottom();
	ui->widget_log->update();
}

void MainWindow::writeLog(QByteArray ba)
{
	QString s = QString::fromUtf8(ba);
	writeLog(s);
}

bool MainWindow::write_log_callback(void *cookie, char const *ptr, int len)
{
	MainWindow *me = (MainWindow *)cookie;
	QByteArray ba(ptr, len);
	me->writeLog(ba);
	return true;
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
	return std::shared_ptr<Git>(new Git(pv->gcx, dir));
}

GitPtr MainWindow::git()
{
	return git(currentWorkingCopyDir());
}

void MainWindow::setLogEnabled(GitPtr g, bool f)
{
	if (f) {
		g->setLogCallback(write_log_callback, this);
	} else {
		g->setLogCallback(nullptr, nullptr);
	}
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
	item->setFlags(item->flags() | Qt::ItemIsEditable);
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
	pv->label_map.clear();
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
		GitDiff dm(d.objcache);
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

bool MainWindow::isDiffThreadValid(QString const &id) const
{
	if (pv->diff.thread) {
		DiffThread *th = dynamic_cast<DiffThread *>(pv->diff.thread.get());
		Q_ASSERT(th);
		if (id == th->id() && !th->isInterruptionRequested()) { // IDが一致して中断されていない
			return true;
		}
	}
	return false;
}

void MainWindow::startDiff(GitPtr g, QString id)
{ // diffスレッドを開始する
	if (!isValidWorkingCopy(g)) return;

	Git::FileStatusList list = g->status();
	pv->uncommited_changes = !list.empty();

	if (id.isEmpty()) {
		if (isThereUncommitedChanges()) {
			id = QString(); // uncommited changes
		} else {
			id = pv->objcache.revParse("HEAD");
		}
	}

	if (isDiffThreadValid(id)) {
		// 同じ問い合わせのスレッドが実行中なので何もしない
	} else {
		stopDiff(); // 現在処理中のスレッドを停止

		bool uncommited = (id.isEmpty() && isThereUncommitedChanges());

		DiffThread *th = new DiffThread(g->dup(), &pv->objcache, id, uncommited);
		pv->diff.thread = std::shared_ptr<QThread>(th);
		th->start();
	}
}

bool MainWindow::makeDiff(QString const &id, QList<Git::Diff> *out)
{ // diffリストを取得する
#if SINGLE_THREAD
	GitPtr g = git();
	if (isValidWorkingCopy(g)) {
		GitDiff dm(&pv->objcache);
		if (dm.diff(id, out)) {
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
//	qDebug() << "failed to makeDiff";
	return false;
}

void MainWindow::updateFilesList(QString id, bool wait)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	startDiff(g, id);

	if (!wait) return;

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
		if (!makeDiff(uncommited ? QString() : id, &pv->diff.result)) {
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
			QString path = s.path1();
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
				path = s.path2(); // renamed newer path
			} else if (s.code_x() == 'M' || s.code_y() == 'M') {
				header = "(chg) ";
			}
			AddItem(path, header, idiff, staged);
		}
	} else {
		if (!makeDiff(id, &pv->diff.result)) {
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

void MainWindow::updateFilesList(Git::CommitItem const &commit, bool wait)
{
	QString id;
	if (Git::isUncommited(commit)) {
		// empty id for uncommited changes
	} else {
		id = commit.commit_id;
	}
	updateFilesList(id, wait);
}

void MainWindow::updateCurrentFilesList()
{
	QTableWidgetItem *item = ui->tableWidget_log->item(selectedLogIndex(), 0);
	if (!item) return;
	int row = item->data(IndexRole).toInt();
	int logs = (int)pv->logs.size();
	if (row < logs) {
		updateFilesList(pv->logs[row], true);
	}
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

QString MainWindow::abbrevCommitID(Git::CommitItem const &commit)
{
	return commit.commit_id.mid(0, 7);
}

QString MainWindow::findFileID(GitPtr /*g*/, const QString &commit_id, const QString &file)
{
	return lookupFileID(&pv->objcache, commit_id, file);
}

QList<MainWindow::Label> const *MainWindow::label(int row)
{
	auto it = pv->label_map.find(row);
	if (it != pv->label_map.end()) {
		return &it->second;
	}
	return nullptr;
}

QString MainWindow::makeCommitInfoText(int row, QList<Label> *label_list)
{
	QString message_ex;
	Git::CommitItem const *commit = &pv->logs[row];
	{ // branch
		if (label_list) {
			if (commit->commit_id == pv->head_id) {
				Label label(Label::Head);
				label.text = "HEAD";
				label_list->push_back(label);
			}
		}
		QList<Git::Branch> list = findBranch(commit->commit_id);
		for (Git::Branch const &b : list) {
			if (b.flags & Git::Branch::HeadDetached) continue;
			Label label(Label::LocalBranch);
			label.text = b.name;//misc::abbrevBranchName(b.name);
			if (label.text.startsWith("remotes/")) {
				label.kind = Label::RemoteBranch;
			}
			if (b.ahead > 0) {
				label.text += tr(", %1 ahead").arg(b.ahead);
			}
			if (b.behind > 0) {
				label.text += tr(", %1 behind").arg(b.behind);
			}
			message_ex += " {" + label.text + '}';
			if (label_list) label_list->push_back(label);
		}
	}
	{ // tag
		QList<Git::Tag> list = findTag(commit->commit_id);
		for (Git::Tag const &t : list) {
			Label label(Label::Tag);
			label.text = t.name;
			message_ex += QString(" {t:%1}").arg(label.text);
			if (label_list) label_list->push_back(label);
		}
	}
	return message_ex;
}

void MainWindow::setWindowTitle_(Git::User const &user)
{
	if (user.name.isEmpty() && user.email.isEmpty()) {
		setWindowTitle(qApp->applicationName());
	} else {
		setWindowTitle(QString("%1 : %2 <%3>")
					   .arg(qApp->applicationName())
					   .arg(user.name)
					   .arg(user.email)
					   );
	}
}

void MainWindow::setRepositoryInfo(QString const &reponame, QString const &brname)
{
	ui->label_repo_name->setText(reponame);
	ui->label_branch_name->setText(brname);
}

void MainWindow::setUnknownRepositoryInfo()
{
	setRepositoryInfo("---", "");

	Git g(pv->gcx, QString());
	Git::User user = g.getUser(Git::GetUserGlobal);
	setWindowTitle_(user);
}

void MainWindow::updateWindowTitle(GitPtr g)
{
	if (isValidWorkingCopy(g)) {
		Git::User user = g->getUser(Git::GetUserDefault);
		setWindowTitle_(user);
	} else {
		setUnknownRepositoryInfo();
	}
}

int MainWindow::limitLogCount() const
{
	return 10000;
}

class RetrieveLogThread_ : public QThread {
private:
	std::function<void()> callback;
protected:
	void run()
	{
		callback();
	}
public:
	RetrieveLogThread_(std::function<void()> callback)
		: callback(callback)
	{
	}
};

bool MainWindow::log_callback(void *cookie, char const *ptr, int len)
{
	ProgressDialog *dlg = (ProgressDialog *)cookie;
	if (dlg->canceledByUser()) {
		qDebug() << "canceled";
		return false;
	}

	QString text = QString::fromUtf8(ptr, len);
	emit dlg->writeLog(text);

	return true;
}



void MainWindow::openRepository_(GitPtr g)
{
	clearLog();
	clearRepositoryInfo();

	pv->objcache.setup(g);

	if (isValidWorkingCopy(g)) {
		updateFilesList(QString(), true);

		{
			ProgressDialog dlg(this);
			dlg.setLabelText(tr("Retrieving the log is in progress"));

			RetrieveLogThread_ th([&](){
				emit dlg.writeLog(tr("Retrieving commit log...\n"));
				// ログを取得
				pv->logs = g->log(limitLogCount());
				// ブランチを取得
				emit dlg.writeLog(tr("Retrieving branches...\n"));
				queryBranches(g);
				// タグを取得
				emit dlg.writeLog(tr("Retrieving tags...\n"));
				pv->tag_map.clear();
				QList<Git::Tag> tags = g->tags();
				for (Git::Tag const &tag : tags) {
					pv->tag_map[tag.id].push_back(tag);
					if (dlg.canceledByUser()) {
						return;
					}
				}

				emit dlg.finish();
			});
			th.start();

			if (th.wait(3000)) {
				// thread completed
			} else {
				dlg.show();
				dlg.grabMouse();
				dlg.exec();
				dlg.releaseMouse();
				th.wait();
			}

			if (dlg.canceledByUser()) {
				setUnknownRepositoryInfo();
				writeLog(tr("Canceled by user\n"));
				return;
			}
		}


		QString branch_name = currentBranch().name;
		if (currentBranch().flags & Git::Branch::HeadDetached) {
			branch_name = QString("(HEAD detached at %1)").arg(branch_name);
		}

		QString repo_name = currentRepositoryName();
		setRepositoryInfo(repo_name, branch_name);
	}
	updateWindowTitle(g);

	pv->head_id = pv->objcache.revParse("HEAD");

	if (isThereUncommitedChanges()) {
		Git::CommitItem item;
		item.parent_ids.push_back(pv->current_branch.id);
		item.message = tr("Uncommited changes");
		pv->logs.insert(pv->logs.begin(), item);
	}

	prepareLogTableWidget();

	int count = pv->logs.size();

	ui->tableWidget_log->setRowCount(count);

	int selrow = -1;

	for (int row = 0; row < count; row++) {
		Git::CommitItem const *commit = &pv->logs[row];
		{
			QTableWidgetItem *item = new QTableWidgetItem();
			item->setData(IndexRole, row);
			ui->tableWidget_log->setItem(row, 0, item);
		}
		int col = 1; // カラム0はコミットグラフなので、その次から
		auto AddColumn = [&](QString const &text, bool bold, QString const &tooltip){
			QTableWidgetItem *item = new QTableWidgetItem(text);
			item->setToolTip(tooltip);
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
		QString message_ex;
		bool isHEAD = commit->commit_id == pv->head_id;
		bool bold = false;
		{
			if (Git::isUncommited(*commit)) { // 未コミットの時
				bold = true; // 太字
				selrow = row;
			} else {
				if (isHEAD && !isThereUncommitedChanges()) { // HEADで、未コミットがないとき
					bold = true; // 太字
					selrow = row;
				}
				commit_id = abbrevCommitID(*commit);
			}
			datetime = misc::makeDateTimeString(commit->commit_date);
			author = commit->author;
			message = commit->message;
			message_ex = makeCommitInfoText(row, &pv->label_map[row]);
		}
		AddColumn(commit_id, false, QString());
		AddColumn(datetime, false, QString());
		AddColumn(author, false, QString());
		AddColumn(message, bold, message + message_ex);
		ui->tableWidget_log->setRowHeight(row, 24);
	}
	ui->tableWidget_log->resizeColumnsToContents();
	ui->tableWidget_log->horizontalHeader()->setStretchLastSection(false);
	ui->tableWidget_log->horizontalHeader()->setStretchLastSection(true);

	ui->tableWidget_log->setFocus();
	setCurrentLogRow(0);

	QTableWidgetItem *p = ui->tableWidget_log->item(selrow < 0 ? 0 : selrow, 2);
	ui->tableWidget_log->setCurrentItem(p);

	udpateButton();
}

void MainWindow::removeRepositoryFromBookmark(int index, bool ask)
{
	if (ask) {
		int r = QMessageBox::warning(this, tr("Confirm Remove"), tr("Are you sure you want to remove the repository from bookmarks ?") + '\n' + tr("(Files will NOT be deleted)"), QMessageBox::Ok, QMessageBox::Cancel);
		if (r != QMessageBox::Ok) return;
	}
	if (index >= 0 && index < pv->repos.size()) {
		pv->repos.erase(pv->repos.begin() + index);
		saveRepositoryBookmarks();
		updateRepositoriesList();
	}
}

void MainWindow::removeSelectedRepositoryFromBookmark(bool ask)
{
	int i = indexOfRepository(ui->treeWidget_repos->currentItem());
	removeRepositoryFromBookmark(i, ask);
}

void MainWindow::openRepository(bool validate, bool waitcursor)
{
	if (validate) {
		QString dir = currentWorkingCopyDir();
		if (!QFileInfo(dir).isDir()) {
			int r = QMessageBox::warning(this, tr("Open Repository"), dir + "\n\n" + tr("No such folder") + "\n\n" + tr("Remove from bookmark ?"), QMessageBox::Ok, QMessageBox::Cancel);
			if (r == QMessageBox::Ok) {
				removeSelectedRepositoryFromBookmark(false);
			}
			return;
		}
		if (!Git::isValidWorkingCopy(dir)) {
			QMessageBox::warning(this, tr("Open Repository"), tr("Not a valid git repository") + "\n\n" + dir);
			return;
		}
	}

	if (waitcursor) {
		OverrideWaitCursor;
		openRepository(false, false);
		return;
	}

	GitPtr g = git(); // ポインタの有効性チェックはしない（nullptrでも続行）
	openRepository_(g);
}

void MainWindow::reopenRepository(bool log, std::function<void(GitPtr g)> callback)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	OverrideWaitCursor;
	if (log) {
		setLogEnabled(g, true);
		AsyncExecGitThread_ th(g, callback);
		th.start();
		while (1) {
			if (th.wait(1)) break;
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		}
		setLogEnabled(g, false);
	} else {
		callback(g);
	}
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
		openRepository(false);
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
	QTreeWidgetItem *treeitem = ui->treeWidget_repos->currentItem();
	RepositoryItem const *item = repositoryItem(treeitem);
	if (item) {
		pv->current_repo = *item;
		openRepository(true, true);
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
	updateCurrentFilesList();
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
			openRepository(true);
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


void MainWindow::updateStatusBarText()
{
	QString text;

	QWidget *w = QWidget::focusWidget();
	if (w == ui->treeWidget_repos) {
		QTreeWidgetItem *ite = ui->treeWidget_repos->currentItem();
		RepositoryItem const *item = repositoryItem(ite);
		if (item) {
			text = QString("%1 : %2")
					.arg(item->name)
					.arg(misc::normalizePathSeparator(item->local_dir))
					;
		}
	} else if (w == ui->tableWidget_log) {
		QTableWidgetItem *item = ui->tableWidget_log->item(selectedLogIndex(), 0);
		if (item) {
			int row = item->data(IndexRole).toInt();
			if (row < (int)pv->logs.size()) {
				Git::CommitItem const &commit = pv->logs[row];
				if (Git::isUncommited(commit)) {
					text = tr("Uncommited changes");
				} else {
					QString id = commit.commit_id;
					text = QString("%1 : %2%3")
							.arg(id.mid(0, 7))
							.arg(commit.message)
							.arg(makeCommitInfoText(row, nullptr))
							;
				}
			}
		}
	}

	setStatusBarText(text);
}

void MainWindow::on_action_commit_triggered()
{
	commit();
}

void MainWindow::on_action_fetch_triggered()
{
	reopenRepository(true, [&](GitPtr g){
		g->fetch();
	});
}

void MainWindow::on_action_push_triggered()
{
	reopenRepository(true, [&](GitPtr g){
		g->push();
	});
}

void MainWindow::on_action_pull_triggered()
{
	reopenRepository(true, [&](GitPtr g){
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

void MainWindow::on_treeWidget_repos_currentItemChanged(QTreeWidgetItem * /*current*/, QTreeWidgetItem * /*previous*/)
{
	updateStatusBarText();
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

int MainWindow::indexOfRepository(QTreeWidgetItem const *treeitem) const
{
	return treeitem->data(0, IndexRole).toInt();
}

void MainWindow::on_treeWidget_repos_customContextMenuRequested(const QPoint &pos)
{
	QTreeWidgetItem *treeitem = ui->treeWidget_repos->currentItem();
	if (!treeitem) return;

	RepositoryItem const *repo = repositoryItem(treeitem);

	int index = indexOfRepository(treeitem);
	if (isGroupItem(treeitem)) { // group item
		QMenu menu;
		QAction *a_add_new_group = menu.addAction(tr("&Add new group"));
		QAction *a_delete_group = menu.addAction(tr("&Delete group"));
		QAction *a_rename_group = menu.addAction(tr("&Rename group"));
		QPoint pt = ui->treeWidget_repos->mapToGlobal(pos);
		QAction *a = menu.exec(pt + QPoint(8, -8));
		if (a) {
			if (a == a_add_new_group) {
				QTreeWidgetItem *child = newQTreeWidgetFolderItem(tr("New group"));
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
			if (a == a_rename_group) {
				ui->treeWidget_repos->editItem(treeitem);
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
		QAction *a_set_remote_url = menu.addAction(tr("Set remote URL"));

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
				removeRepositoryFromBookmark(index, true);
				return;
			}
			if (a == a_properties) {
				GitPtr g = git(repo->local_dir);
				RepositoryPropertyDialog dlg(this, g, *repo);
				dlg.exec();
				return;
			}
			if (a == a_set_remote_url) {
				GitPtr g = git(repo->local_dir);
				if (!isValidWorkingCopy(g)) return;
				QStringList remotes = g->getRemotes();
				SetRemoteUrlDialog dlg(this, remotes, g);
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

		bool is_valid_commit_id = Git::isValidID(commit->commit_id);

		QAction *a_checkout = is_valid_commit_id ? menu.addAction(tr("Checkout...")) : nullptr;
		QAction *a_delbranch = is_valid_commit_id ? menu.addAction(tr("Delete branch...")) : nullptr;

		QAction *a_add_tag = is_valid_commit_id ? menu.addAction(tr("Add a tag...")) : nullptr;
		QAction *a_delete_tags = nullptr;
		if (is_valid_commit_id && pv->tag_map.find(commit->commit_id) != pv->tag_map.end()) {
			a_delete_tags = menu.addAction(tr("Delete tags..."));
		}
		QAction *a_explore = is_valid_commit_id ? menu.addAction(tr("Explore")) : nullptr;

		QAction *a_reset_head = nullptr;
#if 0 // 下手に使うとヤバいので、とりあえず無効にしておく
		if (is_valid_commit_id && commit->commit_id == pv->head_id) {
			a_reset_head = menu.addAction(tr("Reset HEAD"));
		}
#endif

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
			if (a == a_checkout) {
				checkout(commit);
				return;
			}
			if (a == a_delbranch) {
				deleteBranch(commit);
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
			if (a == a_explore) {
				CommitExploreWindow win(this, &pv->objcache, commit);
				win.exec();
				return;
			}
			if (a == a_reset_head) {
				reopenRepository(false, [](GitPtr g){
					g->reset_head();
				});
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
				updateCurrentFilesList();
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
					updateCurrentFilesList();
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
					openRepository(false);
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
					updateCurrentFilesList();
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
			openRepository(true);
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
		openRepository(true);
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

void MainWindow::addWorkingCopyDir(QString dir)
{
	if (dir.endsWith(".git")) {
		int i = dir.size();
		if (i > 4) {
			ushort c = dir.utf16()[i - 5];
			if (c == '/' || c == '\\') {
				dir = dir.mid(0, i - 5);
			}
		}
	}

	if (!Git::isValidWorkingCopy(dir)) {
		qDebug() << "Invalid working dir: " + dir;
		return;
	}

	RepositoryItem item;
	item.local_dir = dir;
	item.name = makeRepositoryName(dir);
	saveRepositoryBookmark(item);
}

void MainWindow::on_action_open_existing_working_copy_triggered()
{
	QString dir = defaultWorkingDir();
	dir = QFileDialog::getExistingDirectory(this, tr("Add existing working copy"), dir);
	addWorkingCopyDir(dir);
}

void MainWindow::on_action_view_refresh_triggered()
{
	openRepository(true);
}

void MainWindow::on_tableWidget_log_currentItemChanged(QTableWidgetItem * /*current*/, QTableWidgetItem * /*previous*/)
{
	stopDiff();

	clearFileList();

	QTableWidgetItem *item = ui->tableWidget_log->item(selectedLogIndex(), 0);
	if (!item) return;

	int row = item->data(IndexRole).toInt();
	if (row < (int)pv->logs.size()) {
		updateFilesList(pv->logs[row], false); // 完了を待たない
		updateStatusBarText();
		pv->update_files_list_counter = 200;
	}
}



void MainWindow::on_toolButton_stage_clicked()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	g->stage(selectedUnstagedFiles());
	updateCurrentFilesList();
}

void MainWindow::on_toolButton_unstage_clicked()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	g->unstage(selectedStagedFiles());
	updateCurrentFilesList();
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
		} else {
			if (LogCount == 1) { // コミットが一つだけ
				LogItem(0).marker_depth = 0;
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

Git::Object MainWindow::cat_file_(GitPtr g, QString const &id)
{
	if (isValidWorkingCopy(g)) {
		QString path_prefix = PATH_PREFIX;
		if (id.startsWith(path_prefix)) {
			QString path = g->workingRepositoryDir();
			path = path / id.mid(path_prefix.size());
			QFile file(path);
			if (file.open(QFile::ReadOnly)) {
				Git::Object obj;
				obj.content = file.readAll();
				return obj;
			}
		} else if (Git::isValidID(id)) {
			return pv->objcache.catFile(id);;
		}
	}
	return Git::Object();
}

Git::Object MainWindow::cat_file(QString const &id)
{
	return cat_file_(git(), id);
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

void MainWindow::logGitVersion()
{
	GitPtr g = git();
	QString s = g->version();
	if (!s.isEmpty()) {
		s += '\n';
		writeLog(s);
	}
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

	logGitVersion();
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

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasUrls()) {
		event->acceptProposedAction();
		event->accept();
	}
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
	Git::Object obj = cat_file(id);
	if (!obj.content.isEmpty()) {
		if (saveByteArrayAs(obj.content, dstpath)) {
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

//void MainWindow::on_action_branch_checkout_triggered()
//{
//	GitPtr g = git();
//	if (!isValidWorkingCopy(g)) return;

//	QList<Git::Branch> branches = g->branches();
//	CheckoutDialog dlg(this, branches);
//	if (dlg.exec() == QDialog::Accepted) {
//		QString name = dlg.branchName();
//		if (!name.isEmpty()) {
//			g->checkoutBranch(name);
//			openRepository(true);
//		}
//	}
//}

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

bool MainWindow::clone_callback(void *cookie, char const *ptr, int len)
{
	ProgressDialog *dlg = (ProgressDialog *)cookie;
	if (dlg->canceledByUser()) {
		qDebug() << "canceled";
		return false;
	}

	QString text = QString::fromUtf8(ptr, len);
	emit dlg->writeLog(text);
//	qDebug() << text;

	return true;
}

void MainWindow::on_action_clone_triggered()
{ // クローン
	QString url;
	QString dir = defaultWorkingDir();
	while (1) {
		CloneDialog dlg(this, url, dir);
		if (dlg.exec() != QDialog::Accepted) {
			return;
		}

		url = dlg.url();
		dir = dlg.dir();

		// 既存チェック

		QFileInfo info(dir);
		if (info.isFile()) {
			QString msg = dir + "\n\n" + tr("A file with same name already exists");
			QMessageBox::warning(this, tr("Clone"), msg);
			continue;
		}
		if (info.isDir()) {
			QString msg = dir + "\n\n" + tr("A folder with same name already exists");
			QMessageBox::warning(this, tr("Clone"), msg);
			continue;
		}

		// クローン先ディレクトリを求める

		Git::CloneData clone_data = Git::preclone(url, dir);

		// クローン先ディレクトリの存在チェック

		QString basedir = misc::normalizePathSeparator(clone_data.basedir);
		if (!QFileInfo(basedir).isDir()) {
			int i = basedir.indexOf('/');
			int j = basedir.indexOf('\\');
			if (i < j) i = j;
			if (i < 0) {
				QString msg = basedir + "\n\n" + tr("Invalid folder");
				QMessageBox::warning(this, tr("Clone"), msg);
				continue;
			}

			QString msg = basedir + "\n\n" + tr("No such folder. Create it now ?");
			if (QMessageBox::warning(this, tr("Clone"), msg, QMessageBox::Ok, QMessageBox::Cancel) != QMessageBox::Ok) {
				continue;
			}

			// ディレクトリを作成

			QString base = basedir.mid(0, i + 1);
			QString sub = basedir.mid(i + 1);
			QDir(base).mkpath(sub);
		}

		ProgressDialog dlg2(this);
		dlg2.setLabelText(tr("Cloning is in progress"));

		GitPtr g = git(QString());
		g->setLogCallback(clone_callback, &dlg2);

		RetrieveLogThread_ th([&](){
			qDebug() << "cloning";
			g->clone(clone_data);
			emit dlg2.finish();
		});
		th.start();

		dlg2.exec();
		th.wait();

		g->setLogCallback(nullptr, nullptr);

		if (dlg2.canceledByUser()) {
			return; // canceled
		}

		RepositoryItem item;
		item.local_dir = dir;
		item.name = makeRepositoryName(dir);
		saveRepositoryBookmark(item);
		pv->current_repo = item;
		openRepository(true);

		return; // done
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

void MainWindow::on_lineEdit_filter_textChanged(const QString &text)
{
	pv->repository_filter_text = text;
	updateRepositoriesList();
}

void MainWindow::on_toolButton_erase_filter_clicked()
{
	ui->lineEdit_filter->clear();
	ui->lineEdit_filter->setFocus();
}

void MainWindow::deleteTags(QStringList const &tagnames)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	if (!tagnames.isEmpty()) {
		reopenRepository(false, [&](GitPtr g){
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

	if (!Git::isValidID(commit_id)) return;

	EditTagDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		reopenRepository(false, [&](GitPtr g){
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
	reopenRepository(false, [&](GitPtr g){
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
{
	return misc::determinFileType(pv->file_command, path, mime, callback);
}

QString MainWindow::determinFileType(QString const &path, bool mime)
{
	return determinFileType_(path, mime, [](QString const &cmd, QByteArray *ba){
		misc::runCommand(cmd, ba);
	});
}

QString MainWindow::determinFileType(QByteArray const &in, bool mime)
{
	// ファイル名を "-" にすると、リダイレクトで標準入力へ流し込める。
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

QPixmap MainWindow::getTransparentPixmap()
{
	if (pv->transparent_pixmap.isNull()) {
		pv->transparent_pixmap = QPixmap(":/image/transparent.png");
	}
	return pv->transparent_pixmap;
}

QString MainWindow::getCommitIdFromTag(QString const &tag)
{
	return pv->objcache.getCommitIdFromTag(tag);
}

bool MainWindow::isValidRemoteURL(const QString &url)
{
	if (url.indexOf('\"') >= 0) {
		return false;
	}
	Git g(pv->gcx, QString());
	QString cmd = "ls-remote \"%1\" HEAD";
	bool f = g.git(cmd.arg(url), false);
	QString line = g.resultText().trimmed();
	QString head;
	int i = line.indexOf('\t');
	if (i == GIT_ID_LENGTH) {
		QString id = line.mid(0, i);
		QString name = line.mid(i + 1);
		qDebug() << id << name;
		if (name == "HEAD" && Git::isValidID(id)) {
			head = id;
		}
	}
	if (f) {
		qDebug() << "This is a valid repository.";
		if (head.isEmpty()) {
			qDebug() << "But HEAD not found";
		}
		return true;
	} else {
		qDebug() << "It is not a repository.";
	}
	return false;
}

void MainWindow::testRemoteRepositoryValidity(QString const &url)
{
	bool f;
	{
		OverrideWaitCursor;
		f = isValidRemoteURL(url);
	}

	QString pass = tr("The URL is a valid repository");
	QString fail = tr("Failed to access the URL");

	QString text = "%1\n\n%2";
	text = text.arg(url).arg(f ? pass : fail);

	QString title = tr("Remote Repository");

	if (f) {
		QMessageBox::information(this, title, text);
	} else {
		QMessageBox::critical(this, title, text);
	}
}

void MainWindow::on_action_set_config_user_triggered()
{
	Git::User global_user;
	Git::User repo_user;
	GitPtr g = git();
	if (isValidWorkingCopy(g)) {
		repo_user = g->getUser(Git::GetUserLocal);
	}
	global_user = g->getUser(Git::GetUserGlobal);

	SetUserDialog dlg(this, global_user, repo_user, currentRepositoryName());
	if (dlg.exec() == QDialog::Accepted) {
		Git::User user = dlg.user();
		if (dlg.isGlobalChecked()) {
			g->setUser(user, true);
		}
		if (dlg.isRepositoryChecked()) {
			g->setUser(user, false);
		}
		updateWindowTitle(g);
	}
}

void MainWindow::on_action_window_log_triggered(bool checked)
{
	ui->dockWidget_log->setVisible(checked);
}

void MainWindow::setBlockUI(bool f)
{
	pv->ui_blocked = f;
	ui->menuBar->setEnabled(!pv->ui_blocked);
}

NamedCommitList MainWindow::namedCommitItems(int flags)
{
	NamedCommitList items;
	if (flags & Branches) {
		for (auto pair: pv->branch_map) {
			QList<Git::Branch> const &list = pair.second;
			for (Git::Branch const &b : list) {
				NamedCommitItem item;
				item.type = NamedCommitItem::Type::Branch;
				item.name = b.name;
				if (item.name.startsWith("remotes/")) {
					item.name = item.name.mid(8);
				}
				item.id = b.id;
				items.push_back(item);
			}
		}
	}
	if (flags & Tags) {
		for (auto pair: pv->tag_map) {
			QList<Git::Tag> const &list = pair.second;
			for (Git::Tag const &t : list) {
				NamedCommitItem item;
				item.type = NamedCommitItem::Type::Tag;
				item.name = t.name;
				item.id = t.id;
				items.push_back(item);
			}
		}
	}
	return items;
}

void MainWindow::on_action_repo_jump_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	NamedCommitList items = namedCommitItems(Branches | Tags);
	JumpDialog::sort(&items);
	JumpDialog dlg(this, items);
	if (dlg.exec() == QDialog::Accepted) {
		QString name = dlg.selectedName();
		QString id = g->rev_parse(name);
		int row = -1;
		for (size_t i = 0; i < pv->logs.size(); i++) {
			Git::CommitItem const &item = pv->logs[i];
			if (item.commit_id == id) {
				row = (int)i;
				break;
			}
		}
		if (row < 0) {
			QMessageBox::warning(this, tr("Jump"), QString("%1\n(%2)\n\n").arg(name).arg(id) + tr("That commmit has not foud or not read yet"));
		} else {
			setCurrentLogRow(row);
		}
	}
}

void MainWindow::checkout(Git::CommitItem const *commit)
{
	if (!commit) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QStringList tags;
	QStringList local_branches;
	QStringList remote_branches;
	{
		NamedCommitList named_commits = namedCommitItems(Branches | Tags);
		JumpDialog::sort(&named_commits);
		for (NamedCommitItem const &item : named_commits) {
			if (item.id == commit->commit_id) {
				QString name = item.name;
				if (item.type == NamedCommitItem::Type::Tag) {
					tags.push_back(name);
				} else if (item.type == NamedCommitItem::Type::Branch) {
					int i = name.lastIndexOf('/');
					if (i < 0) {
						if (name == "HEAD") continue;
						local_branches.push_back(name);
					} else {
						name = name.mid(i + 1);
						if (name == "HEAD") continue;
						remote_branches.push_back(name);
					}
				}
			}
		}
	}

	CheckoutDialog dlg(this, tags, local_branches, remote_branches);
	if (dlg.exec() == QDialog::Accepted) {
		CheckoutDialog::Operation op = dlg.operation();
		QString name = dlg.branchName();
		QString id = commit->commit_id;
		bool ok = false;
		setLogEnabled(g, true);
		if (op == CheckoutDialog::Operation::HeadDetached) {
			ok = g->git(QString("checkout \"%1\"").arg(id), true);
		} else if (op == CheckoutDialog::Operation::CreateLocalBranch) {
			ok = g->git(QString("checkout -b \"%1\" \"%2\"").arg(name).arg(id), true);
		} else if (op == CheckoutDialog::Operation::ExistingLocalBranch) {
			ok = g->git(QString("checkout \"%1\"").arg(name), true);
		}
		if (ok) {
			openRepository(true);
		}
	}

}

void MainWindow::on_action_repo_checkout_triggered()
{
	Git::CommitItem const *commit = selectedCommitItem();
	checkout(commit);
}

void MainWindow::deleteBranch(Git::CommitItem const *commit)
{
	if (!commit) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QStringList all_local_branch_names;
	QStringList current_local_branch_names;
	{
		NamedCommitList named_commits = namedCommitItems(Branches);
		JumpDialog::sort(&named_commits);
		for (NamedCommitItem const &item : named_commits) {
			QString name = item.name;
			int i = name.lastIndexOf('/');
			if (i < 0) {
				if (name == "HEAD") continue;
				if (item.id == commit->commit_id) {
					current_local_branch_names.push_back(name);
				}
				all_local_branch_names.push_back(name);
			}
		}
	}

	DeleteBranchDialog dlg(this, all_local_branch_names, current_local_branch_names);
	if (dlg.exec() == QDialog::Accepted) {
		setLogEnabled(g, true);
		QStringList names = dlg.selectedBranchNames();
		int count = 0;
		for (QString const &name : names) {
			if (g->git(QString("branch -D \"%1\"").arg(name))) {
				count++;
			} else {
				writeLog(tr("Failed to delete the branch '%1'\n").arg(name));
			}
		}
		if (count > 0) {
			openRepository(true);
		}
	}
}

void MainWindow::on_action_test_triggered()
{
	Git::CommitItem const *commit = selectedCommitItem();
	deleteBranch(commit);
}



void MainWindow::on_action_push_u_origin_master_triggered()
{
	reopenRepository(true, [&](GitPtr g){
		g->push_u_origin_master();
	});
}
