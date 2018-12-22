#include "AreYouSureYouWantToContinueConnectingDialog.h"
#include "BlameWindow.h"
#include "CommitViewWindow.h"
#include "Git.h"
#include "LineEditDialog.h"
#include "MainWindow.h"
#include "ReflogWindow.h"
#include "SetGlobalUserDialog.h"
#include "EditTagsDialog.h"
#include "WelcomeWizardDialog.h"
#include "ui_MainWindow.h"
#include "EditGitIgnoreDialog.h"
#include "DoYouWantToInitDialog.h"
#include "RemoteWatcher.h"
#include "ApplicationGlobal.h"
#include "AboutDialog.h"
#include "AvatarLoader.h"
#include "CheckoutDialog.h"
#include "CloneDialog.h"
#include "CommitExploreWindow.h"
#include "CommitPropertyDialog.h"
#include "common/joinpath.h"
#include "common/misc.h"
#include "ConfigCredentialHelperDialog.h"
#include "CreateRepositoryDialog.h"
#include "DeleteBranchDialog.h"
#include "DeleteTagsDialog.h"
#include "InputNewTagDialog.h"
#include "FileHistoryWindow.h"
#include "FilePropertyDialog.h"
#include "FileUtil.h"
#include "Git.h"
#include "GitDiff.h"
#include "gunzip.h"
#include "JumpDialog.h"
#include "LocalSocketReader.h"
#include "main.h"
#include "MemoryReader.h"
#include "MergeBranchDialog.h"
#include "MyProcess.h"
#include "MySettings.h"
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
#include "CommitDialog.h"
#include "SelectGpgKeyDialog.h"
#include "SetGpgSigningDialog.h"
#include "gpg.h"
#include "webclient.h"
#include "RebaseOntoDialog.h"
#include <QBuffer>
#include <QClipboard>
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
#include <deque>
#include <set>
#include <stdlib.h>

#ifdef Q_OS_WIN
#include "win32/win32.h"
#else
#include <unistd.h>
#endif


#ifdef Q_OS_MAC
extern "C" char **environ;
#endif

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
	void run() override
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
	QTimer interval_10ms_timer;
	QTimer remote_watcher_timer;
	QImage graph_color;
	QPixmap digits;
	StatusLabel *status_bar_label;

	QObject *last_focused_file_list = nullptr;

	QListWidgetItem *last_selected_file_item = nullptr;

	RemoteWatcher remote_watcher;
};

MainWindow::MainWindow(QWidget *parent)
	: BasicMainWindow(parent)
	, ui(new Ui::MainWindow)
	, m(new Private)
{
	ui->setupUi(this);

#ifdef Q_OS_MACX
	ui->action_about->setText("About Guitar...");
	ui->action_edit_settings->setText("Settings...");
#endif

	starting_dir_ = QDir::current().absolutePath();

	ui->splitter_v->setSizes({100, 400});
	ui->splitter_h->setSizes({200, 100, 200});

	m->status_bar_label = new StatusLabel(this);
	ui->statusBar->addWidget(m->status_bar_label);

	ui->widget_diff_view->bind(this);

	qApp->installEventFilter(this);

	ui->widget_log->setupForLogWidget(ui->verticalScrollBar_log, ui->horizontalScrollBar_log, themeForTextEditor());
	onLogVisibilityChanged();

	SettingsDialog::loadSettings(&appsettings_);

	initNetworking();

	showFileList(FilesListType::SingleList);

	QFileIconProvider icons;

	m->digits.load(":/image/digits.png");
	m->graph_color = global->theme->graphColorMap();

	repository_icon_ = QIcon(":/image/repository.png");
	folder_icon_ = icons.icon(QFileIconProvider::Folder);

	signature_good_icon_ = QIcon(":/image/signature-good.png");
	signature_bad_icon_ = QIcon(":/image/signature-bad.png");
	signature_dubious_icon_ = QIcon(":/image/signature-dubious.png");

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

	connect(this, SIGNAL(signalWriteLog(QByteArray)), this, SLOT(writeLog(QByteArray)));

	connect(ui->dockWidget_log, SIGNAL(visibilityChanged(bool)), this, SLOT(onLogVisibilityChanged()));
	connect(ui->widget_log, SIGNAL(idle()), this, SLOT(onLogIdle()));

	connect(ui->treeWidget_repos, SIGNAL(dropped()), this, SLOT(onRepositoriesTreeDropped()));

	connect((AbstractPtyProcess *)&pty_process_, SIGNAL(completed()), this, SLOT(onPtyProcessCompleted()));

	connect(this, &BasicMainWindow::remoteInfoChanged, [&](){
		ui->lineEdit_remote->setText(current_remote_);
	});

	// リモート監視
	connect(this, SIGNAL(signalCheckRemoteUpdate()), &m->remote_watcher, SLOT(checkRemoteUpdate()));
	connect(&m->remote_watcher_timer, SIGNAL(timeout()), &m->remote_watcher, SLOT(checkRemoteUpdate()));
	connect(this, &MainWindow::updateButton, [&](){
		doUpdateButton();
	});
	m->remote_watcher.start(this);
	setWatchRemoteInterval(appsettings()->watch_remote_changes_every_mins);

	//

	QString path = getBookmarksFilePath();
	repos_ = RepositoryBookmark::load(path);
	updateRepositoriesList();

	webcx_.set_keep_alive_enabled(true);
	avatar_loader_.start(&webcx_);
	connect(&avatar_loader_, SIGNAL(updated()), this, SLOT(onAvatarUpdated()));

	update_files_list_counter_ = 0;

	connect(ui->widget_diff_view, &FileDiffWidget::textcodecChanged, [&](){ updateDiffView(); });

	if (!global->start_with_shift_key && appsettings()->remember_and_restore_window_position) {
		Qt::WindowStates state = windowState();
		MySettings settings;

		settings.beginGroup("MainWindow");
		bool maximized = settings.value("Maximized").toBool();
		restoreGeometry(settings.value("Geometry").toByteArray());
//		ui->splitter->restoreState(settings.value("SplitterState").toByteArray());
		settings.endGroup();
		if (maximized) {
			state |= Qt::WindowMaximized;
			setWindowState(state);
		}
	}

	startTimers();
}

MainWindow::~MainWindow()
{
	stopPtyProcess();

	avatar_loader_.stop();

	m->remote_watcher.quit();
	m->remote_watcher.wait();

	deleteTempFiles();

	delete m;
	delete ui;
}

bool BasicMainWindow::checkGitCommand()
{
	while (1) {
		if (misc::isExecutable(gcx_.git_command)) {
			return true;
		}
		if (selectGitCommand(true).isEmpty()) {
			close();
			return false;
		}
	}
}

bool BasicMainWindow::checkFileCommand()
{
	while (1) {
		if (misc::isExecutable(global->file_command)) {
			return true;
		}
		if (selectFileCommand(true).isEmpty()) {
			close();
			return false;
		}
	}
}

ApplicationSettings *BasicMainWindow::appsettings()
{
	return &appsettings_;
}

ApplicationSettings const *BasicMainWindow::appsettings() const
{
	return &appsettings_;
}

bool BasicMainWindow::execWelcomeWizardDialog()
{
	WelcomeWizardDialog dlg(this);
	dlg.set_git_command_path(appsettings()->git_command);
	dlg.set_file_command_path(appsettings()->file_command);
	dlg.set_default_working_folder(appsettings()->default_working_dir);
	if (misc::isExecutable(appsettings()->git_command)) {
		gcx_.git_command = appsettings()->git_command;
		Git g(gcx_, QString());
		Git::User user = g.getUser(Git::Source::Global);
		dlg.set_user_name(user.name);
		dlg.set_user_email(user.email);
	}
	if (dlg.exec() == QDialog::Accepted) {
		appsettings()->git_command  = gcx_.git_command   = dlg.git_command_path();
		appsettings()->file_command = global->file_command = dlg.file_command_path();
		appsettings()->default_working_dir = dlg.default_working_folder();
		SettingsDialog::saveSettings(&appsettings_);

		if (misc::isExecutable(appsettings()->git_command)) {
			GitPtr g = git();
			Git::User user;
			user.name = dlg.user_name();
			user.email = dlg.user_email();
			g->setUser(user, true);
		}

		return true;
	}
	return false;
}

bool MainWindow::shown()
{
	while (!misc::isExecutable(appsettings()->git_command) || !misc::isExecutable(appsettings()->file_command)) {
		if (!execWelcomeWizardDialog()) {
			return false;
		}
	}

	setGitCommand(appsettings()->git_command, false);
	setFileCommand(appsettings()->file_command, false);

	writeLog(AboutDialog::appVersion() + '\n'); // print application version
	logGitVersion(); // print git command version

	setGpgCommand(appsettings()->gpg_command, false);

	{
		MySettings s;
		s.beginGroup("Remote");
		bool f = s.value("Online", true).toBool();
		s.endGroup();
		setRemoteOnline(f);
	}

	setUnknownRepositoryInfo();

	checkUser();

	return true;
}



void MainWindow::startTimers()
{
	// interval 10ms

	connect(&m->interval_10ms_timer, &QTimer::timeout, [&](){
		const int ms = 10;
		if (update_commit_table_counter_ > 0) {
			if (update_commit_table_counter_ > ms) {
				update_commit_table_counter_ -= ms;
			} else {
				update_commit_table_counter_ = 0;
				ui->tableWidget_log->viewport()->update();
			}
		}
		if (update_files_list_counter_ > 0) {
			if (update_files_list_counter_ > ms) {
				update_files_list_counter_ -= ms;
			} else {
				update_files_list_counter_ = 0;
				updateCurrentFilesList();
			}
		}
	});
	m->interval_10ms_timer.setInterval(10);
	m->interval_10ms_timer.start();

	startTimer(10);
}

TextEditorThemePtr BasicMainWindow::themeForTextEditor()
{
	return global->theme->text_editor_theme;
}

void MainWindow::setCurrentLogRow(int row)
{
	if (row >= 0 && row < ui->tableWidget_log->rowCount()) {
		ui->tableWidget_log->setCurrentCell(row, 2);
		ui->tableWidget_log->setFocus();
		updateStatusBarText();
	}
}

