#include "MainWindow.h"
#include "ui_MainWindow.h"

#ifdef Q_OS_WIN
#include "win32/win32.h"
#else
#include <unistd.h>
#endif

#include "AboutDialog.h"
#include "AvatarLoader.h"
#include "CheckoutDialog.h"
#include "CloneDialog.h"
#include "CommitExploreWindow.h"
#include "CommitExploreWindow.h"
#include "CommitPropertyDialog.h"
#include "common/joinpath.h"
#include "common/misc.h"
#include "ConfigCredentialHelperDialog.h"
#include "CreateRepositoryDialog.h"
#include "DeleteBranchDialog.h"
#include "DeleteTagsDialog.h"
#include "EditTagDialog.h"
#include "FileHistoryWindow.h"
#include "FilePropertyDialog.h"
#include "FileUtil.h"
#include "Git.h"
#include "GitDiff.h"
#include "gunzip.h"
#include "JumpDialog.h"
#include "LibGit2.h"
#include "LocalSocketReader.h"
#include "main.h"
#include "MemoryReader.h"
#include "MergeBranchDialog.h"
#include "MyProcess.h"
#include "MySettings.h"
#include "NewBranchDialog.h"
#include "PushDialog.h"
#include "RepositoryData.h"
#include "RepositoryPropertyDialog.h"
#include "SelectCommandDialog.h"
#include "SetRemoteUrlDialog.h"
#include "SettingsDialog.h"
#include "SetUserDialog.h"
#include "StatusLabel.h"
#include "Terminal.h"
#include "TextEditDialog.h"
#include "webclient.h"
#include <deque>
#include <set>
#include <stdlib.h>

#include <QBuffer>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QKeyEvent>
#include <QLocalServer>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>

#ifdef Q_OS_MAC
extern "C" char **environ;
#endif

struct GitHubRepositoryInfo {
	QString owner_account_name;
	QString repository_name;
};

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

enum class PtyCondition {
	None,
	Clone,
	Fetch,
	Pull,
	Push,
};


struct MainWindow::Private {
	Git::Context gcx;
	ApplicationSettings appsettings;
	QString file_command;
	QList<RepositoryItem> repos;
	RepositoryItem current_repo;
	ServerType server_type = ServerType::Standard;
	GitHubRepositoryInfo github;
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
	int update_files_list_counter = 0;
	QTimer interval_10ms_timer;
	QTimer interval_250ms_timer;
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
	StatusLabel *status_bar_label;
	bool ui_blocked = false;

	WebContext webcx;
	AvatarLoader avatar_loader;
	int update_commit_table_counter = 0;

	std::map<QString, GitHubAPI::User> committer_map; // key is email

	PtyProcess pty_process;
	PtyCondition pty_condition = PtyCondition::None;
	RepositoryItem temp_repo;
};

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, m(new Private)
{
	ui->setupUi(this);
	ui->splitter_v->setSizes({100, 400});
	ui->splitter_h->setSizes({200, 100, 200});

	m->status_bar_label = new StatusLabel(this);
	ui->statusBar->addWidget(m->status_bar_label);

	ui->widget_diff_view->bind(this);

	qApp->installEventFilter(this);
	ui->treeWidget_repos->installEventFilter(this);
	ui->tableWidget_log->installEventFilter(this);
	ui->listWidget_staged->installEventFilter(this);
	ui->listWidget_unstaged->installEventFilter(this);

	ui->widget_log->bindScrollBar(ui->verticalScrollBar_log, ui->horizontalScrollBar_log);
	ui->widget_log->setTheme(TextEditorTheme::Light());
	ui->widget_log->setAutoLayout(true);
	ui->widget_log->setTerminalMode(true);
	ui->widget_log->layoutEditor();
	onLogVisibilityChanged();

	SettingsDialog::loadSettings(&m->appsettings);

	initNetworking();

	{
		MySettings s;
		s.beginGroup("Remote");
		bool f = s.value("Online", true).toBool();
		s.endGroup();
		setRemoteOnline(f);
	}


	showFileList(FilesListType::SingleList);

	QFileIconProvider icons;

	m->digits.load(":/image/digits.png");
	m->graph_color.load(":/image/graphcolor.png");

	m->repository_icon = QIcon(":/image/repository.png");
	m->folder_icon = icons.icon(QFileIconProvider::Folder);

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


#if USE_LIBGIT2
	LibGit2::init();
	//	LibGit2::test();
#endif

	connect(ui->dockWidget_log, SIGNAL(visibilityChanged(bool)), this, SLOT(onLogVisibilityChanged()));

	connect(ui->treeWidget_repos, SIGNAL(dropped()), this, SLOT(onRepositoriesTreeDropped()));

	connect((AbstractPtyProcess *)&m->pty_process, SIGNAL(completed()), this, SLOT(onPtyProcessCompleted()));

	QString path = getBookmarksFilePath();
	m->repos = RepositoryBookmark::load(path);
	updateRepositoriesList();

	setUnknownRepositoryInfo();

	m->webcx.set_keep_alive_enabled(true);
	m->avatar_loader.start(&m->webcx);
	connect(&m->avatar_loader, SIGNAL(updated()), this, SLOT(onAvatarUpdated()));

	m->update_files_list_counter = 0;

	startTimers();
}

MainWindow::~MainWindow()
{
#if USE_LIBGIT2
	LibGit2::shutdown();
#endif


	m->avatar_loader.interrupt();

	deleteTempFiles();

	m->avatar_loader.wait();

	delete m;
	delete ui;
}

void MainWindow::shown()
{
	writeLog(AboutDialog::appVersion() + '\n');
	setGitCommand(m->appsettings.git_command, false);
	setFileCommand(m->appsettings.file_command, false);
}

void MainWindow::startTimers()
{
	// interval 10ms

	connect(&m->interval_10ms_timer, &QTimer::timeout, [&](){
		const int ms = 10;
		if (m->update_commit_table_counter > 0) {
			if (m->update_commit_table_counter > ms) {
				m->update_commit_table_counter -= ms;
			} else {
				m->update_commit_table_counter = 0;
				ui->tableWidget_log->viewport()->update();
			}
		}
		if (m->update_files_list_counter > 0) {
			if (m->update_files_list_counter > ms) {
				m->update_files_list_counter -= ms;
			} else {
				m->update_files_list_counter = 0;
				updateCurrentFilesList();
			}
		}
	});
	m->interval_10ms_timer.setInterval(10);
	m->interval_10ms_timer.start();

	// interval 250ms

	connect(&m->interval_250ms_timer, &QTimer::timeout, [&](){
		checkGitCommand();
		checkFileCommand();
	});
	m->interval_250ms_timer.setInterval(1000);
	m->interval_250ms_timer.start();

	//

	startTimer(10);
}

