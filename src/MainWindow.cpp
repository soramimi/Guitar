#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "AboutDialog.h"
#include "ApplicationGlobal.h"
#include "AreYouSureYouWantToContinueConnectingDialog.h"
#include "AvatarLoader.h"
#include "BlameWindow.h"
#include "CommitPropertyDialog.h"
#include "DeleteBranchDialog.h"
#include "EditGitIgnoreDialog.h"
#include "EditTagsDialog.h"
#include "FileDiffWidget.h"
#include "GitDiff.h"
#include "JumpDialog.h"
#include "LineEditDialog.h"
#include "MySettings.h"
#include "ReflogWindow.h"
#include "RemoteWatcher.h"
#include "SetGpgSigningDialog.h"
#include "SettingsDialog.h"
#include "StatusLabel.h"
#include "TextEditDialog.h"
#include "common/joinpath.h"
#include "common/misc.h"
#include "CloneFromGitHubDialog.h"
#include "ObjectBrowserDialog.h"
#include "platform.h"
#include "webclient.h"
#include <QClipboard>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QStandardPaths>
#include <QTimer>

FileDiffWidget::DrawData::DrawData()
{
	bgcolor_text = QColor(255, 255, 255);
	bgcolor_gray = QColor(224, 224, 224);
	bgcolor_add = QColor(192, 240, 192);
	bgcolor_del = QColor(255, 224, 224);
	bgcolor_add_dark = QColor(64, 192, 64);
	bgcolor_del_dark = QColor(240, 64, 64);
}

struct MainWindow::Private {
	bool is_online_mode = true;
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

	ui->splitter_v->setSizes({100, 400});
	ui->splitter_h->setSizes({200, 100, 200});

	m->status_bar_label = new StatusLabel(this);
	ui->statusBar->addWidget(m->status_bar_label);

	ui->widget_diff_view->bind(this);

	qApp->installEventFilter(this);

	ui->widget_log->setupForLogWidget(ui->verticalScrollBar_log, ui->horizontalScrollBar_log, themeForTextEditor());
	onLogVisibilityChanged();

	initNetworking();

	showFileList(FilesListType::SingleList);

	m->digits.load(":/image/digits.png");
	m->graph_color = global->theme->graphColorMap();

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

	connect(this, &BasicMainWindow::signalWriteLog, this, &BasicMainWindow::writeLog_);

	connect(ui->dockWidget_log, &QDockWidget::visibilityChanged, this, &MainWindow::onLogVisibilityChanged);
	connect(ui->widget_log, &TextEditorWidget::idle, this, &MainWindow::onLogIdle);

	connect(ui->treeWidget_repos, &RepositoriesTreeWidget::dropped, this, &MainWindow::onRepositoriesTreeDropped);

	connect((AbstractPtyProcess *)getPtyProcess(), &AbstractPtyProcess::completed, this, &MainWindow::onPtyProcessCompleted);

	connect(this, &BasicMainWindow::remoteInfoChanged, [&](){
		ui->lineEdit_remote->setText(currentRemoteName());
	});

	// リモート監視
	connect(this, &BasicMainWindow::signalCheckRemoteUpdate, &m->remote_watcher, &RemoteWatcher::checkRemoteUpdate);
	connect(&m->remote_watcher_timer, &QTimer::timeout, &m->remote_watcher, &RemoteWatcher::checkRemoteUpdate);
	connect(this, &MainWindow::updateButton, [&](){
		doUpdateButton();
	});
	m->remote_watcher.start(this);
	setRemoteMonitoringEnabled(true);

	connect(this, &MainWindow::signalSetRemoteChanged, [&](bool f){
		setRemoteChanged(f);
		updateButton();
	});

	//

	QString path = getBookmarksFilePath();
	*getReposPtr() = RepositoryBookmark::load(path);
	updateRepositoriesList();

	webContext()->set_keep_alive_enabled(true);
	getAvatarLoader()->start(this);
	connect(getAvatarLoader(), &AvatarLoader::updated, this, &MainWindow::onAvatarUpdated);

	*ptrUpdateFilesListCounter() = 0;

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

	setTabOrder(ui->treeWidget_repos, ui->widget_log);
//	setTabOrder(ui->widget_log, ui->treeWidget_repos);

	startTimers();
}