bool MainWindow::event(QEvent *event)
{
	QEvent::Type et = event->type();
	if (et == QEvent::WindowActivate) {
		checkRemoteUpdate();
	} else if (et == QEvent::KeyPress) {
		auto *e = dynamic_cast<QKeyEvent *>(event);
		Q_ASSERT(e);
		int k = e->key();
		if (k == Qt::Key_Escape) {
			emit onEscapeKeyPressed();
		} else if (k == Qt::Key_Delete) {
			if (qApp->focusWidget() == ui->treeWidget_repos) {
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

	if (et == QEvent::KeyPress) {
		auto *e = dynamic_cast<QKeyEvent *>(event);
		Q_ASSERT(e);
		int k = e->key();
		if (k == Qt::Key_Tab) {
			if (centralWidget()->isAncestorOf(qApp->focusWidget())) {
				QList<QWidget *> tabstops;
				tabstops.push_back(ui->treeWidget_repos);
				tabstops.push_back(ui->tableWidget_log);
				if (ui->stackedWidget->currentWidget() == ui->page_files) {
					tabstops.push_back(ui->listWidget_files);
				} else if (ui->stackedWidget->currentWidget() == ui->page_uncommited) {
					tabstops.push_back(ui->listWidget_unstaged);
					tabstops.push_back(ui->toolButton_select_all);
					tabstops.push_back(ui->toolButton_stage);
					tabstops.push_back(ui->toolButton_unstage);
					tabstops.push_back(ui->toolButton_commit);
					tabstops.push_back(ui->listWidget_staged);
				}
				tabstops.push_back(ui->widget_diff_view);
				int n = tabstops.size();
				if (n > 0) {
					QWidget *f = qApp->focusWidget();
					int i;
					for (i = 0; i < n; i++) {
						if (tabstops[i] == f) {
							break;
						}
					}
					if (i < n) {
						if (e->modifiers() & Qt::ShiftModifier) {
							i = (i + n - 1) % n;
						} else {
							i = (i + 1) % n;
						}
						tabstops[i]->setFocus();
					}
				}
				return true;
			}
		}
		if (k == Qt::Key_Escape) {
			if (centralWidget()->isAncestorOf(qApp->focusWidget())) {
				ui->treeWidget_repos->setFocus();
				return true;
			}
		}
		if (e->modifiers() & Qt::ControlModifier) {
			if (k == Qt::Key_Up || k == Qt::Key_Down) {
				int rows = ui->tableWidget_log->rowCount();
				int row = ui->tableWidget_log->currentRow();
				if (k == Qt::Key_Up) {
					if (row > 0) {
						row--;
					}
				} else if (k == Qt::Key_Down) {
					if (row + 1 < rows) {
						row++;
					}
				}
				ui->tableWidget_log->setCurrentCell(row, 0);
				return true;
			}
		}
		if (watched == ui->treeWidget_repos) {
			if (k == Qt::Key_Enter || k == Qt::Key_Return) {
				openSelectedRepository();
				return true;
			}
			if (!(e->modifiers() & Qt::ControlModifier)) {
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
		} else if (watched == ui->listWidget_files || watched == ui->listWidget_unstaged || watched == ui->listWidget_staged) {
			if (k == Qt::Key_Escape) {
				ui->tableWidget_log->setFocus();
				return true;
			}
		}
	} else if (et == QEvent::FocusIn) {
		// ファイルリストがフォーカスを得たとき、diffビューを更新する。（コンテキストメニュー対応）
		if (watched == ui->listWidget_unstaged) {
			m->last_focused_file_list = watched;
			updateStatusBarText();
			updateUnstagedFileCurrentItem();
			return true;
		}
		if (watched == ui->listWidget_staged) {
			m->last_focused_file_list = watched;
			updateStatusBarText();
			updateStagedFileCurrentItem();
			return true;
		}
		if (watched == ui->listWidget_files) {
			m->last_focused_file_list = watched;
		}
	}
	return false;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	if (appsettings()->remember_and_restore_window_position) {
		setWindowOpacity(0);
		Qt::WindowStates state = windowState();
		bool maximized = (state & Qt::WindowMaximized) != 0;
		if (maximized) {
			state &= ~Qt::WindowMaximized;
			setWindowState(state);
		}
		{
			MySettings settings;

			settings.beginGroup("MainWindow");
			settings.setValue("Maximized", maximized);
			settings.setValue("Geometry", saveGeometry());
			//		settings.setValue("SplitterState", ui->splitter->saveState());
			settings.endGroup();
		}
	}
	QMainWindow::closeEvent(event);
}

void MainWindow::setStatusBarText(QString const &text)
{
	m->status_bar_label->setText(text);
}

void MainWindow::clearStatusBarText()
{
	setStatusBarText(QString());
}



QString BasicMainWindow::getObjectID(QListWidgetItem *item)
{
	int i = indexOfDiff(item);
	if (i >= 0 && i < diff_.result.size()) {
		Git::Diff const &diff = diff_.result[i];
		return diff.blob.a_id;
	}
	return QString();
}

void MainWindow::onLogVisibilityChanged()
{
	ui->action_window_log->setChecked(ui->dockWidget_log->isVisible());
}

void MainWindow::internalWriteLog(char const *ptr, int len)
{
	ui->widget_log->logicalMoveToBottom();
	ui->widget_log->write(ptr, len, false);
	ui->widget_log->setChanged(false);
	interaction_canceled_ = false;
}

void BasicMainWindow::writeLog(QString const &str)
{
	std::string s = str.toStdString();
	writeLog(s.c_str(), s.size());
}

void BasicMainWindow::writeLog(QByteArray ba)
{
	if (!ba.isEmpty()) {
		writeLog(ba.data(), ba.size());
	}
}

void BasicMainWindow::emitWriteLog(QByteArray ba)
{
	emit signalWriteLog(ba);
}

bool BasicMainWindow::git_callback(void *cookie, char const *ptr, int len)
{
	auto *mw = (BasicMainWindow *)cookie;
	mw->emitWriteLog(QByteArray(ptr, len));
	return true;
}

bool BasicMainWindow::saveRepositoryBookmarks() const
{
	QString path = getBookmarksFilePath();
	return RepositoryBookmark::save(path, &repos_);
}

void BasicMainWindow::saveRepositoryBookmark(RepositoryItem item)
{
	if (item.local_dir.isEmpty()) return;

	if (item.name.isEmpty()) {
		item.name = tr("Unnamed");
	}

	bool done = false;
	for (auto &repo : repos_) {
		RepositoryItem *p = &repo;
		if (item.local_dir == p->local_dir) {
			*p = item;
			done = true;
			break;
		}
	}
	if (!done) {
		repos_.push_back(item);
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
	repos_ = std::move(newrepos);
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

QString BasicMainWindow::defaultWorkingDir() const
{
	return appsettings()->default_working_dir;
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

bool BasicMainWindow::isThereUncommitedChanges() const
{
	return uncommited_changes_;
}

Git::CommitItemList const *MainWindow::logs() const
{
	return &logs_;
}

QString BasicMainWindow::currentWorkingCopyDir() const
{
	QString workdir = current_repo_.local_dir;
	return workdir.isEmpty() ? QString() : workdir;
}

QString MainWindow::currentWorkingCopyDir() const
{
	QString workdir = BasicMainWindow::currentWorkingCopyDir();
	if (workdir.isEmpty()) {
		RepositoryItem const *repo = selectedRepositoryItem();
		if (repo) {
			workdir = repo->local_dir;
			return workdir;
		}
	}
	return workdir;
}

QString BasicMainWindow::currentRepositoryName() const
{
	return current_repo_.name;
}

QString BasicMainWindow::currentRemoteName() const
{
	return current_remote_;
}

Git::Branch const &BasicMainWindow::currentBranch() const
{
	return current_branch_;
}

QString BasicMainWindow::currentBranchName() const
{
	return currentBranch().name;
}

GitPtr BasicMainWindow::git(QString const &dir) const
{
	const_cast<BasicMainWindow *>(this)->checkGitCommand();

	GitPtr g = std::shared_ptr<Git>(new Git(gcx_, dir));
	g->setLogCallback(git_callback, (void *)this);
	return g;
}

GitPtr BasicMainWindow::git()
{
	return git(currentWorkingCopyDir());
}

bool BasicMainWindow::queryCommit(QString const &id, Git::CommitItem *out)
{
	*out = Git::CommitItem();
	GitPtr g = git();
	return g->queryCommit(id, out);
}

void BasicMainWindow::setLogEnabled(GitPtr g, bool f)
{
	if (f) {
		g->setLogCallback(git_callback, this);
	} else {
		g->setLogCallback(nullptr, nullptr);
	}
}

QString BasicMainWindow::makeRepositoryName(QString const &loc)
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

RepositoryItem const *BasicMainWindow::findRegisteredRepository(QString *workdir) const
{
	*workdir = QDir(*workdir).absolutePath();
	workdir->replace('\\', '/');

	if (Git::isValidWorkingCopy(*workdir)) {
		for (RepositoryItem const &item : repos_) {
			Qt::CaseSensitivity cs = Qt::CaseSensitive;
#ifdef Q_OS_WIN
			cs = Qt::CaseInsensitive;
#endif
			if (workdir->compare(item.local_dir, cs) == 0) {
				return &item;
			}
		}
	}
	return nullptr;
}

int MainWindow::repositoryIndex_(QTreeWidgetItem const *item) const
{
	if (item) {
		int i = item->data(0, IndexRole).toInt();
		if (i >= 0 && i < repos_.size()) {
			return i;
		}
	}
	return -1;
}

RepositoryItem const *MainWindow::repositoryItem(QTreeWidgetItem const *item) const
{
	int row = repositoryIndex_(item);
	return (row >= 0 && row < repos_.size()) ? &repos_[row] : nullptr;
}

RepositoryItem const *MainWindow::selectedRepositoryItem() const
{
	return repositoryItem(ui->treeWidget_repos->currentItem());
}

void BasicMainWindow::openSelectedRepository()
{
	RepositoryItem const *repo = selectedRepositoryItem();
	if (repo) {
		setCurrentRepository(*repo, true);
		openRepository(true);
	}
}

static QTreeWidgetItem *newQTreeWidgetItem()
{
	auto *item = new QTreeWidgetItem;
	item->setSizeHint(0, QSize(20, 20));
	return item;
}

QTreeWidgetItem *MainWindow::newQTreeWidgetFolderItem(QString const &name)
{
	QTreeWidgetItem *item = newQTreeWidgetItem();
	item->setText(0, name);
	item->setData(0, IndexRole, GroupItem);
	item->setIcon(0, folder_icon_);
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	return item;
}

void MainWindow::updateRepositoriesList()
{
	QString path = getBookmarksFilePath();

	repos_ = RepositoryBookmark::load(path);

	QString filter = repository_filter_text_;

	ui->treeWidget_repos->clear();

	std::map<QString, QTreeWidgetItem *> parentmap;

	for (int i = 0; i < repos_.size(); i++) {
		RepositoryItem const &repo = repos_[i];
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
		child->setIcon(0, repository_icon_);
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
	head_id_ = QString();
	current_branch_ = Git::Branch();
	server_type_ = ServerType::Standard;
	github_ = GitHubRepositoryInfo();
	ui->label_repo_name->setText(QString());
	ui->label_branch_name->setText(QString());

}

void MainWindow::setRepositoryInfo(QString const &reponame, QString const &brname)
{
	ui->label_repo_name->setText(reponame);
	ui->label_branch_name->setText(brname);
}

void BasicMainWindow::setUnknownRepositoryInfo()
{
	setRepositoryInfo("---", "");

	Git g(gcx_, QString());
	Git::User user = g.getUser(Git::Source::Global);
	setWindowTitle_(user);
}

bool BasicMainWindow::makeDiff(QString id, QList<Git::Diff> *out)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return false;

	Git::FileStatusList list = g->status();
	uncommited_changes_ = !list.empty();

	if (id.isEmpty() && !isThereUncommitedChanges()) {
		id = objcache_.revParse("HEAD");
	}

	bool uncommited = (id.isEmpty() && isThereUncommitedChanges());

	GitDiff dm(&objcache_);
	if (uncommited) {
		dm.diff_uncommited(out);
	} else {
		dm.diff(id, out);
	}

	return true; // success
}

void BasicMainWindow::addDiffItems(QList<Git::Diff> const *diff_list, std::function<void(QString const &filename, QString header, int idiff)> const &add_item)
{
	for (int idiff = 0; idiff < diff_list->size(); idiff++) {
		Git::Diff const &diff = diff_list->at(idiff);
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

		add_item(diff.path, header, idiff);
	}
}

void MainWindow::updateFilesList(QString id, bool wait)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	if (!wait) return;

	clearFileList();

	Git::FileStatusList stats = g->status();
	uncommited_changes_ = !stats.empty();

	FilesListType files_list_type = FilesListType::SingleList;

	bool staged = false;
	auto AddItem = [&](QString const &filename, QString header, int idiff){
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
		if (!makeDiff(uncommited ? QString() : id, &diff_.result)) {
			return;
		}

		std::map<QString, int> diffmap;

		for (int idiff = 0; idiff < diff_.result.size(); idiff++) {
			Git::Diff const &diff = diff_.result[idiff];
			QString filename = diff.path;
			if (!filename.isEmpty()) {
				diffmap[filename] = idiff;
			}
		}

		showFileList(files_list_type);

		for (Git::FileStatus const &s : stats) {
			staged = (s.isStaged() && s.code_y() == ' ');
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
			AddItem(path, header, idiff);
		}
	} else {
		if (!makeDiff(id, &diff_.result)) {
			return;
		}
		showFileList(files_list_type);
		addDiffItems(&diff_.result, AddItem);
	}

	for (Git::Diff const &diff : diff_.result) {
		QString key = GitDiff::makeKey(diff);
		diff_cache_[key] = diff;
	}
}

void BasicMainWindow::updateFilesList(QString id, QList<Git::Diff> *diff_list, QListWidget *listwidget)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	listwidget->clear();

	auto AddItem = [&](QString const &filename, QString header, int idiff){
		if (header.isEmpty()) {
			header = "(??\?) "; // damn trigraph
		}
		QListWidgetItem *item = new QListWidgetItem(header + filename);
		item->setData(FilePathRole, filename);
		item->setData(DiffIndexRole, idiff);
		item->setData(HunkIndexRole, -1);
		listwidget->addItem(item);
	};

	GitDiff dm(&objcache_);
	if (!dm.diff(id, diff_list)) return;

	addDiffItems(diff_list, AddItem);
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
	int logs = (int)logs_.size();
	if (row < logs) {
		updateFilesList(logs_[row], true);
	}
}

void MainWindow::setRemoteChanged(bool f)
{
	remote_changed_ = f;
	updateButton();
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
		auto *item = new QTableWidgetItem(text);
		ui->tableWidget_log->setHorizontalHeaderItem(i, item);
	}

	updateCommitGraph(); // コミットグラフを更新
}

void BasicMainWindow::clearAuthentication()
{
	ssh_passphrase_for_.clear();
	ssh_passphrase_secret_.clear();
	http_uid_.clear();
	http_pwd_.clear();
}

void BasicMainWindow::setCurrentRepository(RepositoryItem const &repo, bool clear_authentication)
{
	if (clear_authentication) {
		clearAuthentication();
	}
	current_repo_ = repo;
}

bool BasicMainWindow::isValidWorkingCopy(GitPtr const &g) const
{
	return g && g->isValidWorkingCopy();
}

void BasicMainWindow::queryRemotes(GitPtr g)
{
	remotes_ = g->getRemotes();
	std::sort(remotes_.begin(), remotes_.end());
}

void BasicMainWindow::queryBranches(GitPtr g)
{
	Q_ASSERT(g);
	branch_map_.clear();
	QList<Git::Branch> branches = g->branches();
	for (Git::Branch const &b : branches) {
		if (b.isCurrent()) {
			current_branch_ = b;
		}
		branch_map_[b.id].append(b);
	}
}

void BasicMainWindow::updateRemoteInfo()
{
	queryRemotes(git());

	current_remote_ = QString();
	{
		Git::Branch const &r = currentBranch();
		current_remote_ = r.remote;
	}
	if (current_remote_.isEmpty()) {
		if (remotes_.size() == 1) {
			current_remote_ = remotes_[0];
		}
	}

	emit remoteInfoChanged();
}

QList<Git::Branch> BasicMainWindow::findBranch(QString const &id)
{
	auto it = branch_map_.find(id);
	if (it != branch_map_.end()) {
		return it->second;
	}
	return QList<Git::Branch>();
}

QString BasicMainWindow::abbrevCommitID(Git::CommitItem const &commit)
{
	return commit.commit_id.mid(0, 7);
}

QString BasicMainWindow::findFileID(GitPtr /*g*/, QString const &commit_id, QString const &file)
{
	return lookupFileID(&objcache_, commit_id, file);
}

bool BasicMainWindow::isGitHub() const
{
	return server_type_ == ServerType::GitHub;
}

void BasicMainWindow::writeLog(const char *ptr, int len)
{
	internalWriteLog(ptr, len);
}

void BasicMainWindow::updateCommitTableLater()
{
	update_commit_table_counter_ = 200;
}

void BasicMainWindow::onAvatarUpdated()
{
	updateCommitTableLater();
}

bool BasicMainWindow::isAvatarEnabled() const
{
	return appsettings()->get_committer_icon;
}

Git::CommitItem const *BasicMainWindow::commitItem(int row) const
{
	if (row >= 0 && row < (int)logs_.size()) {
		return &logs_[row];
	}
	return nullptr;
}

QIcon BasicMainWindow::verifiedIcon(char s) const
{
	Git::SignatureGrade g = Git::evaluateSignature(s);
	switch (g) {
	case Git::SignatureGrade::Good:
		return signature_good_icon_;
	case Git::SignatureGrade::Bad:
		return signature_bad_icon_;
	case Git::SignatureGrade::Unknown:
	case Git::SignatureGrade::Dubious:
	case Git::SignatureGrade::Missing:
		return signature_dubious_icon_;
	}
	return QIcon();
}

QIcon BasicMainWindow::committerIcon(int row) const
{
	QIcon icon;
	if (isAvatarEnabled()) {
		if (row >= 0 && row < (int)logs_.size()) {
			Git::CommitItem const &commit = logs_[row];
			if (commit.email.indexOf('@') > 0) {
				std::string email = commit.email.toStdString();
				icon = avatar_loader_.fetch(email, true); // from gavatar
			}
		}
	}
	return icon;
}

QList<BasicMainWindow::Label> const *BasicMainWindow::label(int row)
{
	auto it = label_map_.find(row);
	if (it != label_map_.end()) {
		return &it->second;
	}
	return nullptr;
}

QString BasicMainWindow::makeCommitInfoText(int row, QList<Label> *label_list)
{
	QString message_ex;
	Git::CommitItem const *commit = &logs_[row];
	{ // branch
		if (label_list) {
			if (commit->commit_id == head_id_) {
				Label label(Label::Head);
				label.text = "HEAD";
				label_list->push_back(label);
			}
		}
		QList<Git::Branch> list = findBranch(commit->commit_id);
		for (Git::Branch const &b : list) {
			if (b.flags & Git::Branch::HeadDetachedAt) continue;
			if (b.flags & Git::Branch::HeadDetachedFrom) continue;
			Label label(Label::LocalBranch);
			label.text = b.name;
			if (!b.remote.isEmpty()) {
				label.kind = Label::RemoteBranch;
				label.text = "remotes" / b.remote / label.text;
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
			message_ex += QString(" {#%1}").arg(label.text);
			if (label_list) label_list->push_back(label);
		}
	}
	return message_ex;
}

void BasicMainWindow::setWindowTitle_(Git::User const &user)
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

void BasicMainWindow::updateWindowTitle(GitPtr g)
{
	if (isValidWorkingCopy(g)) {
		Git::User user = g->getUser(Git::Source::Default);
		setWindowTitle_(user);
	} else {
		setUnknownRepositoryInfo();
	}
}

QStringList BasicMainWindow::remotes() const
{
	return remotes_;
}

int BasicMainWindow::limitLogCount() const
{
	int n = appsettings()->maximum_number_of_commit_item_acquisitions;
	return (n >= 1 && n <= 100000) ? n : 10000;
}

//struct TemporaryCommitItem {
//	Git::CommitItem const *commit;
//	std::vector<TemporaryCommitItem *> children;
//};

Git::CommitItemList BasicMainWindow::retrieveCommitLog(GitPtr g)
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
	server_type_ = ServerType::Standard;
	github_ = GitHubRepositoryInfo();

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
			github_.owner_account_name = user;
			github_.repository_name = repo;
		}
		server_type_ = ServerType::GitHub;
	}
}

bool MainWindow::fetch(GitPtr g)
{
	pty_condition_ = PtyCondition::Fetch;
	pty_process_ok_ = true;
	g->fetch(&pty_process_);
	while (1) {
		if (pty_process_.wait(1)) break;
		QApplication::processEvents();
	}
	return pty_process_ok_;
}

void MainWindow::clearLog()
{
	logs_.clear();
	label_map_.clear();
	uncommited_changes_ = false;
	ui->tableWidget_log->clearContents();
	ui->tableWidget_log->scrollToTop();
}

void MainWindow::openRepository_(GitPtr g)
{
	objcache_.setup(g);

	if (isValidWorkingCopy(g)) {

		bool do_fetch = isRemoteOnline() && (force_fetch_ || appsettings()->automatically_fetch_when_opening_the_repository);
		force_fetch_ = false;
		if (do_fetch) {
			if (!fetch(g)) {
				return;
			}
		}

		clearLog();
		clearRepositoryInfo();

		detectGitServerType(g);

		updateFilesList(QString(), true);

		bool canceled = false;
		ui->tableWidget_log->setEnabled(false);

		// ログを取得
		logs_ = retrieveCommitLog(g);
		// ブランチを取得
		queryBranches(g);
		// タグを取得
		tag_map_.clear();
		QList<Git::Tag> tags = g->tags();
		for (Git::Tag const &tag : tags) {
			Git::Tag t = tag;
			t.id = objcache_.getCommitIdFromTag(t.id);
			tag_map_[t.id].push_back(t);
		}

		ui->tableWidget_log->setEnabled(true);
		updateCommitTableLater();
		if (canceled) return;

		QString branch_name;
		if (currentBranch().flags & Git::Branch::HeadDetachedAt) {
			branch_name += QString("(HEAD detached at %1)").arg(currentBranchName());
		}
		if (currentBranch().flags & Git::Branch::HeadDetachedFrom) {
			branch_name += QString("(HEAD detached from %1)").arg(currentBranchName());
		}
		if (branch_name.isEmpty()) {
			branch_name = currentBranchName();
		}

		QString repo_name = currentRepositoryName();
		setRepositoryInfo(repo_name, branch_name);
	} else {
		clearLog();
		clearRepositoryInfo();
	}

	updateRemoteInfo();

	updateWindowTitle(g);

	head_id_ = objcache_.revParse("HEAD");

	if (isThereUncommitedChanges()) {
		Git::CommitItem item;
		item.parent_ids.push_back(current_branch_.id);
		item.message = tr("Uncommited changes");
		logs_.insert(logs_.begin(), item);
	}

	prepareLogTableWidget();

	int count = logs_.size();

	ui->tableWidget_log->setRowCount(count);

	int selrow = -1;

	for (int row = 0; row < count; row++) {
		Git::CommitItem const *commit = &logs_[row];
		{
			auto *item = new QTableWidgetItem;
			item->setData(IndexRole, row);
			ui->tableWidget_log->setItem(row, 0, item);
		}
		int col = 1; // カラム0はコミットグラフなので、その次から
		auto AddColumn = [&](QString const &text, bool bold, QString const &tooltip){
			auto *item = new QTableWidgetItem(text);
			if (!tooltip.isEmpty()) {
				QString tt = tooltip;
				tt.replace('\n', ' ');
				tt = tt.toHtmlEscaped();
				tt = "<p style='white-space: pre'>" + tt + "</p>";
				item->setToolTip(tt);
			}
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
		bool isHEAD = commit->commit_id == head_id_;
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
			message_ex = makeCommitInfoText(row, &label_map_[row]);
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

	m->last_focused_file_list = nullptr;

	ui->tableWidget_log->setFocus();
	setCurrentLogRow(0);

	QTableWidgetItem *p = ui->tableWidget_log->item(selrow < 0 ? 0 : selrow, 2);
	ui->tableWidget_log->setCurrentItem(p);

	m->remote_watcher.setCurrent(currentRemoteName(), currentBranchName());

	checkRemoteUpdate();
	doUpdateButton();
}

void BasicMainWindow::removeRepositoryFromBookmark(int index, bool ask)
{
	if (ask) {
		int r = QMessageBox::warning(this, tr("Confirm Remove"), tr("Are you sure you want to remove the repository from bookmarks ?") + '\n' + tr("(Files will NOT be deleted)"), QMessageBox::Ok, QMessageBox::Cancel);
		if (r != QMessageBox::Ok) return;
	}
	if (index >= 0 && index < repos_.size()) {
		repos_.erase(repos_.begin() + index);
		saveRepositoryBookmarks();
		updateRepositoriesList();
	}
}

void MainWindow::removeSelectedRepositoryFromBookmark(bool ask)
{
	int i = indexOfRepository(ui->treeWidget_repos->currentItem());
	removeRepositoryFromBookmark(i, ask);
}

void BasicMainWindow::openRepository(bool validate, bool waitcursor)
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

void BasicMainWindow::reopenRepository(bool log, std::function<void(GitPtr g)> callback)
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

void MainWindow::doUpdateButton()
{
	setNetworkingCommandsEnabled(isRemoteOnline());

	Git::Branch b = currentBranch();

	int n;

	ui->toolButton_fetch->setDot(remote_changed_);

	n = b.ahead > 0 ? b.ahead : -1;
	ui->toolButton_push->setNumber(n);

	n = b.behind > 0 ? b.behind : -1;
	ui->toolButton_pull->setNumber(n);
}

void BasicMainWindow::autoOpenRepository(QString dir)
{
	auto Open = [&](RepositoryItem const &item){
		setCurrentRepository(item, true);
		openRepository(false, true);
	};

	RepositoryItem const *repo = findRegisteredRepository(&dir);
	if (repo) {
		Open(*repo);
		return;
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
	} else {
		DoYouWantToInitDialog dlg(this, dir);
		if (dlg.exec() == QDialog::Accepted) {
			createRepository(dir);
		}
	}
}

QString BasicMainWindow::getBookmarksFilePath() const
{
	return global->app_config_dir / "bookmarks.xml";
}



void BasicMainWindow::commit(bool amend)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	while (1) {
		Git::User user = g->getUser(Git::Source::Default);
		QString sign_id = g->signingKey(Git::Source::Default);
		gpg::Data key;
		{
			QList<gpg::Data> keys;
			gpg::listKeys(global->gpg_command, &keys);
			for (gpg::Data const &k : keys) {
				if (k.id == sign_id) {
					key = k;
				}
			}
		}
		CommitDialog dlg(this, currentRepositoryName(), user, key);
		if (amend) {
			dlg.setText(logs_[0].message);
		}
		if (dlg.exec() == QDialog::Accepted) {
			QString text = dlg.text();
			if (text.isEmpty()) {
				QMessageBox::warning(this, tr("Commit"), tr("Commit message can not be omitted."));
				continue;
			}
			bool sign = dlg.isSigningEnabled();
			bool ok;
			if (amend) {
				ok = g->commit_amend_m(text, sign, &pty_process_);
			} else {
				ok = g->commit(text, sign, &pty_process_);
			}
			if (ok) {
				force_fetch_ = true;
				openRepository(true);
			} else {
				QString err = g->errorMessage().trimmed();
				err += "\n*** ";
				err += tr("Failed to commit");
				err += " ***\n";
				writeLog(err);
			}
		}
		break;
	}
}

void BasicMainWindow::commitAmend()
{
	commit(true);
}


void MainWindow::updateStatusBarText()
{
	QString text;

	QWidget *w = qApp->focusWidget();
	if (w == ui->treeWidget_repos) {
		RepositoryItem const *repo = selectedRepositoryItem();
		if (repo) {
			text = QString("%1 : %2")
					.arg(repo->name)
					.arg(misc::normalizePathSeparator(repo->local_dir))
					;
		}
	} else if (w == ui->tableWidget_log) {
		QTableWidgetItem *item = ui->tableWidget_log->item(selectedLogIndex(), 0);
		if (item) {
			int row = item->data(IndexRole).toInt();
			if (row < (int)logs_.size()) {
				Git::CommitItem const &commit = logs_[row];
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

void BasicMainWindow::execRepositoryPropertyDialog(QString workdir, bool open_repository_menu)
{
	if (workdir.isEmpty()) {
		workdir = currentWorkingCopyDir();
	}
	QString name;
	RepositoryItem const *repo = findRegisteredRepository(&workdir);
	if (repo) {
		name = repo->name;
	} else {
		QMessageBox::warning(this, tr("Repository Property"), tr("Not a valid git repository") + "\n\n" + workdir);
		return;
	}
	if (name.isEmpty()) {
		name = makeRepositoryName(workdir);
	}
	GitPtr g = git(workdir);
	RepositoryPropertyDialog dlg(this, g, *repo, open_repository_menu);
	dlg.exec();
	if (dlg.isRemoteChanged()) {
		emit remoteInfoChanged();
	}
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
	push();
}

void BasicMainWindow::push()
{
	if (!isRemoteOnline()) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	if (g->getRemotes().isEmpty()) {
		QMessageBox::warning(this, qApp->applicationName(), tr("No remote repository is registered."));
		execRepositoryPropertyDialog(QString(), true);
		return;
	}

	int exitcode = 0;
	QString errormsg;

	reopenRepository(true, [&](GitPtr g){
		g->push(false, &pty_process_);
		while (1) {
			if (pty_process_.wait(1)) break;
			QApplication::processEvents();
		}
		exitcode = pty_process_.getExitCode();
		errormsg = pty_process_.getMessage();
	});

	if (exitcode == 128) {
		if (errormsg.indexOf("no upstream branch") >= 0) {
			QString brname = currentBranchName();

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
	}
}

void MainWindow::on_action_pull_triggered()
{
	if (!isRemoteOnline()) return;

	reopenRepository(true, [&](GitPtr g){
		pty_condition_ = PtyCondition::Pull;
		pty_process_ok_ = true;
		g->pull(&pty_process_);
		while (1) {
			if (pty_process_.wait(1)) break;
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

void MainWindow::on_treeWidget_repos_currentItemChanged(QTreeWidgetItem * /*current*/, QTreeWidgetItem * /*previous*/)
{
	updateStatusBarText();
}

void MainWindow::on_treeWidget_repos_itemDoubleClicked(QTreeWidgetItem * /*item*/, int /*column*/)
{
	openSelectedRepository();
}

void BasicMainWindow::execCommitPropertyDialog(QWidget *parent, Git::CommitItem const *commit)
{
	CommitPropertyDialog dlg(parent, this, commit);
	dlg.exec();
}

QAction *BasicMainWindow::addMenuActionProperty(QMenu *menu)
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
		QAction *a_properties = addMenuActionProperty(&menu);

		QPoint pt = ui->treeWidget_repos->mapToGlobal(pos);
		QAction *a = menu.exec(pt + QPoint(8, -8));
		if (a) {
			if (a == a_open) {
				openSelectedRepository();
				return;
			}
			if (a == a_open_folder) {
				openExplorer(repo);
				return;
			}
			if (a == a_open_terminal) {
				openTerminal(repo);
				return;
			}
			if (a == a_remove) {
				removeRepositoryFromBookmark(index, true);
				return;
			}
			if (a == a_properties) {
				execRepositoryPropertyDialog(repo->local_dir);
				return;
			}
		}
	}
}

void BasicMainWindow::execCommitExploreWindow(QWidget *parent, Git::CommitItem const *commit)
{
	CommitExploreWindow win(parent, this, &objcache_, commit);
	win.exec();
}

void MainWindow::on_tableWidget_log_customContextMenuRequested(const QPoint &pos)
{
	Git::CommitItem const *commit = selectedCommitItem();
	if (commit) {
		bool is_valid_commit_id = Git::isValidID(commit->commit_id);
		int row = selectedLogIndex();

		QMenu menu;

		QAction *a_copy_id_7_letters = menu.addAction(tr("Copy commit id (7 letters)"));
		QAction *a_copy_id_complete = menu.addAction(tr("Copy commit id (completely)"));

		menu.addSeparator();

		QAction *a_checkout = menu.addAction(tr("Checkout/Branch..."));

		menu.addSeparator();

		QAction *a_edit_comment = nullptr;
		if (row == 0 && currentBranch().ahead > 0) {
			a_edit_comment = menu.addAction(tr("Edit comment..."));
		}

		QAction *a_merge = is_valid_commit_id ? menu.addAction(tr("Merge")) : nullptr;
		QAction *a_rebase = is_valid_commit_id ? menu.addAction(tr("Rebase")) : nullptr;
		QAction *a_cherrypick = is_valid_commit_id ? menu.addAction(tr("Cherry-pick")) : nullptr;
		QAction *a_edit_tags = is_valid_commit_id ? menu.addAction(tr("Edit tags...")) : nullptr;
		QAction *a_revert = is_valid_commit_id ? menu.addAction(tr("Revert")) : nullptr;

		QAction *a_reset_head = nullptr;
#if 0 // 下手に使うと危険なので、とりあえず無効にしておく
		if (is_valid_commit_id && commit->commit_id == head_id_) {
			a_reset_head = menu.addAction(tr("Reset HEAD"));
		}
#endif

		menu.addSeparator();

		QAction *a_delbranch = is_valid_commit_id ? menu.addAction(tr("Delete branch...")) : nullptr;
		QAction *a_delrembranch = remoteBranches(commit->commit_id).isEmpty() ? nullptr : menu.addAction(tr("Delete remote branch..."));

		menu.addSeparator();

		QAction *a_explore = is_valid_commit_id ? menu.addAction(tr("Explore")) : nullptr;
		QAction *a_properties = addMenuActionProperty(&menu);

		QAction *a = menu.exec(ui->tableWidget_log->viewport()->mapToGlobal(pos) + QPoint(8, -8));
		if (a) {
			if (a == a_copy_id_7_letters) {
				qApp->clipboard()->setText(commit->commit_id.mid(0, 7));
				return;
			}
			if (a == a_copy_id_complete) {
				qApp->clipboard()->setText(commit->commit_id);
				return;
			}
			if (a == a_properties) {
				execCommitPropertyDialog(this, commit);
				return;
			}
			if (a == a_edit_comment) {
				commitAmend();
				return;
			}
			if (a == a_checkout) {
				checkout(this, commit);
				return;
			}
			if (a == a_delbranch) {
				deleteBranch(commit);
				return;
			}
			if (a == a_delrembranch) {
				deleteRemoteBranch(commit);
				return;
			}
			if (a == a_merge) {
				mergeBranch(commit);
				return;
			}
			if (a == a_rebase) {
				rebaseBranch(commit);
				return;
			}
			if (a == a_cherrypick) {
				cherrypick(commit);
				return;
			}
			if (a == a_edit_tags) {
				ui->action_edit_tags->trigger();
				return;
			}
			if (a == a_revert) {
				revertCommit();
				return;
			}
			if (a == a_explore) {
				execCommitExploreWindow(this, commit);
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
	QAction *a_blame = menu.addAction(tr("Blame"));
	QAction *a_properties = addMenuActionProperty(&menu);

	QPoint pt = ui->listWidget_files->mapToGlobal(pos) + QPoint(8, -8);
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
		} else if (a == a_blame) {
			blame(item);
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
		QAction *a_blame = menu.addAction(tr("Blame"));
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
				return;
			}
			if (a == a_reset_file) {
				QStringList paths;
				for_each_selected_files([&](QString const &path){
					paths.push_back(path);
				});
				resetFile(paths);
				return;
			}
			if (a == a_ignore) {
				QString gitignore_path = currentWorkingCopyDir() / ".gitignore";
				if (items.size() == 1) {
					QString file = getFilePath(items[0]);
					EditGitIgnoreDialog dlg(this, gitignore_path, file);
					if (dlg.exec() == QDialog::Accepted) {
						QString appending = dlg.text();
						if (!appending.isEmpty()) {
							QString text;

							QString path = gitignore_path;
							path.replace('/', QDir::separator());

							{
								QFile file(path);
								if (file.open(QFile::ReadOnly)) {
									text += QString::fromUtf8(file.readAll());
								}
							}

							size_t n = text.size();
							if (n > 0 && text[(int)n - 1] != '\n') {
								text += '\n'; // 最後に改行を追加
							}

							text += appending + '\n';

							{
								QFile file(path);
								if (file.open(QFile::WriteOnly)) {
									file.write(text.toUtf8());
								}
							}
							updateCurrentFilesList();
							return;
						}
					} else {
						return;
					}
				}

				QString append;
				for_each_selected_files([&](QString const &path){
					if (path == ".gitignore") {
						// skip
					} else {
						append += path + '\n';
					}
				});
				if (TextEditDialog::editFile(this, gitignore_path, ".gitignore", append)) {
					updateCurrentFilesList();
				}
				return;
			}
			if (a == a_delete) {
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
				return;
			}
			if (a == a_untrack) {
				if (askAreYouSureYouWantToRun("Untrack", "rm --cached")) {
					for_each_selected_files([&](QString const &path){
						g->rm_cached(path);
					});
					openRepository(false);
				}
				return;
			}
			if (a == a_history) {
				execFileHistory(item);
				return;
			}
			if (a == a_blame) {
				blame(item);
				return;
			}
			if (a == a_properties) {
				execFilePropertyDialog(item);
				return;
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
			QAction *a_blame = menu.addAction(tr("Blame"));
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
				} else if (a == a_blame) {
					blame(item);
				} else if (a == a_properties) {
					execFilePropertyDialog(item);
				}
			}
		}
	}
}

bool BasicMainWindow::askAreYouSureYouWantToRun(QString const &title, QString const &command)
{
	QString message = tr("Are you sure you want to run the following command ?");
	QString text = "%1\n\n%2";
	text = text.arg(message).arg(command);
	return QMessageBox::warning(this, title, text, QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok;
}

void BasicMainWindow::resetFile(QStringList const &paths)
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

void BasicMainWindow::revertAllFiles()
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
	if (m->last_focused_file_list == ui->listWidget_files)    return selectedFiles_(ui->listWidget_files);
	if (m->last_focused_file_list == ui->listWidget_staged)   return selectedFiles_(ui->listWidget_staged);
	if (m->last_focused_file_list == ui->listWidget_unstaged) return selectedFiles_(ui->listWidget_unstaged);
	return QStringList();
}

void MainWindow::for_each_selected_files(std::function<void(QString const&)> fn)
{
	for (QString path : selectedFiles()) {
		fn(path);
	}
}

void BasicMainWindow::execFileHistory(QString const &path)
{
	if (path.isEmpty()) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	FileHistoryWindow dlg(this);
	dlg.prepare(g, path);
	dlg.exec();
}

void BasicMainWindow::execFileHistory(QListWidgetItem *item)
{
	if (item) {
		QString path = getFilePath(item);
		if (!path.isEmpty()) {
			execFileHistory(path);
		}
	}
}

void BasicMainWindow::addWorkingCopyDir(QString dir, QString name, bool open)
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
		setCurrentRepository(item, true);
		GitPtr g = git(item.local_dir);
		openRepository_(g);
	}
}

void BasicMainWindow::addWorkingCopyDir(QString dir, bool open)
{
	addWorkingCopyDir(dir, QString(), open);
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
	if (row < (int)logs_.size()) {
		updateStatusBarText();
		update_files_list_counter_ = 200;
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

bool BasicMainWindow::editFile(QString const &path, QString const &title)
{
	return TextEditDialog::editFile(this, path, title);
}

void MainWindow::updateCommitGraph()
{
	const size_t LogCount = logs_.size();
	// 樹形図情報を構築する
	if (LogCount > 0) {
		auto LogItem = [&](size_t i)->Git::CommitItem &{ return logs_[i]; };
		enum { // 有向グラフを構築するあいだ CommitItem::marker_depth をフラグとして使用する
			UNKNOWN = 0,
			KNOWN = 1,
		};
		for (Git::CommitItem &item : logs_) {
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
							item->parent_lines.emplace_back(k); // インデックス値を記憶
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
							tasks.emplace_back(i, j); // タスクを追加
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
						tasks.emplace_back(index, i); // タスク追加
					}
					index = LogItem(index).parent_lines[0].index; // 次の親（親リストの先頭の要素）
				}
				if (e.indexes.size() >= 2) {
					elements.push_back(e);
				}
			}
		}
		// 線情報をクリア
		for (Git::CommitItem &item : logs_) {
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
			for (const auto & e : elements) {
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

QList<Git::Tag> BasicMainWindow::findTag(QString const &id)
{
	auto it = tag_map_.find(id);
	if (it != tag_map_.end()) {
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
	if (editFile(path, ".gitignore")) {
		updateCurrentFilesList();
	}
}

int MainWindow::selectedLogIndex() const
{
	int i = ui->tableWidget_log->currentRow();
	if (i >= 0 && i < (int)logs_.size()) {
		return i;
	}
	return -1;
}

Git::CommitItem const *BasicMainWindow::selectedCommitItem() const
{
	int i = selectedLogIndex();
	return commitItem(i);
}

Git::Object BasicMainWindow::cat_file_(GitPtr g, QString const &id)
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
			return objcache_.catFile(id);;
		}
	}
	return Git::Object();
}

Git::Object BasicMainWindow::cat_file(QString const &id)
{
	return cat_file_(git(), id);
}

void MainWindow::updateDiffView(QListWidgetItem *item)
{
	clearDiffView();

	m->last_selected_file_item = item;

	if (!item) return;

	int idiff = indexOfDiff(item);
	if (idiff >= 0 && idiff < diff_.result.size()) {
		Git::Diff const &diff = diff_.result[idiff];
		QString key = GitDiff::makeKey(diff);
		auto it = diff_cache_.find(key);
		if (it != diff_cache_.end()) {
			int row = ui->tableWidget_log->currentRow();
			bool uncommited = (row >= 0 && row < (int)logs_.size() && Git::isUncommited(logs_[row]));
			ui->widget_diff_view->updateDiffView(it->second, uncommited);
		}
	}
}

void MainWindow::updateDiffView()
{
	updateDiffView(m->last_selected_file_item);
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

void BasicMainWindow::logGitVersion()
{
	GitPtr g = git();
	QString s = g->version();
	if (!s.isEmpty()) {
		s += '\n';
		writeLog(s);
	}
}

bool BasicMainWindow::checkExecutable(QString const &path)
{
	if (QFileInfo(path).isExecutable()) {
		return true;
	}
	QString text = "The specified program '%1' is not executable.\n";
	text = text.arg(path);
	writeLog(text);
	return false;
}

void BasicMainWindow::internalSetCommand(QString const &path, bool save, QString const &name, QString *out)
{
	if (!path.isEmpty() && checkExecutable(path)) {
		if (save) {
			MySettings s;
			s.beginGroup("Global");
			s.setValue(name, path);
			s.endGroup();
		}
		*out = path;
	} else {
		*out = QString();
	}
}

void BasicMainWindow::setGitCommand(QString const &path, bool save)
{
	internalSetCommand(path, save, "GitCommand", &gcx_.git_command);
}

void BasicMainWindow::setFileCommand(QString const &path, bool save)
{
	internalSetCommand(path, save, "FileCommand", &global->file_command);
}

void BasicMainWindow::setGpgCommand(QString const &path, bool save)
{
	internalSetCommand(path, save, "GpgCommand", &global->gpg_command);
	if (!global->gpg_command.isEmpty()) {
		GitPtr g = git();
		g->configGpgProgram(global->gpg_command, true);
	}
}

void MainWindow::setWatchRemoteInterval(int mins)
{
	if (mins > 0) {
		m->remote_watcher_timer.start(mins * 60000);
	} else {
		m->remote_watcher_timer.stop();
	}
}

#ifdef Q_OS_WIN
QString getWin32HttpProxy();
#endif

void BasicMainWindow::initNetworking()
{
	std::string http_proxy;
	std::string https_proxy;
	if (appsettings()->proxy_type == "auto") {
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
	} else if (appsettings()->proxy_type == "manual") {
		http_proxy = appsettings()->proxy_server.toStdString();
	}
	webcx_.set_http_proxy(http_proxy);
	webcx_.set_https_proxy(https_proxy);
}



QStringList BasicMainWindow::whichCommand_(QString const &cmdfile1, QString const &cmdfile2)
{
	QStringList list;

	if (!cmdfile1.isEmpty()){
		std::vector<std::string> vec;
		FileUtil::which(cmdfile1.toStdString(), &vec);
		for (std::string const &s : vec) {
			list.push_back(QString::fromStdString(s));
		}
	}
	if (!cmdfile2.isEmpty()){
		std::vector<std::string> vec;
		FileUtil::which(cmdfile2.toStdString(), &vec);
		for (std::string const &s : vec) {
			list.push_back(QString::fromStdString(s));
		}
	}

	return list;
}

QString BasicMainWindow::selectCommand_(QString const &cmdname, QStringList const &cmdfiles, QStringList const &list, QString path, std::function<void(QString const &)> callback)
{
	QString window_title = tr("Select %1 command");
	window_title = window_title.arg(cmdfiles.front());

	SelectCommandDialog dlg(this, cmdname, cmdfiles, path, list);
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

QString BasicMainWindow::selectCommand_(QString const &cmdname, QString const &cmdfile, QStringList const &list, QString path, std::function<void(QString const &)> callback)
{
	QStringList cmdfiles;
	cmdfiles.push_back(cmdfile);
	return selectCommand_(cmdname, cmdfiles, list, path, callback);
}

#ifdef Q_OS_WIN
#define GIT_COMMAND "git.exe"
#define FILE_COMMAND "file.exe"
#define GPG_COMMAND "gpg.exe"
#define GPG2_COMMAND "gpg2.exe"
#else
#define GIT_COMMAND "git"
#define FILE_COMMAND "file"
#define GPG_COMMAND "gpg"
#define GPG2_COMMAND "gpg2"
#endif

QString BasicMainWindow::selectGitCommand(bool save)
{
	char const *exe = GIT_COMMAND;

	QString path = gcx_.git_command;

	auto fn = [&](QString const &path){
		setGitCommand(path, save);
	};

	QStringList list = whichCommand_(exe);
#ifdef Q_OS_WIN
	{
		QStringList newlist;
		QString suffix1 = "\\bin\\" GIT_COMMAND;
		QString suffix2 = "\\cmd\\" GIT_COMMAND;
		for (QString const &s : list) {
			newlist.push_back(s);
			auto DoIt = [&](QString const &suffix){
				if (s.endsWith(suffix)) {
					QString t = s.mid(0, s.size() - suffix.size());
					QString t1 = t + "\\mingw64\\bin\\" GIT_COMMAND;
					if (misc::isExecutable(t1)) newlist.push_back(t1);
					QString t2 = t + "\\mingw\\bin\\" GIT_COMMAND;
					if (misc::isExecutable(t2)) newlist.push_back(t2);
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

QString BasicMainWindow::selectFileCommand(bool save)
{
	char const *exe = FILE_COMMAND;

	QString path = global->file_command;

	auto fn = [&](QString const &path){
		setFileCommand(path, save);
	};

	QStringList list = whichCommand_(exe);

#ifdef Q_OS_WIN
	QString dir = misc::getApplicationDir();
	QString path1 = dir / FILE_COMMAND;
	QString path2;
	int i = dir.lastIndexOf('/');
	int j = dir.lastIndexOf('\\');
	if (i < j) i = j;
	if (i > 0) {
		path2 = dir.mid(0, i) / FILE_COMMAND;
	}
	path1 = misc::normalizePathSeparator(path1);
	path2 = misc::normalizePathSeparator(path2);
	if (misc::isExecutable(path1)) list.push_back(path1);
	if (misc::isExecutable(path2)) list.push_back(path2);
#endif

	return selectCommand_("File", exe, list, path, fn);
}

QString BasicMainWindow::selectGpgCommand(bool save)
{
	QString path = global->gpg_command;

	auto fn = [&](QString const &path){
		setGpgCommand(path, save);
	};

	QStringList list = whichCommand_(GPG_COMMAND, GPG2_COMMAND);

	QStringList cmdlist;
	cmdlist.push_back(GPG_COMMAND);
	cmdlist.push_back(GPG2_COMMAND);
	return selectCommand_("GPG", cmdlist, list, path, fn);
}

void BasicMainWindow::checkUser()
{
	Git g(gcx_, QString());
	while (1) {
		Git::User user = g.getUser(Git::Source::Global);
		if (!user.name.isEmpty() && !user.email.isEmpty()) {
			return; // ok
		}
		if (!execSetGlobalUserDialog()) {
			return;
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
	bool running = pty_process_.isRunning();
	if (ui->toolButton_stop_process->isEnabled() != running) {
		ui->toolButton_stop_process->setEnabled(running);
		ui->action_stop_process->setEnabled(running);
		setNetworkingCommandsEnabled(!running);
	}
	if (!running) {
		interaction_mode_ = InteractionMode::None;
	}

	while (1) {
		char tmp[1024];
		int len = pty_process_.readOutput(tmp, sizeof(tmp));
		if (len < 1) break;
		writeLog(tmp, len);
	}
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
	if (qApp->focusWidget() == ui->widget_log) {
		int c = event->key();

		auto write_char = [&](char c){
			if (pty_process_.isRunning()) {
				pty_process_.writeInput(&c, 1);
			}
		};

		auto write_text = [&](QString const &str){
			std::string s = str.toStdString();
			for (char i : s) {
				write_char(i);
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

bool BasicMainWindow::saveByteArrayAs(QByteArray const &ba, QString const &dstpath)
{
	QFile file(dstpath);
	if (file.open(QFile::WriteOnly)) {
		file.write(ba);
		file.close();
		return true;
	}
	QString msg = "Failed to open the file '%1' for write";
	msg = msg.arg(dstpath);
	qDebug() << msg;
	return false;
}

bool BasicMainWindow::saveFileAs(QString const &srcpath, QString const &dstpath)
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

bool BasicMainWindow::saveBlobAs(QString const &id, QString const &dstpath)
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

bool BasicMainWindow::saveAs(QString const &id, QString const &dstpath)
{
	if (id.startsWith(PATH_PREFIX)) {
		return saveFileAs(id.mid(1), dstpath);
	} else {
		return saveBlobAs(id, dstpath);
	}
}

QString BasicMainWindow::saveAsTemp(QString const &id)
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
		appsettings_ = newsettings;
		setGitCommand(appsettings()->git_command, false);
		setFileCommand(appsettings()->file_command, false);
		setGpgCommand(appsettings()->gpg_command, false);
		setWatchRemoteInterval(appsettings()->watch_remote_changes_every_mins);
	}
}

void BasicMainWindow::clone()
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

			temp_repo_.local_dir = dir;
			temp_repo_.local_dir.replace('\\', '/');
			temp_repo_.name = makeRepositoryName(dir);

			GitPtr g = git(QString());
			pty_condition_ = PtyCondition::Clone;
			pty_process_ok_ = true;
			g->clone(clone_data, &pty_process_);

		} else if (dlg.action() == CloneDialog::Action::AddExisting) {
			addWorkingCopyDir(dir, true);
		}

		return; // done
	}
}

void MainWindow::onCloneCompleted(bool success)
{
	if (success) {
		saveRepositoryBookmark(temp_repo_);
		setCurrentRepository(temp_repo_, false);
		openRepository(true);
	}
}

void MainWindow::onPtyProcessCompleted()
{
	switch (pty_condition_) {
	case PtyCondition::Clone:
		onCloneCompleted(pty_process_ok_);
		break;
	}
	pty_condition_ = PtyCondition::None;
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
	ui->lineEdit_filter->setText(repository_filter_text_ + c);
}

void MainWindow::backspaceRepoFilter()
{
	QString s = repository_filter_text_;
	int n = s.size();
	if (n > 0) {
		s = s.mid(0, n - 1);
	}
	ui->lineEdit_filter->setText(s);
}

void MainWindow::on_lineEdit_filter_textChanged(QString const &text)
{
	repository_filter_text_ = text;
	updateRepositoriesList();
}

void MainWindow::on_toolButton_erase_filter_clicked()
{
	clearRepoFilter();
	ui->lineEdit_filter->setFocus();
}

void BasicMainWindow::internalDeleteTags(QStringList const &tagnames)
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

void MainWindow::deleteTags(QStringList const &tagnames)
{
	int row = ui->tableWidget_log->currentRow();

	internalDeleteTags(tagnames);

	ui->tableWidget_log->selectRow(row);
}

void BasicMainWindow::deleteTags(const Git::CommitItem &commit)
{
	auto it = tag_map_.find(commit.commit_id);
	if (it != tag_map_.end()) {
		QStringList names;
		QList<Git::Tag> const &tags = it->second;
		for (Git::Tag const &tag : tags) {
			names.push_back(tag.name);
		}
		deleteTags(names);
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

bool BasicMainWindow::internalAddTag(QString const &name)
{
	if (name.isEmpty()) return false;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return false;

	QString commit_id;

	Git::CommitItem const *commit = selectedCommitItem();
	if (commit && !commit->commit_id.isEmpty()) {
		commit_id = commit->commit_id;
	}

	if (!Git::isValidID(commit_id)) return false;

	bool ok = false;
	reopenRepository(false, [&](GitPtr g){
		ok = g->tag(name, commit_id);
	});
}

bool MainWindow::addTag(QString const &name)
{
	int row = ui->tableWidget_log->currentRow();

	bool ok = internalAddTag(name);

	ui->tableWidget_log->selectRow(row);
	return ok;
}

void MainWindow::on_action_tag_push_all_triggered()
{
	reopenRepository(false, [&](GitPtr g){
		g->push(true);
	});
}

QString BasicMainWindow::tempfileHeader() const
{
	QString name = "jp_soramimi_Guitar_%1_";
	return name.arg(QApplication::applicationPid());
}

void BasicMainWindow::deleteTempFiles()
{
	QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
	QString name = tempfileHeader();
	QDirIterator it(dir, { name + "*" });
	while (it.hasNext()) {
		QString path = it.next();
		QFile::remove(path);
	}
}

QString BasicMainWindow::newTempFilePath()
{
	QString tmpdir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
	QString path = tmpdir / tempfileHeader() + QString::number(temp_file_counter_);
	temp_file_counter_++;
	return path;
}

QString BasicMainWindow::determinFileType_(QString const &path, bool mime, std::function<void(QString const &cmd, QByteArray *ba)> callback) const
{
	const_cast<BasicMainWindow *>(this)->checkFileCommand();
	return misc::determinFileType(global->file_command, path, mime, callback);
}

QString BasicMainWindow::determinFileType(QString const &path, bool mime)
{
	return determinFileType_(path, mime, [](QString const &cmd, QByteArray *ba){
		misc2::runCommand(cmd, ba);
	});
}

QString BasicMainWindow::determinFileType(QByteArray in, bool mime)
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
		int r = misc2::runCommand(cmd, &in, ba);
		if (r != 0) {
			ba->clear();
		}
	});
}

void MainWindow::on_tableWidget_log_itemDoubleClicked(QTableWidgetItem *)
{
	Git::CommitItem const *commit = selectedCommitItem();
	if (commit) {
		execCommitPropertyDialog(this, commit);
	}
}

void BasicMainWindow::execFilePropertyDialog(QListWidgetItem *item)
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

QListWidgetItem *MainWindow::currentFileItem() const
{
	QListWidget *listwidget = nullptr;
	if (ui->stackedWidget->currentWidget() == ui->page_uncommited) {
		QWidget *w = qApp->focusWidget();
		if (w == ui->listWidget_unstaged) {
			listwidget = ui->listWidget_unstaged;
		} else if (w == ui->listWidget_staged) {
			listwidget = ui->listWidget_staged;
		}
	} else {
		listwidget = ui->listWidget_files;
	}
	if (listwidget) {
		return listwidget->currentItem();
	}
	return nullptr;
}

QPixmap BasicMainWindow::getTransparentPixmap()
{
	if (transparent_pixmap_.isNull()) {
		transparent_pixmap_ = QPixmap(":/image/transparent.png");
	}
	return transparent_pixmap_;
}

QString BasicMainWindow::getCommitIdFromTag(QString const &tag)
{
	return objcache_.getCommitIdFromTag(tag);
}

bool BasicMainWindow::isValidRemoteURL(QString const &url)
{
	if (url.indexOf('\"') >= 0) {
		return false;
	}
	stopPtyProcess();
	GitPtr g = git(QString());
	QString cmd = "ls-remote \"%1\" HEAD";
	bool f = g->git(cmd.arg(url), false, false, &pty_process_);
	{
		QTime time;
		time.start();
		while (!pty_process_.wait(10)) {
			if (time.elapsed() > 10000) {
				f = false;
				break;
			}
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		}
		stopPtyProcess();
	}
	QString line = g->resultText().trimmed();
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
	}
	qDebug() << "It is not a repository.";
	return false;
}

bool BasicMainWindow::testRemoteRepositoryValidity(QString const &url)
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

bool BasicMainWindow::execSetGlobalUserDialog()
{
	SetGlobalUserDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		GitPtr g = git();
		Git::User user = dlg.user();
		g->setUser(user, true);
		updateWindowTitle(g);
		return true;
	}
	return false;
}

void BasicMainWindow::execSetUserDialog(Git::User const &global_user, Git::User const &repo_user, QString const &reponame)
{
	SetUserDialog dlg(this, global_user, repo_user, reponame);
	if (dlg.exec() == QDialog::Accepted) {
		GitPtr g = git();
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

void MainWindow::on_action_set_config_user_triggered()
{
	Git::User global_user;
	Git::User repo_user;
	GitPtr g = git();
	if (isValidWorkingCopy(g)) {
		repo_user = g->getUser(Git::Source::Local);
	}
	global_user = g->getUser(Git::Source::Global);

	execSetUserDialog(global_user, repo_user, currentRepositoryName());
}

void MainWindow::on_action_window_log_triggered(bool checked)
{
	ui->dockWidget_log->setVisible(checked);
}

NamedCommitList BasicMainWindow::namedCommitItems(int flags)
{
    NamedCommitList items;
	if (flags & Branches) {
		for (auto pair: branch_map_) {
			QList<Git::Branch> const &list = pair.second;
			for (Git::Branch const &b : list) {
				if (b.isHeadDetached()) continue;
				QString key = b.name;
				if (flags & NamedCommitFlag::Remotes) {
					if (!b.remote.isEmpty()) {
						key = "remotes" / b.remote / key;
					}
				} else {
					if (!b.remote.isEmpty()) continue;
				}
				NamedCommitItem item;
				item.type = NamedCommitItem::Type::Branch;
				item.remote = b.remote;
				item.name = b.name;
				item.id = b.id;
                items.push_back(item);
			}
		}
	}
	if (flags & Tags) {
		for (auto pair: tag_map_) {
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

int BasicMainWindow::rowFromCommitId(QString const &id)
{
	for (size_t i = 0; i < logs_.size(); i++) {
		Git::CommitItem const &item = logs_[i];
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

	NamedCommitList items = namedCommitItems(Branches | Tags | Remotes);
	JumpDialog::sort(&items);
	{
		NamedCommitItem head;
		head.name = "HEAD";
		head.id = head_id_;
		items.push_front(head);
	}
	JumpDialog dlg(this, items);
	if (dlg.exec() == QDialog::Accepted) {
		JumpDialog::Action action = dlg.action();
		if (action == JumpDialog::Action::BranchsAndTags) {
			QString name = dlg.text();
			QString id = g->rev_parse(name);
			if (g->objectType(id) == "tag") {
				id = objcache_.getCommitIdFromTag(id);
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
			jumpToCommit(id);
		}
	}
}

void BasicMainWindow::jumpToCommit(QString id)
{
	GitPtr g = git();
	id = g->rev_parse(id);
	if (!id.isEmpty()) {
		int row = rowFromCommitId(id);
		setCurrentLogRow(row);
	}
}

void BasicMainWindow::checkout(QWidget *parent, Git::CommitItem const *commit)
{
	if (!commit) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QStringList tags;
	QStringList local_branches;
	QStringList remote_branches;
	{
		NamedCommitList named_commits = namedCommitItems(Branches | Tags | Remotes);
		JumpDialog::sort(&named_commits);
		for (NamedCommitItem const &item : named_commits) {
			if (item.id == commit->commit_id) {
				QString name = item.name;
				if (item.type == NamedCommitItem::Type::Tag) {
					tags.push_back(name);
				} else if (item.type == NamedCommitItem::Type::Branch) {
					int i = name.lastIndexOf('/');
					if (i < 0 && name == "HEAD") continue;
					if (i > 0 && name.mid(i + 1) == "HEAD") continue;
					if (item.remote.isNull()) {
						local_branches.push_back(name);
					} else {
						remote_branches.push_back(name);
					}
				}
			}
		}
	}

	CheckoutDialog dlg(parent, tags, local_branches, remote_branches);
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

void BasicMainWindow::checkout()
{
	checkout(this, selectedCommitItem());
}


void BasicMainWindow::deleteBranch(Git::CommitItem const *commit)
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
			if (item.name == "HEAD") continue;
			if (item.id == commit->commit_id) {
				current_local_branch_names.push_back(item.name);
			}
			all_local_branch_names.push_back(item.name);
		}
	}

	DeleteBranchDialog dlg(this, false, all_local_branch_names, current_local_branch_names);
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

void MainWindow::rebaseBranch(Git::CommitItem const *commit)
{
	if (!commit) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QString text = tr("Are you sure you want to rebase the commit ?");
	text += "\n\n";
	text += "> git rebase " + commit->commit_id;
	int r = QMessageBox::information(this, tr("Rebase"), text, QMessageBox::Ok, QMessageBox::Cancel);
	if (r == QMessageBox::Ok) {
		g->rebaseBranch(commit->commit_id);
		openRepository(true);
	}
}

void MainWindow::cherrypick(Git::CommitItem const *commit)
{
	if (!commit) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	g->cherrypick(commit->commit_id);
	openRepository(true);
}



void BasicMainWindow::deleteBranch()
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

bool BasicMainWindow::runOnRepositoryDir(std::function<void(QString)> callback, RepositoryItem const *repo)
{
	if (!repo) {
		repo = &current_repo_;
	}
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

void BasicMainWindow::openTerminal(RepositoryItem const *repo)
{
	runOnRepositoryDir([](QString dir){
#ifdef Q_OS_MAC
		if (!isValidDir(dir)) return;
		QString cmd = "open -n -a /Applications/Utilities/Terminal.app --args \"%1\"";
		cmd = cmd.arg(dir);
		QProcess::execute(cmd);
#else
		Terminal::open(dir);
#endif
	}, repo);
}

void BasicMainWindow::openExplorer(RepositoryItem const *repo)
{
	runOnRepositoryDir([](QString dir){
#ifdef Q_OS_MAC
		if (!isValidDir(dir)) return;
		QString cmd = "open \"%1\"";
		cmd = cmd.arg(dir);
		QProcess::execute(cmd);
#else
		QDesktopServices::openUrl(dir);
#endif
	}, repo);
}

void MainWindow::on_toolButton_terminal_clicked()
{
	openTerminal(nullptr);
}

void MainWindow::on_toolButton_explorer_clicked()
{
	openExplorer(nullptr);
}

void BasicMainWindow::pushSetUpstream(QString const &remote, QString const &branch)
{
	if (remote.isEmpty()) return;
	if (branch.isEmpty()) return;

	int exitcode = 0;
	QString errormsg;

	reopenRepository(true, [&](GitPtr g){
		g->push_u(remote, branch, &pty_process_);
		while (1) {
			if (pty_process_.wait(1)) break;
			QApplication::processEvents();
		}
		exitcode = pty_process_.getExitCode();
		errormsg = pty_process_.getMessage();
	});

	if (exitcode == 128) {
		if (errormsg.indexOf("Connection refused") >= 0) {
			QMessageBox::critical(this, qApp->applicationName(), tr("Connection refused."));
			return;
		}
	}

	updateRemoteInfo();
}

bool BasicMainWindow::pushSetUpstream(bool testonly)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return false;
	QStringList remotes = g->getRemotes();

	QString current_branch = currentBranchName();

	QStringList branches;
	for (Git::Branch const &b : g->branches()) {
		branches.push_back(b.name);
	}

	if (remotes.isEmpty() || branches.isEmpty()) {
		return false;
	}

	if (testonly) {
		return true;
	}

	PushDialog dlg(this, remotes, branches, PushDialog::RemoteBranch(QString(), current_branch));
	if (dlg.exec() == QDialog::Accepted) {
		PushDialog::Action a = dlg.action();
		if (a == PushDialog::PushSimple) {
			push();
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

void BasicMainWindow::createRepository(QString const &dir)
{
	CreateRepositoryDialog dlg(this, dir);
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
					QString remote_name = dlg.remoteName();
					QString remote_url = dlg.remoteURL();
					if (!remote_name.isEmpty() && !remote_url.isEmpty()) {
						g->addRemoteURL(remote_name, remote_url);
					}
				}
			}
		} else {
			// not dir
		}
	}
}

void MainWindow::on_action_create_a_repository_triggered()
{
	createRepository(QString());
}

bool MainWindow::isRemoteOnline() const
{
	return ui->radioButton_remote_online->isChecked();
}

void MainWindow::setNetworkingCommandsEnabled(bool f)
{
	ui->action_clone->setEnabled(f);

	ui->toolButton_clone->setEnabled(f);

	if (!Git::isValidWorkingCopy(currentWorkingCopyDir())) {
		f = false;
	}

	ui->action_fetch->setEnabled(f);
	ui->action_pull->setEnabled(f);
	ui->action_push->setEnabled(f);

	ui->toolButton_fetch->setEnabled(f);
	ui->toolButton_pull->setEnabled(f);
	ui->toolButton_push->setEnabled(f);
}

void MainWindow::setRemoteOnline(bool f)
{
	QRadioButton *rb = nullptr;
	rb = f ? ui->radioButton_remote_online : ui->radioButton_remote_offline;
	rb->blockSignals(true);
	rb->click();
	rb->blockSignals(false);

	setNetworkingCommandsEnabled(f);

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

void BasicMainWindow::stopPtyProcess()
{
	pty_process_.stop();
	QDir::setCurrent(starting_dir_);
}

void BasicMainWindow::abortPtyProcess()
{
	stopPtyProcess();
	pty_process_ok_ = false;
	interaction_canceled_ = true;
}

void MainWindow::on_toolButton_stop_process_clicked()
{
	abortPtyProcess();
}

void MainWindow::on_action_stop_process_triggered()
{
	abortPtyProcess();
}

void MainWindow::on_action_exit_triggered()
{
	close();
}

void MainWindow::on_action_reflog_triggered()
{
	GitPtr g = git();
	Git::ReflogItemList reflog;
	g->reflog(&reflog);

	ReflogWindow dlg(this, this, reflog);
	dlg.exec();
}

void MainWindow::blame(QListWidgetItem *item)
{
	QList<BlameItem> list;
	QString path = getFilePath(item);
	{
		GitPtr g = git();
		QByteArray ba = g->blame(path);
		if (!ba.isEmpty()) {
			char const *begin = ba.data();
			char const *end = begin + ba.size();
			list = BlameWindow::parseBlame(begin, end);
		}
	}
	if (!list.isEmpty()) {
		qApp->setOverrideCursor(Qt::WaitCursor);
		BlameWindow win(this, path, list);
		qApp->restoreOverrideCursor();
		win.exec();
	}
}

void MainWindow::blame()
{
	blame(currentFileItem());
}

void BasicMainWindow::execCommitViewWindow(Git::CommitItem const *commit)
{
	CommitViewWindow win(this, commit);
	win.exec();
}

void MainWindow::on_action_repository_property_triggered()
{
	execRepositoryPropertyDialog(currentWorkingCopyDir());
}

void MainWindow::on_action_set_gpg_signing_triggered()
{
	GitPtr g = git();
	QString global_key_id = g->signingKey(Git::Source::Global);
	QString repository_key_id;
	if (g->isValidWorkingCopy()) {
		repository_key_id = g->signingKey(Git::Source::Local);
	}
	SetGpgSigningDialog dlg(this, currentRepositoryName(), global_key_id, repository_key_id);
	if (dlg.exec() == QDialog::Accepted) {
		g->setSigningKey(dlg.id(), dlg.isGlobalChecked());
	}
}

void MainWindow::execAreYouSureYouWantToContinueConnectingDialog()
{
	using TheDlg = AreYouSureYouWantToContinueConnectingDialog;

	interaction_mode_ = InteractionMode::Busy;

	QApplication::restoreOverrideCursor();

	TheDlg dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		TheDlg::Result r = dlg.result();
		if (r == TheDlg::Result::Yes) {
			pty_process_.writeInput("yes\n", 4);
		} else {
			pty_process_ok_ = false; // abort
			pty_process_.writeInput("no\n", 3);
			QThread::msleep(300);
			stopPtyProcess();
		}
	} else {
		ui->widget_log->setFocus();
		interaction_canceled_ = true;
	}
	interaction_mode_ = InteractionMode::Busy;
}

void MainWindow::onLogIdle()
{
	if (interaction_canceled_) return;
	if (interaction_mode_ != InteractionMode::None) return;

	static char const are_you_sure_you_want_to_continue_connecting[] = "Are you sure you want to continue connecting (yes/no)?";
	static char const enter_passphrase[] = "Enter passphrase: ";
	static char const enter_passphrase_for_key[] = "Enter passphrase for key '";
	static char const fatal_authentication_failed_for[] = "fatal: Authentication failed for '";

	std::vector<char> vec;
	ui->widget_log->retrieveLastText(&vec, 100);
	if (!vec.empty()) {
		std::string line;
		int n = (int)vec.size();
		int i = n;
		while (i > 0) {
			i--;
			if (i + 1 < n && vec[i] == '\n') {
				i++;
				line.assign(&vec[i], n - i);
				break;
			}
		}
		if (!line.empty()) {
			auto ExecLineEditDialog = [&](QWidget *parent, QString const &title, QString const &prompt, QString const &val, bool password){
				LineEditDialog dlg(parent, title, prompt, val, password);
				if (dlg.exec() == QDialog::Accepted) {
					std::string ret = dlg.text().toStdString();
					std::string str = ret + '\n';
					pty_process_.writeInput(str.c_str(), str.size());
					return ret;
				}
				abortPtyProcess();
				return std::string();
			};

			auto Match = [&](char const *str){
				int n = strlen(str);
				if (strncmp(line.c_str(), str, n) == 0) {
					char const *p = line.c_str() + n;
					while (1) {
						if (!*p) return true;
						if (!isspace((unsigned char)*p)) break;
						p++;
					}
				}
				return false;
			};

			auto StartsWith = [&](char const *str){
				char const *p = line.c_str();
				while (*str) {
					if (*p != *str) return false;
					str++;
					p++;
				}
				return true;
			};

			if (Match(are_you_sure_you_want_to_continue_connecting)) {
				execAreYouSureYouWantToContinueConnectingDialog();
				return;
			}

			if (line == enter_passphrase) {
				ExecLineEditDialog(this, "Passphrase", QString::fromStdString(line), QString(), true);
				return;
			}

			if (StartsWith(enter_passphrase_for_key)) {
				std::string keyfile;
				{
					int i = strlen(enter_passphrase_for_key);
					char const *p = line.c_str() + i;
					char const *q = strrchr(p, ':');
					if (q && p + 2 < q && q[-1] == '\'') {
						keyfile.assign(p, q - 1);
					}
				}
				if (!keyfile.empty()) {
					if (keyfile == ssh_passphrase_for_ && !ssh_passphrase_secret_.empty()) {
						std::string text = ssh_passphrase_secret_ + '\n';
						pty_process_.writeInput(text.c_str(), text.size());
					} else {
						ssh_passphrase_for_ = keyfile;
						std::string s = ExecLineEditDialog(this, "Passphrase for key", QString::fromStdString(line), QString(), true);
						ssh_passphrase_secret_ = s;
					}
					return;
				}
			}

			char const *begin = line.c_str();
			char const *end = line.c_str() + line.size();
			auto Input = [&](QString const &title, bool password, std::string *value){
				std::string header = QString("%1 for '").arg(title).toStdString();
				if (strncmp(begin, header.c_str(), header.size()) == 0) {
					QString msg;
					if (memcmp(end - 2, "':", 2) == 0) {
						msg = QString::fromUtf8(begin, end - begin - 1);
					} else if (memcmp(end - 3, "': ", 3) == 0) {
						msg = QString::fromUtf8(begin, end - begin - 2);
					}
					if (!msg.isEmpty()) {
						std::string s = ExecLineEditDialog(this, title, msg, value ? QString::fromStdString(*value) : QString(), password);
						if (value) *value = s;
						return true;
					}
				}
				return false;
			};
			std::string uid = http_uid_;
			std::string pwd = http_pwd_;
			if (Input("Username", false, &uid)) {
				http_uid_ = uid;
				return;
			}
			if (Input("Password", true, &pwd)) {
				http_pwd_ = pwd;
				return;
			}

			if (StartsWith(fatal_authentication_failed_for)) {
				QMessageBox::critical(this, tr("Authentication Failed"), QString::fromStdString(line));
				abortPtyProcess();
				return;
			}
		}
	}
}

QList<Git::Tag> BasicMainWindow::queryTagList()
{
	QList<Git::Tag> list;
	Git::CommitItem const *commit = selectedCommitItem();
	if (commit && Git::isValidID(commit->commit_id)) {
		list = findTag(commit->commit_id);
	}
	return list;
}

void MainWindow::on_action_edit_tags_triggered()
{
	Git::CommitItem const *commit = selectedCommitItem();
	if (commit && Git::isValidID(commit->commit_id)) {
		EditTagsDialog dlg(this, commit);
		dlg.exec();
	}
}

void MainWindow::on_action_push_u_triggered()
{
    pushSetUpstream(false);
}

QStringList MainWindow::remoteBranches(QString const &id)
{
	QStringList list;

	GitPtr g = git();
	if (isValidWorkingCopy(g)) {
		NamedCommitList named_commits = namedCommitItems(Branches | Remotes);
		JumpDialog::sort(&named_commits);
		for (NamedCommitItem const &item : named_commits) {
			if (item.id == id && !item.remote.isEmpty()) {
				list.push_back(item.remote / item.name);
			}
		}
	}

	return list;
}

void MainWindow::deleteRemoteBranch(Git::CommitItem const *commit)
{
	if (!commit) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QStringList remote_branches = remoteBranches(commit->commit_id);
	if (remote_branches.isEmpty()) return;

	DeleteBranchDialog dlg(this, true, QStringList(), remote_branches);
	if (dlg.exec() == QDialog::Accepted) {
		setLogEnabled(g, true);
		QStringList names = dlg.selectedBranchNames();
		for (QString const &name : names) {
			int i = name.indexOf('/');
			if (i > 0) {
				QString remote = name.mid(0, i);
				QString branch = ':' + name.mid(i + 1);
				pushSetUpstream(remote, branch);
			}
		}
	}
}

void MainWindow::on_action_delete_remote_branch_triggered()
{
	deleteRemoteBranch(selectedCommitItem());
}

void MainWindow::rebaseOnto()
{
	struct Commit {
		QStringList parents;
		QStringList children;
	};

	std::map<QString, Commit> commit_map;

	Git::CommitItem const *commit = selectedCommitItem();
	if (commit) {
		for (Git::CommitItem const &item : logs_) {
			Commit c;
			c.parents = item.parent_ids;
			commit_map[item.commit_id] = c;
		}
		for (Git::CommitItem const &item : logs_) {
			for (QString const &parent : item.parent_ids) {
				auto it = commit_map.find(parent);
				if (it != commit_map.end()) {
					it->second.children.push_back(item.commit_id);
				}
			}
		}
		QString upstream;
		QString id = head_id_;
		while (1) {
			auto it = commit_map.find(id);
			if (it == commit_map.end()) break;
			Commit const &c = it->second;
			if (c.parents.size() > 1) goto done;
			if (c.parents.size() != 1) break;
			if (id != head_id_) {
				if (c.children.size() > 1) goto done;
				if (!findTag(id).isEmpty()) goto done;
				auto it2 = branch_map_.find(id);
				if (it2 != branch_map_.end()) {
					QList<Git::Branch> b = it2->second;
					if (!it2->second.empty()) goto done;
				}
			}
			id = c.parents.first();
			continue;
done:;
			upstream = id;
			break;
		}

		QString newbase = commit->commit_id;
		QString branch = head_id_;

		RebaseOntoDialog dlg(this);
		if (dlg.exec(newbase, upstream, branch) == QDialog::Accepted) {
			reopenRepository(true, [&](GitPtr g){
				pty_condition_ = PtyCondition::Pull;
				pty_process_ok_ = true;
				g->rebaseOnto(newbase, upstream, branch, &pty_process_);
				while (1) {
					if (pty_process_.wait(1)) break;
					QApplication::processEvents();
				}
			});
		}
	}
}

void MainWindow::on_action_rebase_onto_triggered()
{
	rebaseOnto();
}

void BasicMainWindow::checkRemoteUpdate()
{
	if (pty_process_.isRunning()) return;

	emit signalCheckRemoteUpdate();
}

void MainWindow::on_action_test_triggered()
{
}