void MainWindow::setCurrentLogRow(int row)
{
	if (row >= 0 && row < ui->tableWidget_log->rowCount()) {
		ui->tableWidget_log->setCurrentCell(row, 2);
	}
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
		}
	}

	return QMainWindow::event(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
	QEvent::Type et = event->type();

	if (m->ui_blocked) {
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
			if (k >= 0 && k < 128 && QChar((uchar)k).isLetterOrNumber()) {
				appendCharToRepoFilter(k);
				return true;
			}
			if (k == Qt::Key_Backspace) {
				backspaceRepoFilter();
				return true;
			}
			if (k == Qt::Key_Escape) {
				clearRepoFilter();
				return true;
			}
			if (k == Qt::Key_Tab) {
				ui->tableWidget_log->setFocus();
				return true;
			}
		} else if (watched == ui->tableWidget_log) {
			if (k == Qt::Key_Home) {
				setCurrentLogRow(0);
				return true;
			}
			if (k == Qt::Key_Escape) {
				ui->treeWidget_repos->setFocus();
				return true;
			}
			if (k == Qt::Key_Tab) {
				// consume the event
				return true;
			}
		} else if (watched == ui->listWidget_files || watched == ui->listWidget_unstaged || watched == ui->listWidget_staged) {
			if (k == Qt::Key_Escape) {
				ui->tableWidget_log->setFocus();
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
	m->status_bar_label->setText(text);
}

void MainWindow::clearStatusBarText()
{
	setStatusBarText(QString());
}

WebContext *MainWindow::getWebContextPtr()
{
	return &m->webcx;
}

QString MainWindow::getObjectID(QListWidgetItem *item)
{
	int i = indexOfDiff(item);
	if (i >= 0 && i < m->diff.result.size()) {
		Git::Diff const &diff = m->diff.result[i];
		return diff.blob.a_id;
	}
	return QString();
}

void MainWindow::onLogVisibilityChanged()
{
	ui->action_window_log->setChecked(ui->dockWidget_log->isVisible());
}

void MainWindow::writeLog(char const *ptr, int len)
{
	ui->widget_log->write(ptr, len);
}

void MainWindow::writeLog(const QString &str)
{
	std::string s = str.toStdString();
	writeLog(s.c_str(), s.size());
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
	return RepositoryBookmark::save(path, &m->repos);
}

void MainWindow::saveRepositoryBookmark(RepositoryItem item)
{
	if (item.local_dir.isEmpty()) return;

	if (item.name.isEmpty()) {
		item.name = tr("Unnamed");
	}

	bool done = false;
	for (int i = 0; i < m->repos.size(); i++) {
		RepositoryItem *p = &m->repos[i];
		if (item.local_dir == p->local_dir) {
			*p = item;
			done = true;
			break;
		}
	}
	if (!done) {
		m->repos.push_back(item);
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
	m->repos = std::move(newrepos);
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
	return m->digits;
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
	pr->drawPixmap(x, y, w, h, m->digits, n * w, 0, w, h);
}

QString MainWindow::defaultWorkingDir() const
{
	return m->appsettings.default_working_dir;
}

QColor MainWindow::color(unsigned int i)
{
	unsigned int n = m->graph_color.width();
	if (n > 0) {
		n--;
		if (i > n) i = n;
		QRgb const *p = (QRgb const *)m->graph_color.scanLine(0);
		return QColor(qRed(p[i]), qGreen(p[i]), qBlue(p[i]));
	}
	return Qt::black;
}

bool MainWindow::isThereUncommitedChanges() const
{
	return m->uncommited_changes;
}

Git::CommitItemList const *MainWindow::logs() const
{
	return &m->logs;
}

QString MainWindow::currentWorkingCopyDir() const
{
	return m->current_repo.local_dir;
}

GitPtr MainWindow::git(QString const &dir) const
{
	GitPtr g = std::shared_ptr<Git>(new Git(m->gcx, dir));
	g->setLogCallback(git_callback, (void *)this);
	return g;
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
		if (i >= 0 && i < m->repos.size()) {
			return i;
		}
	}
	return -1;
}

RepositoryItem const *MainWindow::repositoryItem(QTreeWidgetItem *item)
{
	int row = repositoryIndex_(item);
	if (row >= 0 && row < m->repos.size()) {
		return &m->repos[row];
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
	item->setIcon(0, m->folder_icon);
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	return item;
}

void MainWindow::updateRepositoriesList()
{
	QString path = getBookmarksFilePath();

	m->repos = RepositoryBookmark::load(path);

	QString filter = m->repository_filter_text;

	ui->treeWidget_repos->clear();

	std::map<QString, QTreeWidgetItem *> parentmap;

	for (int i = 0; i < m->repos.size(); i++) {
		RepositoryItem const &repo = m->repos[i];
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
		child->setIcon(0, m->repository_icon);
		child->setFlags(child->flags() & ~Qt::ItemIsDropEnabled);
		parent->addChild(child);
		parent->setExpanded(true);
	}
}

void MainWindow::showFileList(FilesListType files_list_type)
{
	switch (files_list_type) {
	case FilesListType::SingleList:
		ui->stackedWidget->setCurrentWidget(ui->page_files);
		break;
	case FilesListType::SideBySide:
		ui->stackedWidget->setCurrentWidget(ui->page_uncommited);
		break;
	}
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
	m->head_id = QString();
	m->current_branch = Git::Branch();
	m->server_type = ServerType::Standard;
	m->github = GitHubRepositoryInfo();
	ui->label_repo_name->setText(QString());
	ui->label_branch_name->setText(QString());

}

void MainWindow::setRepositoryInfo(QString const &reponame, QString const &brname)
{
	ui->label_repo_name->setText(reponame);
	ui->label_branch_name->setText(brname);
}

void MainWindow::setUnknownRepositoryInfo()
{
	setRepositoryInfo("---", "");

	Git g(m->gcx, QString());
	Git::User user = g.getUser(Git::GetUserGlobal);
	setWindowTitle_(user);
}

bool MainWindow::makeDiff(QString id, QList<Git::Diff> *out)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return false;

	Git::FileStatusList list = g->status();
	m->uncommited_changes = !list.empty();

	if (id.isEmpty() && !isThereUncommitedChanges()) {
		id = m->objcache.revParse("HEAD");
	}

	bool uncommited = (id.isEmpty() && isThereUncommitedChanges());

	GitDiff dm(&m->objcache);
	if (uncommited) {
		dm.diff_uncommited(out);
	} else {
		dm.diff(id, out);
	}

	return true; // success
}

void MainWindow::updateFilesList(QString id, bool wait)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	if (!wait) return;

	clearFileList();

	Git::FileStatusList stats = g->status();
	m->uncommited_changes = !stats.empty();

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
		if (!makeDiff(uncommited ? QString() : id, &m->diff.result)) {
			return;
		}

		std::map<QString, int> diffmap;

		for (int idiff = 0; idiff < m->diff.result.size(); idiff++) {
			Git::Diff const &diff = m->diff.result[idiff];
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
			} else if (s.isUnmerged()) {
				header += "(unmerged) ";
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
		if (!makeDiff(id, &m->diff.result)) {
			return;
		}

		showFileList(files_list_type);

		for (int idiff = 0; idiff < m->diff.result.size(); idiff++) {
			Git::Diff const &diff = m->diff.result[idiff];
			QString header;

			switch (diff.type) {
			case Git::Diff::Type::Modify:   header = "(chg) "; break;
			case Git::Diff::Type::Copy:     header = "(cpy) "; break;
			case Git::Diff::Type::Rename:   header = "(ren) "; break;
			case Git::Diff::Type::Create:   header = "(add) "; break;
			case Git::Diff::Type::Delete:   header = "(del) "; break;
			case Git::Diff::Type::ChType:   header = "(chg) "; break;
			case Git::Diff::Type::Unmerged: header = "(unmerged) "; break;
			}

			AddItem(diff.path, header, idiff, false);
		}
	}

	for (Git::Diff const &diff : m->diff.result) {
		QString key = GitDiff::makeKey(diff);
		m->diff_cache[key] = diff;
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
	int logs = (int)m->logs.size();
	if (row < logs) {
		updateFilesList(m->logs[row], true);
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
	return m->current_repo.name;
}

Git::Branch MainWindow::currentBranch() const
{
	return m->current_branch;
}

bool MainWindow::isValidWorkingCopy(GitPtr const &g) const
{
	return g && g->isValidWorkingCopy();
}

void MainWindow::queryBranches(GitPtr g)
{
	Q_ASSERT(g);
	m->branch_map.clear();
	QList<Git::Branch> branches = g->branches();
	for (Git::Branch const &b : branches) {
		if (b.flags & Git::Branch::Current) {
			m->current_branch = b;
		}
		m->branch_map[b.id].append(b);
	}
}

QString MainWindow::abbrevCommitID(Git::CommitItem const &commit)
{
	return commit.commit_id.mid(0, 7);
}

QString MainWindow::findFileID(GitPtr /*g*/, const QString &commit_id, const QString &file)
{
	return lookupFileID(&m->objcache, commit_id, file);
}

bool MainWindow::isGitHub() const
{
	return m->server_type == ServerType::GitHub;
}

void MainWindow::updateCommitTableLater()
{
	m->update_commit_table_counter = 200;
}

void MainWindow::onAvatarUpdated()
{
	updateCommitTableLater();
}

bool MainWindow::isAvatarEnabled() const
{
	return m->appsettings.get_committer_icon;
}

QIcon MainWindow::committerIcon(int row)
{
	QIcon icon;
	if (isAvatarEnabled()) {
		if (row >= 0 && row < (int)m->logs.size()) {
			Git::CommitItem const &commit = m->logs[row];
			if (commit.email.indexOf('@') > 0) {
				std::string email = commit.email.toStdString();
				icon = m->avatar_loader.fetch(email, true); // from gavatar
			}
		}
	}
	return icon;
}

QList<MainWindow::Label> const *MainWindow::label(int row)
{
	auto it = m->label_map.find(row);
	if (it != m->label_map.end()) {
		return &it->second;
	}
	return nullptr;
}

QString MainWindow::makeCommitInfoText(int row, QList<Label> *label_list)
{
	QString message_ex;
	Git::CommitItem const *commit = &m->logs[row];
	{ // branch
		if (label_list) {
			if (commit->commit_id == m->head_id) {
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

struct TemporaryCommitItem {
	Git::CommitItem const *commit;
	std::vector<TemporaryCommitItem *> children;
};

Git::CommitItemList MainWindow::retrieveCommitLog(GitPtr g)
{
	Git::CommitItemList list = g->log(limitLogCount());

	// 親子関係を調べて、順番が狂っていたら、修正する。

	std::set<QString> set;

	size_t i = 0;
	while (i < list.size()) {
		size_t newpos = -1;
		for (QString const &parent : list[i].parent_ids) {
			auto it = set.find(parent);
			if (it != set.end()) {
				for (size_t j = 0; j < i; j++) {
					if (parent == list[j].commit_id) {
						if (newpos == (size_t)-1 || j < newpos) {
							newpos = j;
						}
						qDebug() << "fix commit order" << parent;
						break;
					}
				}
			}
		}
		set.insert(set.end(), list[i].commit_id);
		if (newpos != (size_t)-1) {
			Git::CommitItem t = list[newpos];
			list.erase(list.begin() + newpos);
			list.insert(list.begin() + i, t);
		}
		i++;
	}

	return list;
}

void MainWindow::detectGitServerType(GitPtr g)
{
	m->server_type = ServerType::Standard;
	m->github = GitHubRepositoryInfo();

	QString push_url;
	QList<Git::Remote> remotes;
	g->getRemoteURLs(&remotes);
	for (Git::Remote const &r : remotes) {
		if (r.purpose == "push") {
			push_url = r.url;
		}
	}

	auto Check = [&](QString const &s){
		int i = push_url.indexOf(s);
		if (i > 0) return i + s.size();
		return 0;
	};

	// check GitHub
	int pos = Check("@github.com:");
	if (pos == 0) {
		pos = Check("://github.com/");
	}
	if (pos > 0) {
		int end = push_url.size();
		{
			QString s = ".git";
			if (push_url.endsWith(s)) {
				end -= s.size();
			}
		}
		QString s = push_url.mid(pos, end - pos);
		int i = s.indexOf('/');
		if (i > 0) {
			QString user = s.mid(0, i);
			QString repo = s.mid(i + 1);
			m->github.owner_account_name = user;
			m->github.repository_name = repo;
		}
		m->server_type = ServerType::GitHub;
	}
}

void MainWindow::fetch(GitPtr g)
{
	m->pty_condition = PtyCondition::Fetch;
	g->fetch(&m->pty_process);
	while (1) {
		if (m->pty_process.wait(1)) break;
		QApplication::processEvents();
	}
}

void MainWindow::clearLog()
{
	m->logs.clear();
	m->label_map.clear();
	m->uncommited_changes = false;
	ui->tableWidget_log->clearContents();
	ui->tableWidget_log->scrollToTop();
}

void MainWindow::openRepository_(GitPtr g)
{
	m->objcache.setup(g);

	if (isValidWorkingCopy(g)) {

		if (isRemoteOnline() && m->appsettings.automatically_fetch_when_opening_the_repository) {
			fetch(g);
		}

		clearLog();
		clearRepositoryInfo();

		detectGitServerType(g);

		updateFilesList(QString(), true);

		bool canceled = false;
		ui->tableWidget_log->setEnabled(false);

		// ログを取得
		m->logs = retrieveCommitLog(g);
		// ブランチを取得
		queryBranches(g);
		// タグを取得
		m->tag_map.clear();
		QList<Git::Tag> tags = g->tags();
		for (Git::Tag const &tag : tags) {
			Git::Tag t = tag;
			t.id = m->objcache.getCommitIdFromTag(t.id);
			m->tag_map[t.id].push_back(t);
		}

		ui->tableWidget_log->setEnabled(true);
		updateCommitTableLater();
		if (canceled) return;

		QString branch_name = currentBranch().name;
		if (currentBranch().flags & Git::Branch::HeadDetached) {
			branch_name = QString("(HEAD detached at %1)").arg(branch_name);
		}

		QString repo_name = currentRepositoryName();
		setRepositoryInfo(repo_name, branch_name);
	} else {
		clearLog();
		clearRepositoryInfo();
	}

	updateWindowTitle(g);

	m->head_id = m->objcache.revParse("HEAD");

	if (isThereUncommitedChanges()) {
		Git::CommitItem item;
		item.parent_ids.push_back(m->current_branch.id);
		item.message = tr("Uncommited changes");
		m->logs.insert(m->logs.begin(), item);
	}

	prepareLogTableWidget();

	int count = m->logs.size();

	ui->tableWidget_log->setRowCount(count);

	int selrow = -1;

	for (int row = 0; row < count; row++) {
		Git::CommitItem const *commit = &m->logs[row];
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
		bool isHEAD = commit->commit_id == m->head_id;
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
			message_ex = makeCommitInfoText(row, &m->label_map[row]);
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
	if (index >= 0 && index < m->repos.size()) {
		m->repos.erase(m->repos.begin() + index);
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
	dir.replace('\\', '/');

	auto Open = [&](RepositoryItem const &item){
		m->current_repo = item;
		openRepository(false);
	};

	for (RepositoryItem const &item : m->repos) {
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
		m->current_repo = *item;
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
			dlg.setText(m->logs[0].message);
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
			if (row < (int)m->logs.size()) {
				Git::CommitItem const &commit = m->logs[row];
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
	if (!isRemoteOnline()) return;

	reopenRepository(true, [&](GitPtr g){
		fetch(g);
	});
}

void MainWindow::on_action_push_triggered()
{
	if (!isRemoteOnline()) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	if (g->getRemotes().isEmpty()) {
		QMessageBox::warning(this, qApp->applicationName(), tr("No remote repository is registered."));
		execSetRemoteUrlDialog();
		return;
	}

	int exitcode = 0;
	QString errormsg;

	reopenRepository(true, [&](GitPtr g){
		g->push(false);
		exitcode = g->getProcessExitCode();
		errormsg = g->errorMessage();
	});

	if (exitcode == 128) {
		if (errormsg.indexOf("no upstream branch") >= 0) {
			QString brname = currentBranch().name;

			QString msg = tr("The current branch %1 has no upstream branch.");
			msg = msg.arg(brname);
			msg += '\n';
			msg += tr("You try push --set-upstream");
			QMessageBox::warning(this, qApp->applicationName(), msg);
			pushSetUpstream(false);
			return;
		}
		if (errormsg.indexOf("Connection refused") >= 0) {
			QMessageBox::critical(this, qApp->applicationName(), tr("Connection refused."));
			return;
		}
		qDebug() << errormsg;
	}
}

void MainWindow::on_action_pull_triggered()
{
	if (!isRemoteOnline()) return;

	reopenRepository(true, [&](GitPtr g){
		m->pty_condition = PtyCondition::Pull;
		g->pull(&m->pty_process);
		while (1) {
			if (m->pty_process.wait(1)) break;
			QApplication::processEvents();
		}
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
	Git git(m->gcx, currentWorkingCopyDir());
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

QAction *MainWindow::addMenuActionProperty(QMenu *menu)
{
	return menu->addAction(tr("&Property"));
}

int MainWindow::indexOfRepository(QTreeWidgetItem const *treeitem) const
{
	if (!treeitem) return -1;
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
		QAction *a_properties = addMenuActionProperty(&menu);

		QPoint pt = ui->treeWidget_repos->mapToGlobal(pos);
		QAction *a = menu.exec(pt + QPoint(8, -8));
		if (a) {
			if (a == a_open) {
				openSelectedRepository();
				return;
			}
			if (a == a_open_folder) {
				openExplorer();
				return;
			}
			if (a == a_open_terminal) {
				openTerminal();
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
				execSetRemoteUrlDialog();
				return;
			}
		}
	}
}

void MainWindow::execSetRemoteUrlDialog(RepositoryItem const *repo)
{
	QTreeWidgetItem *treeitem = ui->treeWidget_repos->currentItem();
	if (!treeitem) return;

	repo = repositoryItem(treeitem);
	if (!repo) return;

	GitPtr g = git(repo->local_dir);
	if (!isValidWorkingCopy(g)) return;
	QStringList remotes = g->getRemotes();
	SetRemoteUrlDialog dlg(this, remotes, g);
	dlg.exec();
}

void MainWindow::on_tableWidget_log_customContextMenuRequested(const QPoint &pos)
{
	Git::CommitItem const *commit = selectedCommitItem();
	if (commit) {
		int row = selectedLogIndex();

		QMenu menu;

		QAction *a_push_upstream = nullptr;
		if (pushSetUpstream(true)) {
			a_push_upstream = menu.addAction("push --set-upstream ...");
		}

		QAction *a_edit_comment = nullptr;
		if (row == 0 && currentBranch().ahead > 0) {
			a_edit_comment = menu.addAction(tr("Edit comment..."));
		}

		bool is_valid_commit_id = Git::isValidID(commit->commit_id);

		QAction *a_checkout = is_valid_commit_id ? menu.addAction(tr("Checkout/Branch...")) : nullptr;
		QAction *a_delbranch = is_valid_commit_id ? menu.addAction(tr("Delete branch...")) : nullptr;
		QAction *a_merge = is_valid_commit_id ? menu.addAction(tr("Merge")) : nullptr;
		QAction *a_cherrypick = is_valid_commit_id ? menu.addAction(tr("Cherry-pick")) : nullptr;

		QAction *a_add_tag = is_valid_commit_id ? menu.addAction(tr("Add a tag...")) : nullptr;
		QAction *a_delete_tags = nullptr;
		if (is_valid_commit_id && m->tag_map.find(commit->commit_id) != m->tag_map.end()) {
			a_delete_tags = menu.addAction(tr("Delete tags..."));
		}
		QAction *a_revert = is_valid_commit_id ? menu.addAction(tr("Revert")) : nullptr;
		QAction *a_explore = is_valid_commit_id ? menu.addAction(tr("Explore")) : nullptr;

		QAction *a_reset_head = nullptr;
#if 0 // 下手に使うと危険なので、とりあえず無効にしておく
		if (is_valid_commit_id && commit->commit_id == m->head_id) {
			a_reset_head = menu.addAction(tr("Reset HEAD"));
		}
#endif
		QAction *a_properties = addMenuActionProperty(&menu);

		QAction *a = menu.exec(ui->tableWidget_log->viewport()->mapToGlobal(pos) + QPoint(8, -8));
		if (a) {
			if (a == a_properties) {
				execCommitPropertyDialog(commit);
				return;
			}
			if (a == a_push_upstream) {
				pushSetUpstream(false);
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
			if (a == a_merge) {
				mergeBranch(commit);
				return;
			}
			if (a == a_cherrypick) {
				cherrypick(commit);
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
			if (a == a_revert) {
				revertCommit();
				return;
			}
			if (a == a_explore) {
				CommitExploreWindow win(this, &m->objcache, commit);
				win.exec();
				return;
			}
			if (a == a_reset_head) {
				reopenRepository(false, [](GitPtr g){
					g->reset_head1();
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
	QAction *a_delete = menu.addAction(tr("Delete"));
	QAction *a_untrack = menu.addAction(tr("Untrack"));
	QAction *a_history = menu.addAction(tr("History"));
	QAction *a_properties = addMenuActionProperty(&menu);

	QPoint pt = ui->listWidget_unstaged->mapToGlobal(pos) + QPoint(8, -8);
	QAction *a = menu.exec(pt);
	if (a) {
		QListWidgetItem *item = ui->listWidget_files->currentItem();
		if (a == a_delete) {
			if (askAreYouSureYouWantToRun("Delete", tr("Delete selected files."))) {
				for_each_selected_files([&](QString const &path){
					g->removeFile(path);
					g->chdirexec([&](){
						QFile(path).remove();
						return true;
					});
				});
				openRepository(false);
			}
		} else if (a == a_untrack) {
			if (askAreYouSureYouWantToRun("Untrack", tr("rm --cached files"))) {
				for_each_selected_files([&](QString const &path){
					g->rm_cached(path);
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

void MainWindow::on_listWidget_unstaged_customContextMenuRequested(const QPoint &pos)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QList<QListWidgetItem *> items = ui->listWidget_unstaged->selectedItems();
	if (!items.isEmpty()) {
		QMenu menu;
		QAction *a_stage = menu.addAction(tr("Stage"));
		QAction *a_reset_file = menu.addAction(tr("Reset"));
		QAction *a_ignore = menu.addAction(tr("Ignore"));
		QAction *a_delete = menu.addAction(tr("Delete"));
		QAction *a_untrack = menu.addAction(tr("Untrack"));
		QAction *a_history = menu.addAction(tr("History"));
		QAction *a_properties = addMenuActionProperty(&menu);
		QPoint pt = ui->listWidget_unstaged->mapToGlobal(pos) + QPoint(8, -8);
		QAction *a = menu.exec(pt);
		if (a) {
			QListWidgetItem *item = ui->listWidget_unstaged->currentItem();
			if (a == a_stage) {
				for_each_selected_files([&](QString const &path){
					g->stage(path);
				});
				updateCurrentFilesList();
			} else if (a == a_reset_file) {
				QStringList paths;
				for_each_selected_files([&](QString const &path){
					paths.push_back(path);
				});
				resetFile(paths);
			} else if (a == a_ignore) {
				QString append;
				for_each_selected_files([&](QString const &path){
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
					for_each_selected_files([&](QString const &path){
						g->removeFile(path);
						g->chdirexec([&](){
							QFile(path).remove();
							return true;
						});
					});
					openRepository(false);
				}
			} else if (a == a_untrack) {
				if (askAreYouSureYouWantToRun("Untrack", "rm --cached")) {
					for_each_selected_files([&](QString const &path){
						g->rm_cached(path);
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
			QAction *a_properties = addMenuActionProperty(&menu);
			QPoint pt = ui->listWidget_staged->mapToGlobal(pos) + QPoint(8, -8);
			QAction *a = menu.exec(pt);
			if (a) {
				QListWidgetItem *item = ui->listWidget_unstaged->currentItem();
				if (a == a_unstage) {
					g->unstage(path);
					openRepository(false);
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

void MainWindow::resetFile(QStringList const &paths)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	if (paths.isEmpty()) {
		// nop
	} else {
		QString cmd = "git checkout -- \"%1\"";
		cmd = cmd.arg(paths[0]);
		if (askAreYouSureYouWantToRun(tr("Reset a file"), "> " + cmd)) {
			for (QString const &path : paths) {
				g->resetFile(path);
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
		g->resetAllFiles();
		openRepository(true);
	}
}

QStringList MainWindow::selectedFiles_(QListWidget *listwidget) const
{
	QStringList list;
	QList<QListWidgetItem *> items = listwidget->selectedItems();
	for (QListWidgetItem *item : items) {
		QString path = getFilePath(item);
		list.push_back(path);
	}
	return list;

}

QStringList MainWindow::selectedFiles() const
{
	QWidget *w = QWidget::focusWidget();
	if (w == ui->listWidget_files) return selectedFiles_(ui->listWidget_files);
	if (w == ui->listWidget_staged) return selectedFiles_(ui->listWidget_staged);
	if (w == ui->listWidget_unstaged) return selectedFiles_(ui->listWidget_unstaged);
	return QStringList();
}

void MainWindow::for_each_selected_files(std::function<void(QString const&)> fn)
{
	for (QString path : selectedFiles()) {
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

void MainWindow::addWorkingCopyDir(QString dir, QString name, bool open)
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

	if (name.isEmpty()) {
		name = makeRepositoryName(dir);
	}

	RepositoryItem item;
	item.local_dir = dir;
	item.name = name;
	saveRepositoryBookmark(item);

	if (open) {
		m->current_repo = item;
		GitPtr g = git(item.local_dir);
		openRepository_(g);
	}
}

void MainWindow::on_action_open_existing_working_copy_triggered()
{
	QString dir = defaultWorkingDir();
	dir = QFileDialog::getExistingDirectory(this, tr("Add existing working copy"), dir);
	addWorkingCopyDir(dir, false);
}

void MainWindow::on_action_view_refresh_triggered()
{
	openRepository(true);
}

void MainWindow::on_tableWidget_log_currentItemChanged(QTableWidgetItem * /*current*/, QTableWidgetItem * /*previous*/)
{
	clearFileList();

	QTableWidgetItem *item = ui->tableWidget_log->item(selectedLogIndex(), 0);
	if (!item) return;

	int row = item->data(IndexRole).toInt();
	if (row < (int)m->logs.size()) {
		updateStatusBarText();
		m->update_files_list_counter = 200;
	}
}

void MainWindow::on_toolButton_stage_clicked()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	g->stage(selectedFiles());
	updateCurrentFilesList();
}

void MainWindow::on_toolButton_unstage_clicked()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	g->unstage(selectedFiles());
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
	const size_t LogCount = m->logs.size();
	// 樹形図情報を構築する
	if (LogCount > 0) {
		auto LogItem = [&](size_t i)->Git::CommitItem &{ return m->logs[i]; };
		enum { // 有向グラフを構築するあいだ CommitItem::marker_depth をフラグとして使用する
			UNKNOWN = 0,
			KNOWN = 1,
		};
		for (Git::CommitItem &item : m->logs) {
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
		for (Git::CommitItem &item : m->logs) {
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
	auto it = m->branch_map.find(id);
	if (it != m->branch_map.end()) {
		return it->second;
	}
	return QList<Git::Branch>();
}

QList<Git::Tag> MainWindow::findTag(QString const &id)
{
	auto it = m->tag_map.find(id);
	if (it != m->tag_map.end()) {
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
	if (i >= 0 && i < (int)m->logs.size()) {
		return i;
	}
	return -1;
}

Git::CommitItem const *MainWindow::selectedCommitItem() const
{
	int i = selectedLogIndex();
	if (i >= 0 && i < (int)m->logs.size()) {
		return &m->logs[i];
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
			return m->objcache.catFile(id);;
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

	if (!item) return;

	int idiff = indexOfDiff(item);
	if (idiff >= 0 && idiff < m->diff.result.size()) {
		Git::Diff const &diff = m->diff.result[idiff];
		QString key = GitDiff::makeKey(diff);
		auto it = m->diff_cache.find(key);
		if (it != m->diff_cache.end()) {
			int row = ui->tableWidget_log->currentRow();
			bool uncommited = (row >= 0 && row < (int)m->logs.size() && Git::isUncommited(m->logs[row]));
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
	m->gcx.git_command = path;

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
	m->file_command = path;
}


#ifdef Q_OS_WIN
QString getWin32HttpProxy();
#endif

void MainWindow::initNetworking()
{
	std::string http_proxy;
	std::string https_proxy;
	if (m->appsettings.proxy_type == "auto") {
#ifdef Q_OS_WIN
		http_proxy = misc::makeProxyServerURL(getWin32HttpProxy().toStdString());
#else
		auto getienv = [](std::string const &name)->char const *{
			char **p = environ;
			while (*p) {
				if (strncasecmp(*p, name.c_str(), name.size()) == 0) {
					char const *e = *p + name.size();
					if (*e == '=') {
						return e + 1;
					}
				}
				p++;
			}
			return nullptr;
		};
		char const *p;
		p = getienv("http_proxy");
		if (p) {
			http_proxy = misc::makeProxyServerURL(std::string(p));
		}
		p = getienv("https_proxy");
		if (p) {
			https_proxy = misc::makeProxyServerURL(std::string(p));
		}
#endif
	} else if (m->appsettings.proxy_type == "manual") {
		http_proxy = m->appsettings.proxy_server.toStdString();
	}
	m->webcx.set_http_proxy(http_proxy);
	m->webcx.set_https_proxy(https_proxy);
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

QString MainWindow::selectGitCommand(bool save)
{
#ifdef Q_OS_WIN
	char const *exe = "git.exe";
#else
	char const *exe = "git";
#endif
	QString path = m->gcx.git_command;

	auto fn = [&](QString const &path){
		setGitCommand(path, save);
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
	QString path = m->file_command;

	auto fn = [&](QString const &path){
		setFileCommand(path, true);
	};

	QStringList list = whichCommand_(exe);

#ifdef Q_OS_WIN
	QString dir = misc::getApplicationDir();
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
		QFileInfo info(m->gcx.git_command);
		if (info.isExecutable()) {
			break; // ok
		}
		if (selectGitCommand(true).isEmpty()) {
			close();
			break;
		}
	}
}

void MainWindow::checkFileCommand()
{
	while (1) {
		QFileInfo info(m->file_command);
		if (info.isExecutable()) {
			break; // ok
		}
		if (selectFileCommand().isEmpty()) {
			close();
			break;
		}
	}
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
	if (QApplication::modalWindow()) return;

	if (event->mimeData()->hasUrls()) {
		event->acceptProposedAction();
		event->accept();
	}
}

void MainWindow::timerEvent(QTimerEvent *)
{
	while (1) {
		char tmp[1024];
		int len = m->pty_process.readOutput(tmp, sizeof(tmp));
		if (len < 1) break;
		ui->widget_log->write(tmp, len);
	}
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
	if (focusWidget() == ui->widget_log) {
		int c = event->key();

		auto write_char = [&](char c){
			if (m->pty_process.isRunning()) {
				m->pty_process.writeInput(&c, 1);
			}
		};

		auto write_text = [&](QString const &str){
			std::string s = str.toStdString();
			for (int i = 0; i < (int)s.size(); i++) {
				write_char(s[i]);
			}
		};

		if (c == Qt::Key_Return || c == Qt::Key_Enter) {
			write_char('\n');
		} else {
			QString text = event->text();
			write_text(text);
		}
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
		ApplicationSettings const &newsettings = dlg.settings();
		m->appsettings = newsettings;
		setGitCommand(m->appsettings.git_command, false);
		setFileCommand(m->appsettings.file_command, false);
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

bool MainWindow::git_callback(void *cookie, char const *ptr, int len)
{
	MainWindow *mw = (MainWindow *)cookie;
	mw->writeLog(ptr, len);
	return true;
}

void MainWindow::clone()
{
	if (!isRemoteOnline()) return;

	QString url;
	QString dir = defaultWorkingDir();
	while (1) {
		CloneDialog dlg(this, url, dir);
		if (dlg.exec() != QDialog::Accepted) {
			return;
		}

		url = dlg.url();
		dir = dlg.dir();

		if (dlg.action() == CloneDialog::Action::Clone) {
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

			m->temp_repo.local_dir = dir;
			m->temp_repo.local_dir.replace('\\', '/');
			m->temp_repo.name = makeRepositoryName(dir);

			GitPtr g = git(QString());
			m->pty_condition = PtyCondition::Clone;
			g->clone(clone_data, &m->pty_process);

		} else if (dlg.action() == CloneDialog::Action::AddExisting) {
			addWorkingCopyDir(dir, true);
		}

		return; // done
	}
}

void MainWindow::onCloneCompleted()
{
	saveRepositoryBookmark(m->temp_repo);
	m->current_repo = m->temp_repo;
	openRepository(true);
}

void MainWindow::onPtyProcessCompleted()
{
	switch (m->pty_condition) {
	case PtyCondition::Clone:
		onCloneCompleted();
		break;
	}
	m->pty_condition = PtyCondition::None;
}

void MainWindow::on_action_clone_triggered()
{
	clone();
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

void MainWindow::clearRepoFilter()
{
	ui->lineEdit_filter->clear();
}

void MainWindow::appendCharToRepoFilter(ushort c)
{
	if (QChar(c).isLetter()) {
		c = QChar(c).toLower().unicode();
	}
	ui->lineEdit_filter->setText(m->repository_filter_text + c);
}

void MainWindow::backspaceRepoFilter()
{
	QString s = m->repository_filter_text;
	int n = s.size();
	if (n > 0) {
		s = s.mid(0, n - 1);
	}
	ui->lineEdit_filter->setText(s);
}

void MainWindow::on_lineEdit_filter_textChanged(const QString &text)
{
	m->repository_filter_text = text;
	updateRepositoriesList();
}

void MainWindow::on_toolButton_erase_filter_clicked()
{
	clearRepoFilter();
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
	auto it = m->tag_map.find(commit.commit_id);
	if (it != m->tag_map.end()) {
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

void MainWindow::revertCommit()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	Git::CommitItem const *commit = selectedCommitItem();
	if (commit) {
		g->revert(commit->commit_id);
		openRepository(false);
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
	QString path = tmpdir / tempfileHeader() + QString::number(m->temp_file_counter);
	m->temp_file_counter++;
	return path;
}

QString MainWindow::determinFileType_(QString const &path, bool mime, std::function<void(QString const &cmd, QByteArray *ba)> callback)
{
	return misc::determinFileType(m->file_command, path, mime, callback);
}

QString MainWindow::determinFileType(QString const &path, bool mime)
{
	return determinFileType_(path, mime, [](QString const &cmd, QByteArray *ba){
		misc::runCommand(cmd, ba);
	});
}

QString MainWindow::determinFileType(QByteArray in, bool mime)
{
	if (in.isEmpty()) return QString();

	if (in.size() > 10) {
		if (memcmp(in.data(), "\x1f\x8b\x08", 3) == 0) { // gzip
			QBuffer buf;
			MemoryReader reader(in.data(), in.size());
			reader.open(MemoryReader::ReadOnly);
			buf.open(QBuffer::WriteOnly);
			gunzip z;
			z.set_maximul_size(100000);
			z.decode(&reader, &buf);
			in = buf.buffer();
		}
	}

	// ファイル名を "-" にすると、リダイレクトで標準入力へ流し込める。
	return determinFileType_("-", mime, [&](QString const &cmd, QByteArray *ba){
		int r = misc::runCommand(cmd, &in, ba);
		if (r != 0) {
			ba->clear();
		}
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
	if (m->transparent_pixmap.isNull()) {
		m->transparent_pixmap = QPixmap(":/image/transparent.png");
	}
	return m->transparent_pixmap;
}

QString MainWindow::getCommitIdFromTag(QString const &tag)
{
	return m->objcache.getCommitIdFromTag(tag);
}

bool MainWindow::isValidRemoteURL(const QString &url)
{
	if (url.indexOf('\"') >= 0) {
		return false;
	}
	Git g(m->gcx, QString());
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

bool MainWindow::testRemoteRepositoryValidity(QString const &url)
{
	bool ok;
	{
		OverrideWaitCursor;
		ok = isValidRemoteURL(url);
	}

	QString pass = tr("The URL is a valid repository");
	QString fail = tr("Failed to access the URL");

	QString text = "%1\n\n%2";
	text = text.arg(url).arg(ok ? pass : fail);

	QString title = tr("Remote Repository");

	if (ok) {
		QMessageBox::information(this, title, text);
	} else {
		QMessageBox::critical(this, title, text);
	}

	return ok;
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
	m->ui_blocked = f;
	ui->menuBar->setEnabled(!m->ui_blocked);
}

NamedCommitList MainWindow::namedCommitItems(int flags)
{
	NamedCommitList items;
	if (flags & Branches) {
		for (auto pair: m->branch_map) {
			QList<Git::Branch> const &list = pair.second;
			for (Git::Branch const &b : list) {
				if (b.isHeadDetached()) continue;
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
		for (auto pair: m->tag_map) {
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

int MainWindow::rowFromCommitId(QString const &id)
{
	for (size_t i = 0; i < m->logs.size(); i++) {
		Git::CommitItem const &item = m->logs[i];
		if (item.commit_id == id) {
			return (int)i;
		}
	}
	return -1;
}

void MainWindow::on_action_repo_jump_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	NamedCommitList items = namedCommitItems(Branches | Tags);
	JumpDialog::sort(&items);
	{
		NamedCommitItem head;
		head.name = "HEAD";
		head.id = m->head_id;
		items.push_front(head);
	}
	JumpDialog dlg(this, items);
	if (dlg.exec() == QDialog::Accepted) {
		JumpDialog::Action action = dlg.action();
		if (action == JumpDialog::Action::BranchsAndTags) {
			QString name = dlg.text();
			QString id = g->rev_parse(name);
			if (g->objectType(id) == "tag") {
				id = m->objcache.getCommitIdFromTag(id);
			}
			int row = rowFromCommitId(id);
			if (row < 0) {
				QMessageBox::warning(this, tr("Jump"), QString("%1\n(%2)\n\n").arg(name).arg(id) + tr("That commmit has not foud or not read yet"));
			} else {
				setCurrentLogRow(row);

				if (dlg.isCheckoutChecked()) {
					NamedCommitItem item;
					for (NamedCommitItem const &t : items) {
						if (t.name == name) {
							item = t;
							break;
						}
					}
					bool ok = false;
					if (item.type == NamedCommitItem::Type::Branch) {
						ok = g->git(QString("checkout \"%1\"").arg(name), true);
					} else if (item.type == NamedCommitItem::Type::Tag) {
						ok = g->git(QString("checkout -b \"%1\" \"%2\"").arg(name).arg(id), true);
					}
					if (ok) {
						openRepository(true);
					}
				}
			}
		} else if (action == JumpDialog::Action::CommitId) {
			QString id = dlg.text();
			id = g->rev_parse(id);
			if (!id.isEmpty()) {
				int row = rowFromCommitId(id);
				setCurrentLogRow(row);
			}
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

void MainWindow::mergeBranch(Git::CommitItem const *commit)
{
	if (!commit) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	g->mergeBranch(commit->commit_id);
	openRepository(true);
}

void MainWindow::cherrypick(Git::CommitItem const *commit)
{
	if (!commit) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	g->cherrypick(commit->commit_id);
	openRepository(true);
}

void MainWindow::checkout()
{
	checkout(selectedCommitItem());
}

void MainWindow::deleteBranch()
{
	deleteBranch(selectedCommitItem());
}

void MainWindow::on_action_repo_checkout_triggered()
{
	checkout();
}

void MainWindow::on_action_delete_branch_triggered()
{
	deleteBranch();
}

void MainWindow::on_action_push_u_origin_master_triggered()
{
	reopenRepository(true, [&](GitPtr g){
		g->push_u_origin_master();
	});
}

bool MainWindow::runOnCurrentRepositoryDir(std::function<void(QString)> callback)
{
	RepositoryItem const *repo = &m->current_repo;
	QString dir = repo->local_dir;
	dir.replace('\\', '/');
	if (QFileInfo(dir).isDir()) {
		callback(dir);
		return true;
	}
	QMessageBox::warning(this, qApp->applicationName(), tr("No repository selected"));
	return false;
}

#ifdef Q_OS_MAC
namespace {

bool isValidDir(QString const &dir)
{
	if (dir.indexOf('\"') >= 0 || dir.indexOf('\\') >= 0) return false;
	return QFileInfo(dir).isDir();
}

}
#endif

void MainWindow::openTerminal()
{
	runOnCurrentRepositoryDir([](QString dir){
#ifdef Q_OS_MAC
		if (!isValidDir(dir)) return;
		QString cmd = "open -n -a /Applications/Utilities/Terminal.app --args \"%1\"";
		cmd = cmd.arg(dir);
		QProcess::execute(cmd);
#else
		Terminal::open(dir);
#endif
	});
}

void MainWindow::openExplorer()
{
	runOnCurrentRepositoryDir([](QString dir){
#ifdef Q_OS_MAC
		if (!isValidDir(dir)) return;
		QString cmd = "open \"%1\"";
		cmd = cmd.arg(dir);
		QProcess::execute(cmd);
#else
		QDesktopServices::openUrl(dir);
#endif
	});
}

void MainWindow::on_toolButton_terminal_clicked()
{
	openTerminal();
}

void MainWindow::on_toolButton_explorer_clicked()
{
	openExplorer();
}


void MainWindow::on_action_push_u_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	reopenRepository(true, [&](GitPtr g){
		g->push_u_origin_master();
	});
}

void MainWindow::pushSetUpstream(QString const &remote, QString const &branch)
{
	if (remote.isEmpty()) return;
	if (branch.isEmpty()) return;

	reopenRepository(true, [&](GitPtr g){
		g->push_u(remote, branch);
	});
}

bool MainWindow::pushSetUpstream(bool testonly)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return false;
	QStringList remotes = g->getRemotes();

	QStringList branches;
	int row = ui->tableWidget_log->currentRow();
	QList<Label> const *labels = label(row);
	for (Label const &label : *labels) {
		if (label.kind == Label::LocalBranch) {
			QString branch = label.text;
			branches.push_back(branch);
		}
	}

	if (remotes.isEmpty() || branches.isEmpty()) {
		return false;
	}

	if (testonly) {
		return true;
	}

	PushDialog dlg(this, remotes, branches);
	if (dlg.exec() == QDialog::Accepted) {
		PushDialog::Action a = dlg.action();
		if (a == PushDialog::PushSimple) {
			ui->action_push->trigger();
		} else if (a == PushDialog::PushSetUpstream) {
			QString remote = dlg.remote();
			QString branch = dlg.branch();
			pushSetUpstream(remote, branch);
		}
		return true;
	}

	return false;
}

void MainWindow::on_action_reset_HEAD_1_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	g->reset_head1();
	openRepository(false);
}

void MainWindow::on_action_create_a_repository_triggered()
{
	CreateRepositoryDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		QString path = dlg.path();
		if (QFileInfo(path).isDir()) {
			if (Git::isValidWorkingCopy(path)) {
				// A valid git repository already exists there.
			} else {
				GitPtr g = git(path);
				if (g->init()) {
					QString name = dlg.name();
					if (!name.isEmpty()) {
						addWorkingCopyDir(path, name, true);
					}
					QString url = dlg.remoteURL();
					if (!url.isEmpty()) {
						QString remote = "origin";
						g->addRemoteURL(remote, url);
					}
				}
			}
		} else {
			// not dir
		}
	}
}

bool MainWindow::isRemoteOnline() const
{
	return ui->radioButton_remote_online->isChecked();
}

void MainWindow::setRemoteOnline(bool f)
{
	QRadioButton *rb = nullptr;
	rb = f ? ui->radioButton_remote_online : ui->radioButton_remote_offline;
	rb->blockSignals(true);
	rb->click();
	rb->blockSignals(false);

	ui->toolButton_clone->setEnabled(f);
	ui->toolButton_fetch->setEnabled(f);
	ui->toolButton_pull->setEnabled(f);
	ui->toolButton_push->setEnabled(f);

	MySettings s;
	s.beginGroup("Remote");
	s.setValue("Online", f);
	s.endGroup();
}

void MainWindow::on_radioButton_remote_online_clicked()
{
	setRemoteOnline(true);
}

void MainWindow::on_radioButton_remote_offline_clicked()
{
	setRemoteOnline(false);
}

void MainWindow::on_verticalScrollBar_log_valueChanged(int)
{
	ui->widget_log->refrectScrollBar();
}

void MainWindow::on_horizontalScrollBar_log_valueChanged(int)
{
	ui->widget_log->refrectScrollBar();

}

void MainWindow::on_action_test_triggered()
{
}