MainWindow::~MainWindow()
{
	stopPtyProcess();

	getAvatarLoader()->stop();

	m->remote_watcher.quit();
	m->remote_watcher.wait();

	delete m;
	delete ui;
}


void MainWindow::notifyRemoteChanged(bool f)
{
	postUserFunctionEvent([&](QVariant const &v){
		setRemoteChanged(v.toBool());
		updateButton();
	}, QVariant(f));
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
		auto *p1 = ptrUpdateCommitTableCounter();
		if (*p1 > 0) {
			if (*p1 > ms) {
				*p1 -= ms;
			} else {
				*p1 = 0;
				ui->tableWidget_log->viewport()->update();
			}
		}
		auto *p2 = ptrUpdateFilesListCounter();
		if (*p2 > 0) {
			if (*p2 > ms) {
				*p2 -= ms;
			} else {
				*p2 = 0;
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
	return BasicMainWindow::event(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
	QEvent::Type et = event->type();

	if (et == QEvent::KeyPress) {
		if (QApplication::activeModalWidget()) {
			// thru
		} else {
			auto *e = dynamic_cast<QKeyEvent *>(event);
			Q_ASSERT(e);
			int k = e->key();
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
		}
	} else if (et == QEvent::FocusIn) {
		auto SelectItem = [](QListWidget *w){
			int row = w->currentRow();
			if (row < 0) {
				row = 0;
				w->setCurrentRow(row);
			}
			w->setItemSelected(w->item(row), true);
			w->viewport()->update();
		};
		// ファイルリストがフォーカスを得たとき、diffビューを更新する。（コンテキストメニュー対応）
		if (watched == ui->listWidget_unstaged) {
			m->last_focused_file_list = watched;
			updateStatusBarText();
			updateUnstagedFileCurrentItem();
			SelectItem(ui->listWidget_unstaged);
			return true;
		}
		if (watched == ui->listWidget_staged) {
			m->last_focused_file_list = watched;
			updateStatusBarText();
			updateStagedFileCurrentItem();
			SelectItem(ui->listWidget_staged);
			return true;
		}
		if (watched == ui->listWidget_files) {
			m->last_focused_file_list = watched;
			SelectItem(ui->listWidget_files);
			return true;
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
	if (i >= 0 && i < diffResult()->size()) {
		Git::Diff const &diff = diffResult()->at(i);
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
	setInteractionCanceled(false);
}

void BasicMainWindow::writeLog(QString const &str)
{
	std::string s = str.toStdString();
	writeLog(s.c_str(), s.size());
}

void BasicMainWindow::writeLog_(QByteArray ba)
{
	if (!ba.isEmpty()) {
		writeLog(ba.data(), ba.size());
	}
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
	*getReposPtr() = std::move(newrepos);
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

RepositoryItem const *BasicMainWindow::findRegisteredRepository(QString *workdir) const
{
	*workdir = QDir(*workdir).absolutePath();
	workdir->replace('\\', '/');

	if (Git::isValidWorkingCopy(*workdir)) {
		for (RepositoryItem const &item : getRepos()) {
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
		if (i >= 0 && i < getRepos().size()) {
			return i;
		}
	}
	return -1;
}

RepositoryItem const *MainWindow::repositoryItem(QTreeWidgetItem const *item) const
{
	int row = repositoryIndex_(item);
	auto const &repos = getRepos();
	return (row >= 0 && row < repos.size()) ? &repos[row] : nullptr;
}

RepositoryItem const *MainWindow::selectedRepositoryItem() const
{
	return repositoryItem(ui->treeWidget_repos->currentItem());
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
	item->setIcon(0, getFolderIcon());
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	return item;
}

void MainWindow::updateRepositoriesList()
{
	QString path = getBookmarksFilePath();

	auto *repos = getReposPtr();
	*repos = RepositoryBookmark::load(path);

	QString filter = getRepositoryFilterText();

	ui->treeWidget_repos->clear();

	std::map<QString, QTreeWidgetItem *> parentmap;

	for (int i = 0; i < repos->size(); i++) {
		RepositoryItem const &repo = repos->at(i);
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
		child->setIcon(0, getRepositoryIcon());
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
	internalClearRepositoryInfo();
	ui->label_repo_name->setText(QString());
	ui->label_branch_name->setText(QString());
}

void MainWindow::setRepositoryInfo(QString const &reponame, QString const &brname)
{
	ui->label_repo_name->setText(reponame);
	ui->label_branch_name->setText(brname);
}

void MainWindow::updateFilesList(QString id, bool wait)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	if (!wait) return;

	clearFileList();

	Git::FileStatusList stats = g->status();
	setUncommitedChanges(!stats.empty());

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
		if (!makeDiff(uncommited ? QString() : id, diffResult())) {
			return;
		}

		std::map<QString, int> diffmap;

		for (int idiff = 0; idiff < diffResult()->size(); idiff++) {
			Git::Diff const &diff = diffResult()->at(idiff);
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
		if (!makeDiff(id, diffResult())) {
			return;
		}
		showFileList(files_list_type);
		addDiffItems(diffResult(), AddItem);
	}

	for (Git::Diff const &diff : *diffResult()) {
		QString key = GitDiff::makeKey(diff);
		(*getDiffCacheMap())[key] = diff;
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
	auto logs = getLogs();
	QTableWidgetItem *item = ui->tableWidget_log->item(selectedLogIndex(), 0);
	if (!item) return;
	int row = item->data(IndexRole).toInt();
	int count = (int)logs.size();
	if (row < count) {
		updateFilesList(logs[row], true);
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
		auto *item = new QTableWidgetItem(text);
		ui->tableWidget_log->setHorizontalHeaderItem(i, item);
	}

	updateCommitGraph(); // コミットグラフを更新
}

void MainWindow::detectGitServerType(GitPtr g)
{
	setServerType(ServerType::Standard);
	*ptrGitHub() = GitHubRepositoryInfo();

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
			auto *p = ptrGitHub();
			QString user = s.mid(0, i);
			QString repo = s.mid(i + 1);
			p->owner_account_name = user;
			p->repository_name = repo;
		}
		setServerType(ServerType::GitHub);
	}
}

bool BasicMainWindow::fetch(GitPtr g, bool prune)
{
	setPtyCondition(PtyCondition::Fetch);
	setPtyProcessOk(true);
	g->fetch(getPtyProcess(), prune);
	while (1) {
		if (getPtyProcess()->wait(1)) break;
		QApplication::processEvents();
	}
	return getPtyProcessOk();
}

void MainWindow::clearLog()
{
	clearLogs();
	clearLabelMap();
	setUncommitedChanges(false);
	ui->tableWidget_log->clearContents();
	ui->tableWidget_log->scrollToTop();
}

void MainWindow::openRepository_(GitPtr g)
{
	getObjCache()->setup(g);

	if (isValidWorkingCopy(g)) {

		bool do_fetch = isRemoteOnline() && (getForceFetch() || appsettings()->automatically_fetch_when_opening_the_repository);
		setForceFetch(false);
		if (do_fetch) {
			if (!fetch(g, false)) {
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
		setLogs(retrieveCommitLog(g));
		// ブランチを取得
		queryBranches(g);
		// タグを取得
		ptrTagMap()->clear();
		QList<Git::Tag> tags = g->tags();
		for (Git::Tag const &tag : tags) {
			Git::Tag t = tag;
			t.id = getObjCache()->getCommitIdFromTag(t.id);
			(*ptrTagMap())[t.id].push_back(t);
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

	setHeadId(getObjCache()->revParse("HEAD"));

	if (isThereUncommitedChanges()) {
		Git::CommitItem item;
		item.parent_ids.push_back(currentBranch().id);
		item.message = tr("Uncommited changes");
		auto p = getLogsPtr();
		p->insert(p->begin(), item);
	}

	prepareLogTableWidget();

	auto const &logs = getLogs();
	const int count = logs.size();

	ui->tableWidget_log->setRowCount(count);

	int selrow = -1;

	for (int row = 0; row < count; row++) {
		Git::CommitItem const *commit = &logs[row];
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
		bool isHEAD = (commit->commit_id == getHeadId());
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
			message_ex = makeCommitInfoText(row, &(*getLabelMap())[row]);
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

void MainWindow::removeSelectedRepositoryFromBookmark(bool ask)
{
	int i = indexOfRepository(ui->treeWidget_repos->currentItem());
	removeRepositoryFromBookmark(i, ask);
}

void MainWindow::doUpdateButton()
{
	setNetworkingCommandsEnabled(isRemoteOnline());

	ui->toolButton_fetch->setDot(getRemoteChanged());

	Git::Branch b = currentBranch();
	ui->toolButton_push->setNumber(b.ahead > 0 ? b.ahead : -1);
	ui->toolButton_pull->setNumber(b.behind > 0 ? b.behind : -1);
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
			auto const &logs = getLogs();
			int row = item->data(IndexRole).toInt();
			if (row < (int)logs.size()) {
				Git::CommitItem const &commit = logs[row];
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
		fetch(g, false);
	});
}

void MainWindow::on_action_fetch_prune_triggered()
{
	if (!isRemoteOnline()) return;

	reopenRepository(true, [&](GitPtr g){
		fetch(g, true);
	});
}

void MainWindow::on_action_push_triggered()
{
	push();
}

void MainWindow::on_action_pull_triggered()
{
	if (!isRemoteOnline()) return;

	reopenRepository(true, [&](GitPtr g){
		setPtyCondition(PtyCondition::Pull);
		setPtyProcessOk(true);
		g->pull(getPtyProcess());
		while (1) {
			if (getPtyProcess()->wait(1)) break;
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
		QAction *a_delrembranch = remoteBranches(commit->commit_id, nullptr).isEmpty() ? nullptr : menu.addAction(tr("Delete remote branch..."));

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

void BasicMainWindow::execFileHistory(QListWidgetItem *item)
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

	auto const &logs = getLogs();
	int row = item->data(IndexRole).toInt();
	if (row < (int)logs.size()) {
		updateStatusBarText();
		*ptrUpdateFilesListCounter() = 200;
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
	auto const &logs = getLogs();
	int i = ui->tableWidget_log->currentRow();
	if (i >= 0 && i < (int)logs.size()) {
		return i;
	}
	return -1;
}

void MainWindow::updateDiffView(QListWidgetItem *item)
{
	clearDiffView();

	m->last_selected_file_item = item;

	if (!item) return;

	int idiff = indexOfDiff(item);
	if (idiff >= 0 && idiff < diffResult()->size()) {
		Git::Diff const &diff = diffResult()->at(idiff);
		QString key = GitDiff::makeKey(diff);
		auto it = getDiffCacheMap()->find(key);
		if (it != getDiffCacheMap()->end()) {
			auto const &logs = getLogs();
			int row = ui->tableWidget_log->currentRow();
			bool uncommited = (row >= 0 && row < (int)logs.size() && Git::isUncommited(logs[row]));
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

void MainWindow::setWatchRemoteInterval(int mins)
{
	if (mins > 0) {
		m->remote_watcher_timer.start(mins * 60000);
	} else {
		m->remote_watcher_timer.stop();
	}
}

void MainWindow::setRemoteMonitoringEnabled(bool enable)
{
	if (enable) {
		setWatchRemoteInterval(appsettings()->watch_remote_changes_every_mins);
	} else {
		setWatchRemoteInterval(0);
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
	bool running = getPtyProcess()->isRunning();
	if (ui->toolButton_stop_process->isEnabled() != running) {
		ui->toolButton_stop_process->setEnabled(running);
		ui->action_stop_process->setEnabled(running);
		setNetworkingCommandsEnabled(!running);
	}
	if (!running) {
		setInteractionMode(InteractionMode::None);
	}

	while (1) {
		char tmp[1024];
		int len = getPtyProcess()->readOutput(tmp, sizeof(tmp));
		if (len < 1) break;
		writeLog(tmp, len);
	}
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
	if (QApplication::focusWidget() == ui->widget_log) {
		int c = event->key();

		auto write_char = [&](char c){
			if (getPtyProcess()->isRunning()) {
				getPtyProcess()->writeInput(&c, 1);
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

void MainWindow::on_action_edit_settings_triggered()
{
	SettingsDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		ApplicationSettings const &newsettings = dlg.settings();
		setAppSettings(newsettings);
		setGitCommand(appsettings()->git_command, false);
		setFileCommand(appsettings()->file_command, false);
		setGpgCommand(appsettings()->gpg_command, false);
		setRemoteMonitoringEnabled(true);
	}
}

void MainWindow::onCloneCompleted(bool success, QVariant const &userdata)
{
	if (success) {
		RepositoryItem r = userdata.value<RepositoryItem>();
		saveRepositoryBookmark(r);
		setCurrentRepository(r, false);
		openRepository(true);
	}
}

void MainWindow::onPtyProcessCompleted(bool /*ok*/, QVariant const &userdata)
{
	switch (getPtyCondition()) {
	case PtyCondition::Clone:
		onCloneCompleted(getPtyProcessOk(), userdata);
		break;
	}
	setPtyCondition(PtyCondition::None);
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
	ui->lineEdit_filter->setText(getRepositoryFilterText() + c);
}

void MainWindow::backspaceRepoFilter()
{
	QString s = getRepositoryFilterText();
	int n = s.size();
	if (n > 0) {
		s = s.mid(0, n - 1);
	}
	ui->lineEdit_filter->setText(s);
}

void MainWindow::on_lineEdit_filter_textChanged(QString const &text)
{
	setRepositoryFilterText(text);
	updateRepositoriesList();
}

void MainWindow::on_toolButton_erase_filter_clicked()
{
	clearRepoFilter();
	ui->lineEdit_filter->setFocus();
}

void MainWindow::deleteTags(QStringList const &tagnames)
{
	int row = ui->tableWidget_log->currentRow();

	internalDeleteTags(tagnames);

	ui->tableWidget_log->selectRow(row);
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

bool MainWindow::addTag(QString const &name)
{
	int row = ui->tableWidget_log->currentRow();

	bool ok = internalAddTag(name);

	ui->tableWidget_log->selectRow(row);
	return ok;
}

void MainWindow::on_action_push_all_tags_triggered()
{
	reopenRepository(false, [&](GitPtr g){
		g->push(true);
	});
}

void MainWindow::on_tableWidget_log_itemDoubleClicked(QTableWidgetItem *)
{
	Git::CommitItem const *commit = selectedCommitItem();
	if (commit) {
		execCommitPropertyDialog(this, commit);
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

void MainWindow::on_action_repo_jump_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	NamedCommitList items = namedCommitItems(Branches | Tags | Remotes);
	{
		NamedCommitItem head;
		head.name = "HEAD";
		head.id = getHeadId();
		items.push_front(head);
	}
	JumpDialog dlg(this, items);
	if (dlg.exec() == QDialog::Accepted) {
		QString text = dlg.text();
		if (text.isEmpty()) return;
		QString id = g->rev_parse(text);
		if (id.isEmpty() && Git::isValidID(text)) {
			QStringList list = findGitObject(text);
			if (list.isEmpty()) {
				QMessageBox::warning(this, tr("Jump"), QString("%1\n\n").arg(text) + tr("No such commit"));
				return;
			}
			ObjectBrowserDialog dlg2(this, list);
			if (dlg2.exec() == QDialog::Accepted) {
				id = dlg2.text();
				if (id.isEmpty()) return;
			}
		}
		if (g->objectType(id) == "tag") {
			id = getObjCache()->getCommitIdFromTag(id);
		}
		int row = rowFromCommitId(id);
		if (row < 0) {
			QMessageBox::warning(this, tr("Jump"), QString("%1\n(%2)\n\n").arg(text).arg(id) + tr("No such commit"));
		} else {
			setCurrentLogRow(row);
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

void MainWindow::on_action_repo_checkout_triggered()
{
	checkout();
}

void MainWindow::on_action_delete_branch_triggered()
{
	deleteBranch();
}

void MainWindow::on_toolButton_terminal_clicked()
{
	openTerminal(nullptr);
}

void MainWindow::on_toolButton_explorer_clicked()
{
	openExplorer(nullptr);
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
	createRepository(QString());
}

void MainWindow::setNetworkingCommandsEnabled(bool f)
{
	ui->action_clone->setEnabled(f);

	ui->toolButton_clone->setEnabled(f);

	if (!Git::isValidWorkingCopy(currentWorkingCopyDir())) {
		f = false;
	}

	ui->action_fetch->setEnabled(f);
	ui->action_fetch_prune->setEnabled(f);
	ui->action_pull->setEnabled(f);
	ui->action_push->setEnabled(f);
	ui->action_push_u->setEnabled(f);
	ui->action_push_all_tags->setEnabled(f);

	ui->toolButton_fetch->setEnabled(f);
	ui->toolButton_pull->setEnabled(f);
	ui->toolButton_push->setEnabled(f);
}

bool MainWindow::isRemoteOnline() const
{
	return m->is_online_mode;
}

void MainWindow::setRemoteOnline(bool f)
{
	m->is_online_mode = f;

	QRadioButton *rb = nullptr;
	rb = f ? ui->radioButton_remote_online : ui->radioButton_remote_offline;
	rb->blockSignals(true);
	rb->click();
	rb->blockSignals(false);

	ui->action_online->setCheckable(true);
	ui->action_offline->setCheckable(true);
	ui->action_online->setChecked(f);
	ui->action_offline->setChecked(!f);

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

	setInteractionMode(InteractionMode::Busy);

	QApplication::restoreOverrideCursor();

	TheDlg dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		TheDlg::Result r = dlg.result();
		if (r == TheDlg::Result::Yes) {
			getPtyProcess()->writeInput("yes\n", 4);
		} else {
			setPtyProcessOk(false); // abort
			getPtyProcess()->writeInput("no\n", 3);
			QThread::msleep(300);
			stopPtyProcess();
		}
	} else {
		ui->widget_log->setFocus();
		setInteractionCanceled(true);
	}
	setInteractionMode(InteractionMode::Busy);
}

void MainWindow::onLogIdle()
{
	if (interactionCanceled()) return;
	if (interactionMode() != InteractionMode::None) return;

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
					getPtyProcess()->writeInput(str.c_str(), str.size());
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
					if (keyfile == sshPassphraseUser() && !sshPassphrasePass().empty()) {
						std::string text = sshPassphrasePass() + '\n';
						getPtyProcess()->writeInput(text.c_str(), text.size());
					} else {
						std::string secret = ExecLineEditDialog(this, "Passphrase for key", QString::fromStdString(line), QString(), true);
						sshSetPassphrase(keyfile, secret);
					}
					return;
				}
			}

			char const *begin = line.c_str();
			char const *end = line.c_str() + line.size();
			auto Input = [&](QString const &title, bool password, std::string *value){
				Q_ASSERT(value);
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
						*value = s;
						return true;
					}
				}
				return false;
			};
			std::string uid = httpAuthenticationUser();
			std::string pwd = httpAuthenticationPass();
			bool ok = false;
			if (Input("Username", false, &uid)) ok = true;
			if (Input("Password", true, &pwd))  ok = true;
			if (ok) {
				httpSetAuthentication(uid, pwd);
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

void MainWindow::on_action_delete_remote_branch_triggered()
{
	deleteRemoteBranch(selectedCommitItem());
}

void MainWindow::on_action_terminal_triggered()
{
	auto const *repo = &currentRepository();
	openTerminal(repo);
}

void MainWindow::on_action_explorer_triggered()
{
	auto const *repo = &currentRepository();
	openExplorer(repo);
}

void MainWindow::on_action_reset_hard_triggered()
{
	doGitCommand([&](GitPtr g){
		g->reset_hard();
	});
}

void MainWindow::on_action_clean_df_triggered()
{
	doGitCommand([&](GitPtr g){
		g->clean_df();
	});
}

void MainWindow::postOpenRepositoryFromGitHub(QString const &username, QString const &reponame)
{
	QVariantList list;
	list.push_back(username);
	list.push_back(reponame);
	postUserFunctionEvent([&](QVariant const &v){
		QVariantList l = v.toList();
		QString uname = l[0].toString();
		QString rname = l[1].toString();
		CloneFromGitHubDialog dlg(this, uname, rname);
		if (dlg.exec() == QDialog::Accepted) {
			clone(dlg.url());
		}
	}, QVariant(list));
}

void MainWindow::on_action_stash_triggered()
{
	doGitCommand([&](GitPtr g){
		g->stash();
	});
}

void MainWindow::on_action_stash_apply_triggered()
{
	doGitCommand([&](GitPtr g){
		g->stash_apply();
	});
}

void MainWindow::on_action_stash_drop_triggered()
{
	doGitCommand([&](GitPtr g){
		g->stash_drop();
	});
}

void MainWindow::on_action_online_triggered()
{
	ui->radioButton_remote_online->click();
}

void MainWindow::on_action_offline_triggered()
{
	ui->radioButton_remote_offline->click();
}

void MainWindow::on_action_test_triggered()
{
}




