#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "AboutDialog.h"
#include "AddRepositoryDialog.h"
#include "ApplicationGlobal.h"
#include "AreYouSureYouWantToContinueConnectingDialog.h"
#include "BlameWindow.h"
#include "CheckoutDialog.h"
#include "CherryPickDialog.h"
#include "CleanSubModuleDialog.h"
#include "CloneDialog.h"
#include "CloneFromGitHubDialog.h"
#include "CommitDialog.h"
#include "CommitExploreWindow.h"
#include "CommitPropertyDialog.h"
#include "CommitViewWindow.h"
#include "ConfigUserDialog.h"
#include "CreateRepositoryDialog.h"
#include "DeleteBranchDialog.h"
#include "DoYouWantToInitDialog.h"
#include "EditGitIgnoreDialog.h"
#include "EditTagsDialog.h"
#include "FileHistoryWindow.h"
#include "FilePropertyDialog.h"
#include "FileUtil.h"
#include "FindCommitDialog.h"
#include "GitDiff.h"
#include "GitHubAPI.h"
#include "JumpDialog.h"
#include "LineEditDialog.h"
#include "MemoryReader.h"
#include "MergeDialog.h"
#include "MySettings.h"
#include "ObjectBrowserDialog.h"
#include "PushDialog.h"
#include "ReflogWindow.h"
#include "RepositoryPropertyDialog.h"
#include "SelectCommandDialog.h"
#include "SetGlobalUserDialog.h"
#include "SetGpgSigningDialog.h"
#include "SettingsDialog.h"
#include "StatusLabel.h"
#include "SubmoduleAddDialog.h"
#include "SubmoduleMainWindow.h"
#include "SubmoduleUpdateDialog.h"
#include "SubmodulesDialog.h"
#include "Terminal.h"
#include "TextEditDialog.h"
#include "UserEvent.h"
#include "WelcomeWizardDialog.h"
#include "coloredit/ColorDialog.h"
#include "common/misc.h"
#include "gunzip.h"
#include "platform.h"
#include "unix/UnixPtyProcess.h"
#include "webclient.h"
#include <QBuffer>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFontDatabase>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QShortcut>
#include <QStandardPaths>
#include <QTimer>
#include <chrono>
#include <fcntl.h>
#include <sys/stat.h>
#include <QProcess>
#include <thread>
#include "GitObjectManager.h"

#ifdef Q_OS_MAC
namespace {

bool isValidDir(QString const &dir)
{
	if (dir.indexOf('\"') >= 0 || dir.indexOf('\\') >= 0) return false;
	return QFileInfo(dir).isDir();
}

}

#include <QProcess>
#endif

struct EventItem {
	QObject *receiver = nullptr;
	QEvent *event = nullptr;
	QDateTime at;
	EventItem(QObject *receiver, QEvent *event, QDateTime const &at)
		: receiver(receiver)
		, event(event)
		, at(at)
	{
	}
};

struct MainWindow::Private {

	QIcon repository_icon;
	QIcon folder_icon;
	QIcon signature_good_icon;
	QIcon signature_dubious_icon;
	QIcon signature_bad_icon;
	QPixmap transparent_pixmap;

	QString starting_dir;
	Git::Context gcx;
	RepositoryData current_repo;

	Git::User current_git_user;

	QList<RepositoryData> repos;
	QList<Git::Diff> diff_result;
	QList<Git::SubmoduleItem> submodules;

	QStringList added;
	QStringList remotes;
	QString current_remote_name;
	Git::Branch current_branch;
	unsigned int temp_file_counter = 0;

	std::string ssh_passphrase_user;
	std::string ssh_passphrase_pass;

	std::string http_uid;
	std::string http_pwd;

	std::map<QString, GitHubAPI::User> committer_map; // key is email

	PtyProcess pty_process;
	bool pty_process_ok = false;
	MainWindow::PtyCondition pty_condition = MainWindow::PtyCondition::None;

	bool interaction_canceled = false;
	MainWindow::InteractionMode interaction_mode = MainWindow::InteractionMode::None;

	QString repository_filter_text;
	bool uncommited_changes = false;

//	bool remote_changed = false;

	GitHubRepositoryInfo github;

	Git::CommitID head_id;
	bool force_fetch = false;

	RepositoryData temp_repo_for_clone_complete;
	QVariant pty_process_completion_data;

	std::vector<EventItem> event_item_list;

	bool is_online_mode = true;
	QTimer interval_10ms_timer;
	QImage graph_color;
	QPixmap digits;
	StatusLabel *status_bar_label;

	QObject *last_focused_file_list = nullptr;

	QListWidgetItem *last_selected_file_item = nullptr;

	bool searching = false;
	QString search_text;

	int repos_panel_width = 0;

	std::set<QString> ancestors;

	QWidget *focused_widget = nullptr;
	QList<int> splitter_h_sizes;

	std::vector<char> log_history_bytes;

	QAction *action_edit_profile = nullptr;
	QAction *action_detect_profile = nullptr;

	int current_account_profiles = -1;
};

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, m(new Private)
{
	ui->setupUi(this);

	ui->frame_repository_wrapper->bind(this
									   , ui->tableWidget_log
									   , ui->listWidget_files
									   , ui->listWidget_unstaged
									   , ui->listWidget_staged
									   , ui->widget_diff_view
									   );

	loadApplicationSettings();
	setupExternalPrograms();
	m->starting_dir = QDir::current().absolutePath();

	{ // load graphic resources
		QFileIconProvider icons;
		m->folder_icon = icons.icon(QFileIconProvider::Folder);
		m->repository_icon = QIcon(":/image/repository.png");
		m->signature_good_icon = QIcon(":/image/signature-good.png");
		m->signature_bad_icon = QIcon(":/image/signature-bad.png");
		m->signature_dubious_icon = QIcon(":/image/signature-dubious.png");
		m->transparent_pixmap = QPixmap(":/image/transparent.png");
	}

#ifdef Q_OS_WIN
	ui->action_create_desktop_launcher_file->setText(tr("Create shortcut file..."));
#endif
#ifdef Q_OS_MACX
	ui->action_create_desktop_launcher_file->setVisible(false);
#endif

#ifdef Q_OS_MACX
	ui->action_about->setText("About Guitar...");
	ui->action_edit_settings->setText("Settings...");
#endif

	ui->splitter_v->setSizes({100, 400});
	ui->splitter_h->setSizes({200, 100, 200});

	m->status_bar_label = new StatusLabel(this);
	ui->statusBar->addWidget(m->status_bar_label);

	frame()->filediffwidget()->bind(this);

	qApp->installEventFilter(this);

	setShowLabels(appsettings()->show_labels, false);
	setShowGraph(appsettings()->show_graph, false);

	{
		QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		ui->widget_log->view()->setTextFont(font);
		ui->widget_log->view()->setSomethingBadFlag(true); // TODO:
	}
	ui->widget_log->view()->setupForLogWidget(themeForTextEditor());
	onLogVisibilityChanged();

	initNetworking();

	showFileList(FilesListType::SingleList);

	m->digits.load(":/image/digits.png");
	m->graph_color = global->theme->graphColorMap();

	frame()->prepareLogTableWidget();

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

	ui->widget_log->view()->setTextFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

	connect(this, &MainWindow::signalWriteLog, this, &MainWindow::writeLog_);

	connect(ui->dockWidget_log, &QDockWidget::visibilityChanged, this, &MainWindow::onLogVisibilityChanged);
	connect(ui->widget_log->view(), &TextEditorView::idle, this, &MainWindow::onLogIdle);

	connect(ui->treeWidget_repos, &RepositoriesTreeWidget::dropped, this, &MainWindow::onRepositoriesTreeDropped);

	connect((AbstractPtyProcess *)getPtyProcess(), &AbstractPtyProcess::completed, this, &MainWindow::onPtyProcessCompleted);

	connect(this, &MainWindow::remoteInfoChanged, [&](){
		ui->lineEdit_remote->setText(currentRemoteName());
	});

	// 右上のアイコンがクリックされたとき、ConfigUserダイアログを表示
	connect(ui->widget_avatar_icon, &SimpleImageWidget::clicked, this, &MainWindow::on_action_configure_user_triggered);

	connect(new QShortcut(QKeySequence("Ctrl+T"), this), &QShortcut::activated, this, &MainWindow::test);

	//

	QString path = getBookmarksFilePath();
	setRepos(RepositoryBookmark::load(path));
	updateRepositoriesList();

	// アイコン取得機能
	global->avatar_loader.connectAvatarReady(this, &MainWindow::avatarReady);

	connect(frame()->filediffwidget(), &FileDiffWidget::textcodecChanged, [&](){ updateDiffView(frame()); });

	if (!global->start_with_shift_key && appsettings()->remember_and_restore_window_position) {
		Qt::WindowStates state = windowState();
		MySettings settings;

		settings.beginGroup("MainWindow");
		bool maximized = settings.value("Maximized").toBool();
		restoreGeometry(settings.value("Geometry").toByteArray());
		settings.endGroup();
		if (maximized) {
			state |= Qt::WindowMaximized;
			setWindowState(state);
		}
	}

	ui->action_sidebar->setChecked(true);

	startTimers();
}

MainWindow::~MainWindow()
{
	global->avatar_loader.disconnectAvatarReady(this, &MainWindow::avatarReady);

	cancelPendingUserEvents();

	stopPtyProcess();

	deleteTempFiles();

	delete m;
	delete ui;
}

RepositoryWrapperFrame *MainWindow::frame()
{
	return ui->frame_repository_wrapper;
}

RepositoryWrapperFrame const *MainWindow::frame() const
{
	return ui->frame_repository_wrapper;
}

/**
 * @brief イベントをポストする
 * @param receiver 宛先
 * @param event イベント
 * @param ms_later 遅延時間（0なら即座）
 */
void MainWindow::postEvent(QObject *receiver, QEvent *event, int ms_later)
{
	if (ms_later <= 0) {
		QApplication::postEvent(this, event);
	} else {
		auto at = QDateTime::currentDateTime().addMSecs(ms_later);
		m->event_item_list.emplace_back(receiver, event, at);
		std::stable_sort(m->event_item_list.begin(), m->event_item_list.end(), [](EventItem const &l, EventItem const &r){
			return l.at > r.at; // 降順
		});
	}
}

/**
 * @brief ユーザー関数イベントをポストする
 * @param fn 関数
 * @param v QVariant
 * @param p ポインタ
 * @param ms_later 遅延時間（0なら即座）
 */
void MainWindow::postUserFunctionEvent(const std::function<void (const QVariant &, void *ptr)> &fn, const QVariant &v, void *p, int ms_later)
{
	postEvent(this, new UserFunctionEvent(fn, v, p), ms_later);
}

/**
 * @brief 未送信のイベントをすべて削除する
 */
void MainWindow::cancelPendingUserEvents()
{
	for (auto &item : m->event_item_list) {
		delete item.event;
	}
	m->event_item_list.clear();
}

/**
 * @brief 開始イベントをポストする
 */
void MainWindow::postStartEvent(int ms_later)
{
	postEvent(this, new StartEvent, ms_later);
}

/**
 * @brief インターバルタイマを開始する
 */
void MainWindow::startTimers()
{
	// タイマ開始
	connect(&m->interval_10ms_timer, &QTimer::timeout, this, &MainWindow::onInterval10ms);
	m->interval_10ms_timer.setInterval(10);
	m->interval_10ms_timer.start();
}


/**
 * @brief PTYプロセスの出力をログに書き込む
 */
void MainWindow::updatePocessLog(bool processevents)
{
	while (1) {
		char tmp[1024];
		int len = getPtyProcess()->readOutput(tmp, sizeof(tmp));
		if (len < 1) break;
		writeLog(tmp, len, true);
		if (processevents) {
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		}
	}
}

/**
 * @brief 10ms間隔のインターバルタイマ
 */
void MainWindow::onInterval10ms()
{
	{
		// ユーザーイベントの処理

		std::vector<EventItem> items; // 処理するイベント

		QDateTime now = QDateTime::currentDateTime(); // 現在時刻

		size_t i = m->event_item_list.size(); // 後ろから走査
		while (i > 0) {
			i--;
			if (m->event_item_list[i].at <= now) { // 予約時間を過ぎていたら
				items.push_back(m->event_item_list[i]); // 処理リストに追加
				m->event_item_list.erase(m->event_item_list.begin() + (int)i); // 処理待ちリストから削除
			}
		}

		// イベントをポストする
		for (auto it = items.rbegin(); it != items.rend(); it++) {
			QApplication::postEvent(it->receiver, it->event);
		}
	}

	{
		// PTYプロセスの監視

		bool running = getPtyProcess()->isRunning();
		if (ui->toolButton_stop_process->isEnabled() != running) {
			ui->toolButton_stop_process->setEnabled(running); // ボタンの状態を設定
			ui->action_stop_process->setEnabled(running);
			setNetworkingCommandsEnabled(!running);
		}
		if (!running) {
			setInteractionMode(InteractionMode::None);
		}

		// PTYプロセスの出力をログに書き込む
		updatePocessLog(true);
	}
}

bool MainWindow::shown()
{
	m->repos_panel_width = ui->stackedWidget_leftpanel->width();
	ui->stackedWidget_leftpanel->setCurrentWidget(ui->page_repos);
	ui->action_repositories_panel->setChecked(true);

	{
		MySettings settings;
		{
			settings.beginGroup("Remote");
			bool f = settings.value("Online", true).toBool();
			settings.endGroup();
			setRemoteOnline(f, false);
		}
		{
			settings.beginGroup("MainWindow");
			int n = settings.value("FirstColumnWidth", 50).toInt();
			if (n < 10) n = 50;
			frame()->logtablewidget()->setColumnWidth(0, n);
			settings.endGroup();
		}
	}
	updateUI();

	postStartEvent(100); // 開始イベント（100ms後）

	return true;
}

bool MainWindow::isUninitialized()
{
	return !misc::isExecutable(appsettings()->git_command);
}

void MainWindow::setupExternalPrograms()
{
	setGitCommand(appsettings()->git_command, false);
	setGpgCommand(appsettings()->gpg_command, false);
	setSshCommand(appsettings()->ssh_command, false);
}

void MainWindow::onStartEvent()
{
	if (isUninitialized()) { // gitコマンドの有効性チェック
		if (!execWelcomeWizardDialog()) { // ようこそダイアログを表示
			close(); // キャンセルされたらプログラム終了
		}
	}
	if (isUninitialized()) { // 正しく初期設定されたか
		postStartEvent(100); // 初期設定されなかったら、もういちどようこそダイアログを出す（100ms後）
	} else {
		// 外部コマンド登録
		setupExternalPrograms();

		// メインウィンドウのタイトルを設定
		updateWindowTitle(git());

		// プログラムバーション表示
		writeLog(AboutDialog::appVersion() + '\n', true);
		// gitコマンドバージョン表示
		logGitVersion();
	}
}

void MainWindow::setCurrentLogRow(RepositoryWrapperFrame *frame, int row)
{
	if (row >= 0 && row < frame->logtablewidget()->rowCount()) {
		updateStatusBarText(frame);
		frame->logtablewidget()->setFocus();
		frame->logtablewidget()->setCurrentCell(row, 2);
	}
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
				if (qApp->focusWidget() == ui->treeWidget_repos) {
					clearRepoFilter();
				} else if (centralWidget()->isAncestorOf(qApp->focusWidget())) {
					ui->treeWidget_repos->setFocus();
					return true;
				}
			}
			if (e->modifiers() & Qt::ControlModifier) {
				if (k == Qt::Key_Up || k == Qt::Key_Down) {
					int rows = frame()->logtablewidget()->rowCount();
					int row = frame()->logtablewidget()->currentRow();
					if (k == Qt::Key_Up) {
						if (row > 0) {
							row--;
						}
					} else if (k == Qt::Key_Down) {
						if (row + 1 < rows) {
							row++;
						}
					}
					frame()->logtablewidget()->setCurrentCell(row, 0);
					return true;
				}
			}
			if (watched == ui->treeWidget_repos) {
				if (k == Qt::Key_Enter || k == Qt::Key_Return) {
					openSelectedRepository();
					return true;
				}
				if (!(e->modifiers() & Qt::ControlModifier)) {
					if (k >= 0 && k < 128 && QChar((uchar)k).isPrint()) {
						appendCharToRepoFilter(k);
						return true;
					}
					if (k == Qt::Key_Backspace) {
						backspaceRepoFilter();
						return true;
					}
				}
			} else if (watched == frame()->logtablewidget()) {
				if (k == Qt::Key_Home) {
					setCurrentLogRow(frame(), 0);
					return true;
				}
				if (k == Qt::Key_Escape) {
					ui->treeWidget_repos->setFocus();
					return true;
				}
			} else if (watched == frame()->fileslistwidget() || watched == frame()->unstagedFileslistwidget() || watched == frame()->stagedFileslistwidget()) {
				if (k == Qt::Key_Escape) {
					frame()->logtablewidget()->setFocus();
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
			auto *item = w->item(row);
			if (item) {
				item->setSelected(true);
				w->viewport()->update();
			}
		};
		// ファイルリストがフォーカスを得たとき、diffビューを更新する。（コンテキストメニュー対応）
		if (watched == frame()->unstagedFileslistwidget()) {
			m->last_focused_file_list = watched;
			updateStatusBarText(frame());
//			updateUnstagedFileCurrentItem(frame());
//			SelectItem(frame()->unstagedFileslistwidget());
			return true;
		}
		if (watched == frame()->stagedFileslistwidget()) {
			m->last_focused_file_list = watched;
			updateStatusBarText(frame());
//			updateStagedFileCurrentItem(frame());
//			SelectItem(frame()->stagedFileslistwidget());
			return true;
		}
		if (watched == frame()->fileslistwidget()) {
			m->last_focused_file_list = watched;
//			SelectItem(frame()->fileslistwidget());
			return true;
		}
	}
	return false;
}

bool MainWindow::event(QEvent *event)
{
	QEvent::Type et = event->type();
	if (et == QEvent::KeyPress) {
		auto *e = dynamic_cast<QKeyEvent *>(event);
		Q_ASSERT(e);
		int k = e->key();
		if (k == Qt::Key_Delete) {
			if (qApp->focusWidget() == ui->treeWidget_repos) {
				removeSelectedRepositoryFromBookmark(true);
				return true;
			}
		}
	} else if (et == (QEvent::Type)UserEvent::UserFunction) {
		if (auto *e = (UserFunctionEvent *)event) {
			e->func(e->var, e->ptr);
			return true;
		}
	}
	return QMainWindow::event(event);
}

void MainWindow::customEvent(QEvent *e)
{
	if (e->type() == (QEvent::Type)UserEvent::Start) {
		onStartEvent();
		return;
	}
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	MySettings settings;

	if (appsettings()->remember_and_restore_window_position) {
		setWindowOpacity(0);
		Qt::WindowStates state = windowState();
		bool maximized = (state & Qt::WindowMaximized) != 0;
		if (maximized) {
			state &= ~Qt::WindowMaximized;
			setWindowState(state);
		}
		{
			settings.beginGroup("MainWindow");
			settings.setValue("Maximized", maximized);
			settings.setValue("Geometry", saveGeometry());
			settings.endGroup();
		}
	}

	{
		settings.beginGroup("MainWindow");
		settings.setValue("FirstColumnWidth", frame()->logtablewidget()->columnWidth(0));
		settings.endGroup();
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

void MainWindow::onLogVisibilityChanged()
{
	ui->action_window_log->setChecked(ui->dockWidget_log->isVisible());
}

void MainWindow::appendLogHistory(char const *ptr, int len)
{
	m->log_history_bytes.insert(m->log_history_bytes.begin(), ptr, ptr + len);
}

std::vector<std::string> MainWindow::getLogHistoryLines()
{
	std::vector<std::string> lines;
	if (m->log_history_bytes.empty()) return {};
	char const *begin = m->log_history_bytes.data();
	char const *end = begin + m->log_history_bytes.size();
	char const *top = begin;
	char const *ptr = begin;
	bool cr = false;
	while (1) {
		int c = -1;
		if (ptr < end) {
			c = (unsigned char)*ptr;
		}
		if (c == '\r' || c == '\n' || c == -1) {
			if (cr && c == '\n') {
				// nop
			} else {
				std::string line(top, ptr - top);
				lines.push_back(line);
				if (c == -1) break;
			}
			cr = (c == '\r');
			ptr++;
			top = ptr;
		} else {
			ptr++;
		}
	}
	m->log_history_bytes.erase(m->log_history_bytes.begin(), m->log_history_bytes.begin() + (ptr - begin));
	return lines;
}

void MainWindow::clearLogHistory()
{
//	if (m->log_history_bytes.empty()) return;
	m->log_history_bytes.clear();
//	qDebug() << "---";
}

void MainWindow::internalWriteLog(char const *ptr, int len, bool record)
{
	if (record) { // 受信ログのみ記録
		appendLogHistory(ptr, len);
	}

	ui->widget_log->view()->logicalMoveToBottom();
	ui->widget_log->view()->appendBulk(ptr, len);
	ui->widget_log->view()->setChanged(false);
	ui->widget_log->updateLayoutAndMoveToBottom();

	setInteractionCanceled(false);
}

void MainWindow::buildRepoTree(QString const &group, QTreeWidgetItem *item, QList<RepositoryData> *repos)
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
		RepositoryData const *repo = repositoryItem(item);
		if (repo) {
			RepositoryData newrepo = *repo;
			newrepo.name = name;
			newrepo.group = group;
			item->setData(0, IndexRole, repos->size());
			repos->push_back(newrepo);
		}
	}
}

void MainWindow::refrectRepositories()
{
	QList<RepositoryData> newrepos;
	int n = ui->treeWidget_repos->topLevelItemCount();
	for (int i = 0; i < n; i++) {
		QTreeWidgetItem *item = ui->treeWidget_repos->topLevelItem(i);
		buildRepoTree(QString(), item, &newrepos);
	}
	setRepos(newrepos);
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
	return appsettings()->default_working_dir;
}

//WebContext *MainWindow::webContext()
//{
//	return &m->webcx;
//}

QIcon MainWindow::verifiedIcon(char s) const
{
	Git::SignatureGrade g = Git::evaluateSignature(s);
	switch (g) {
	case Git::SignatureGrade::Good:
		return m->signature_good_icon;
	case Git::SignatureGrade::Bad:
		return m->signature_bad_icon;
	case Git::SignatureGrade::Unknown:
	case Git::SignatureGrade::Dubious:
	case Git::SignatureGrade::Missing:
		return m->signature_dubious_icon;
	}
	return QIcon();
}

QAction *MainWindow::addMenuActionProperty(QMenu *menu)
{
	return menu->addAction(tr("&Property"));
}

QString MainWindow::currentWorkingCopyDir() const
{
	return m->current_repo.local_dir;
}

/**
 * @brief サブモジュール情報を取得する
 * @param path
 * @param commit コミット情報を取得（nullptr可）
 * @return
 */
Git::SubmoduleItem const *MainWindow::querySubmoduleByPath(const QString &path, Git::CommitItem *commit)
{
	if (commit) *commit = {};
	for (auto const &submod : m->submodules) {
		if (submod.path == path) {
			if (commit) {
				GitPtr g = git(submod);
				auto c = g->queryCommit(submod.id);
				if (c) *commit = *c;
			}
			return &submod;
		}
	}
	return nullptr;
}

QColor MainWindow::color(unsigned int i)
{
	unsigned int n = (unsigned int)m->graph_color.width();
	if (n > 0) {
		n--;
		if (i > n) i = n;
		QRgb const *p = reinterpret_cast<QRgb const *>(m->graph_color.scanLine(0));
		return QColor(qRed(p[i]), qGreen(p[i]), qBlue(p[i]));
	}
	return Qt::black;
}

RepositoryData const *MainWindow::findRegisteredRepository(QString *workdir) const
{
	*workdir = QDir(*workdir).absolutePath();
	workdir->replace('\\', '/');

	if (Git::isValidWorkingCopy(*workdir)) {
		for (RepositoryData const &item : cRepositories()) {
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

bool MainWindow::git_log_callback(void *cookie, const char *ptr, int len)
{
	auto *mw = (MainWindow *)cookie;
	mw->emitWriteLog(QByteArray(ptr, len), false);
	return true;
}

bool MainWindow::execSetGlobalUserDialog()
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

bool MainWindow::addExistingLocalRepository(QString dir, QString name, QString sshkey, bool open)
{
	if (dir.endsWith(".git")) {
		auto i = dir.size();
		if (i > 4) {
			ushort c = dir.utf16()[i - 5];
			if (c == '/' || c == '\\') {
				dir = dir.mid(0, i - 5);
			}
		}
	}

	if (!Git::isValidWorkingCopy(dir)) {
		if (QFileInfo(dir).isDir()) {
			QString text;
			text += tr("The folder is not a valid git repository.") + '\n';
			text += '\n';
			text += dir + '\n';
			text += '\n';
			text += tr("Do you want to initialize it as a git repository?") + '\n';
			int r = QMessageBox::information(this, tr("Initialize Repository") , text, QMessageBox::Yes, QMessageBox::No);
			if (r == QMessageBox::Yes) {
				createRepository(dir);
			}
		}
		return false;
	}

	if (name.isEmpty()) {
		name = makeRepositoryName(dir);
	}

	RepositoryData item;
	item.local_dir = dir;
	item.name = name;
	item.ssh_key = sshkey;
	saveRepositoryBookmark(item);

	if (open) {
		setCurrentRepository(item, true);
		GitPtr g = git(item.local_dir, {}, sshkey);
		openRepository_(g);
	}

	return true;
}

bool MainWindow::execWelcomeWizardDialog()
{
	WelcomeWizardDialog dlg(this);
	dlg.set_git_command_path(appsettings()->git_command);
	dlg.set_default_working_folder(appsettings()->default_working_dir);
	if (misc::isExecutable(appsettings()->git_command)) {
		gitCommand() = appsettings()->git_command;
		Git g(m->gcx, {}, {}, {});
		Git::User user = g.getUser(Git::Source::Global);
		dlg.set_user_name(user.name);
		dlg.set_user_email(user.email);
	}
	if (dlg.exec() == QDialog::Accepted) {
		setGitCommand(dlg.git_command_path(), false);
		appsettings()->default_working_dir = dlg.default_working_folder();
		saveApplicationSettings();

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

void MainWindow::execRepositoryPropertyDialog(const RepositoryData &repo, bool open_repository_menu)
{
	QString workdir = repo.local_dir;

	if (workdir.isEmpty()) {
		workdir = currentWorkingCopyDir();
	}
	QString name = repo.name;
	if (name.isEmpty()) {
		name = makeRepositoryName(workdir);
	}
	GitPtr g = git(workdir, {}, repo.ssh_key);
	RepositoryPropertyDialog dlg(this, &m->gcx, g, repo, open_repository_menu);
	dlg.exec();
	if (dlg.isRemoteChanged()) {
		emit remoteInfoChanged();
	}
	if (dlg.isNameChanged()) {
		this->changeRepositoryBookmarkName(repo, dlg.getName());
	}
}

void MainWindow::execConfigUserDialog(const Git::User &global_user, const Git::User &local_user, bool enable_local_user, const QString &reponame)
{
	ConfigUserDialog dlg(this, global_user, local_user, enable_local_user, reponame);
	if (dlg.exec() == QDialog::Accepted) {
		GitPtr g = git();
		Git::User user;

		// global
		user = dlg.user(true);
		if (user) {
			if (user.email != global_user.email || user.name != global_user.name) {
				g->setUser(user, true);
			}
		}

		// local
		if (dlg.isLocalUnset()) {
			g->setUser({}, false);
		} else {
			user = dlg.user(false);
			if (user) {
				if (user.email != local_user.email || user.name != local_user.name) {
					g->setUser(user, false);
				}
			}
		}

		updateWindowTitle(g);
	}
}

void MainWindow::setGitCommand(QString const &path, bool save)
{
	appsettings()->git_command = m->gcx.git_command = executableOrEmpty(path);

	internalSaveCommandPath(path, save, "GitCommand");
}

void MainWindow::setGpgCommand(QString const &path, bool save)
{
	appsettings()->gpg_command = executableOrEmpty(path);

	internalSaveCommandPath(appsettings()->gpg_command, save, "GpgCommand");
	if (!global->appsettings.gpg_command.isEmpty()) {
		GitPtr g = git();
		g->configGpgProgram(global->appsettings.gpg_command, true);
	}
}

void MainWindow::setSshCommand(QString const &path, bool save)
{
	appsettings()->ssh_command = m->gcx.ssh_command = executableOrEmpty(path);

	internalSaveCommandPath(path, save, "SshCommand");
}

bool MainWindow::checkGitCommand()
{
	while (1) {
		if (misc::isExecutable(gitCommand())) {
			return true;
		}
		if (selectGitCommand(true).isEmpty()) {
			close();
			return false;
		}
	}
}

bool MainWindow::saveBlobAs(RepositoryWrapperFrame *frame, const QString &id, const QString &dstpath)
{
	Git::Object obj = cat_file(frame, id);
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

bool MainWindow::saveByteArrayAs(const QByteArray &ba, const QString &dstpath)
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

QString MainWindow::makeRepositoryName(const QString &loc)
{
	auto i = loc.lastIndexOf('/');
	auto j = loc.lastIndexOf('\\');
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

bool MainWindow::saveFileAs(const QString &srcpath, const QString &dstpath)
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

QString MainWindow::saveAsTemp(RepositoryWrapperFrame *frame, const QString &id)
{
	QString path = newTempFilePath();
	saveAs(frame, id, path);
	return path;
}

QString MainWindow::executableOrEmpty(const QString &path)
{
	if (!path.isEmpty() && checkExecutable(path)) {
		return path;
	}
	return QString();
}

bool MainWindow::checkExecutable(const QString &path)
{
	if (QFileInfo(path).isExecutable()) {
		return true;
	}
	QString text = "The specified program '%1' is not executable.\n";
	text = text.arg(path);
	writeLog(text, false);
	return false;
}

void MainWindow::internalSaveCommandPath(const QString &path, bool save, const QString &name)
{
//	if (checkExecutable(path)) {
		if (save) {
			MySettings s;
			s.beginGroup("Global");
			s.setValue(name, path);
			s.endGroup();
		}
//	}
}

void MainWindow::logGitVersion()
{
	GitPtr g = git();
	QString s = g->version();
	if (!s.isEmpty()) {
		s += '\n';
		writeLog(s, true);
	}
}

void MainWindow::internalClearRepositoryInfo()
{
	setHeadId(QString());
	setCurrentBranch(Git::Branch());
	m->github = GitHubRepositoryInfo();
}

void MainWindow::checkUser()
{
	Git g(m->gcx, {}, {}, {});
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

void MainWindow::openRepository(bool validate, bool waitcursor, bool keep_selection)
{
	if (validate) {
		QString dir = currentWorkingCopyDir();
		if (!QFileInfo(dir).isDir()) {
			int r = QMessageBox::warning(this, tr("Open Repository"), dir + "\n\n" + tr("No such folder") + "\n\n" + tr("Remove from bookmark?"), QMessageBox::Ok, QMessageBox::Cancel);
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
		openRepository(false, false, keep_selection);
		return;
	}

	GitPtr g = git();
	if (!g) {
		qDebug() << "Guitar: git pointer is null";
		return;
	}
	openRepository_(g, keep_selection);
}

void MainWindow::updateRepository()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	OverrideWaitCursor;
	openRepository_(g);
}

void MainWindow::reopenRepository(bool log, const std::function<void (GitPtr )> &callback)
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

void MainWindow::setCurrentRepository(const RepositoryData &repo, bool clear_authentication)
{
	if (clear_authentication) {
		clearAuthentication();
	}
	m->current_repo = repo;
}

void MainWindow::openSelectedRepository()
{
	RepositoryData const *repo = selectedRepositoryItem();
	if (repo) {
		setCurrentRepository(*repo, true);
		openRepository(true);
	}
}

QList<Git::Diff> MainWindow::makeDiffs(RepositoryWrapperFrame *frame, QString id, bool *ok)
{
	QList<Git::Diff> out;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) {
		if (ok) *ok = false;
		return {};
	}

	Git::FileStatusList list = g->status_s();
	setUncommitedChanges(!list.empty());

	if (id.isEmpty() && !isThereUncommitedChanges()) {
		id = getObjCache(frame)->revParse("HEAD").toQString();
	}

	QList<Git::SubmoduleItem> mods;
	updateSubmodules(g, id, &mods);
	setSubmodules(mods);

	bool uncommited = (id.isEmpty() && isThereUncommitedChanges());

	GitDiff dm(getObjCache(frame));
	if (uncommited) {
		dm.diff_uncommited(submodules(), &out);
	} else {
		dm.diff(id, submodules(), &out);
	}

	if (ok) *ok = true;
	return out;
}

void MainWindow::queryBranches(RepositoryWrapperFrame *frame, GitPtr g)
{
	Q_ASSERT(g);
	frame->branch_map.clear();
	QList<Git::Branch> branches = g->branches();
	for (Git::Branch const &b : branches) {
		if (b.isCurrent()) {
			setCurrentBranch(b);
		}
		commitToBranchMapRef(frame)[b.id].append(b);
	}
}

void MainWindow::updateRemoteInfo()
{
	queryRemotes(git());

	m->current_remote_name = QString();
	{
		Git::Branch const &r = currentBranch();
		m->current_remote_name = r.remote;
	}
	if (m->current_remote_name.isEmpty()) {
		if (m->remotes.size() == 1) {
			m->current_remote_name = m->remotes[0];
		}
	}

	emit remoteInfoChanged();
}

void MainWindow::queryRemotes(GitPtr g)
{
	if (!g) return;
	m->remotes = g->getRemotes();
	std::sort(m->remotes.begin(), m->remotes.end());
}

bool MainWindow::cloneRepository(Git::CloneData const &clonedata, RepositoryData const &repodata)
{
	// 既存チェック

	QFileInfo info(repodata.local_dir);
	if (info.isFile()) {
		QString msg = repodata.local_dir + "\n\n" + tr("A file with same name already exists");
		QMessageBox::warning(this, tr("Clone"), msg);
		return false;
	}
	if (info.isDir()) {
		QString msg = repodata.local_dir + "\n\n" + tr("A folder with same name already exists");
		QMessageBox::warning(this, tr("Clone"), msg);
		return false;
	}

	// クローン先ディレクトリの存在チェック

	QString basedir = misc::normalizePathSeparator(clonedata.basedir);
	if (!QFileInfo(basedir).isDir()) {
		int i = basedir.indexOf('/');
		int j = basedir.indexOf('\\');
		if (i < j) i = j;
		if (i < 0) {
			QString msg = basedir + "\n\n" + tr("Invalid folder");
			QMessageBox::warning(this, tr("Clone"), msg);
			return false;
		}

		QString msg = basedir + "\n\n" + tr("No such folder. Create it now?");
		if (QMessageBox::warning(this, tr("Clone"), msg, QMessageBox::Ok, QMessageBox::Cancel) != QMessageBox::Ok) {
			return false;
		}

		// ディレクトリを作成

		QString base = basedir.mid(0, i + 1);
		QString sub = basedir.mid(i + 1);
		QDir(base).mkpath(sub);
	}

	GitPtr g = git({}, {}, repodata.ssh_key);
	setPtyUserData(QVariant::fromValue<RepositoryData>(repodata));
	setPtyCondition(PtyCondition::Clone);
	setPtyProcessOk(true);
	g->clone(clonedata, getPtyProcess());

	return true;
}

void MainWindow::clone(QString url, QString dir)
{
	if (!isOnlineMode()) return;

	if (dir.isEmpty()) {
		dir = defaultWorkingDir();
	}

	while (1) {
		QString ssh_key;
		CloneDialog dlg(this, url, dir, &m->gcx);
		if (dlg.exec() != QDialog::Accepted) {
			return;
		}
		const CloneDialog::Action action = dlg.action();
		url = dlg.url();
		dir = dlg.dir();
		ssh_key = dlg.overridedSshKey();

		RepositoryData reposdata;
		reposdata.local_dir = dir;
		reposdata.local_dir.replace('\\', '/');
		reposdata.name = makeRepositoryName(dir);
		reposdata.ssh_key = ssh_key;

		// クローン先ディレクトリを求める

		Git::CloneData clonedata = Git::preclone(url, dir);

		if (action == CloneDialog::Action::Clone) {
			if (!cloneRepository(clonedata, reposdata)) {
				continue;
			}
		} else if (action == CloneDialog::Action::AddExisting) {
			addExistingLocalRepository(dir, true);
		}

		return; // done
	}
}

void MainWindow::submodule_add(QString url, QString const &local_dir)
{
	if (!isOnlineMode()) return;
	if (local_dir.isEmpty()) return;

	QString dir = local_dir;

	while (1) {
		SubmoduleAddDialog dlg(this, url, dir, &m->gcx);
		if (dlg.exec() != QDialog::Accepted) {
			return;
		}
		url = dlg.url();
		dir = dlg.dir();
		const QString ssh_key = dlg.overridedSshKey();

		RepositoryData repos_item_data;
		repos_item_data.local_dir = dir;
		repos_item_data.local_dir.replace('\\', '/');
		repos_item_data.name = makeRepositoryName(dir);
		repos_item_data.ssh_key = ssh_key;

		Git::CloneData data = Git::preclone(url, dir);
		bool force = dlg.isForce();

		GitPtr g = git(local_dir, {}, repos_item_data.ssh_key);

		auto callback = [&](GitPtr g){
			g->submodule_add(data, force, getPtyProcess());
		};

		{
			OverrideWaitCursor;
			{
				setLogEnabled(g, true);
				AsyncExecGitThread_ th(g, callback);
				th.start();
				while (1) {
					if (th.wait(1)) break;
					QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
				}
				setLogEnabled(g, false);
			}
			openRepository_(g);
		}

		return; // done
	}
}

const Git::CommitItem *MainWindow::selectedCommitItem(RepositoryWrapperFrame *frame) const
{
	int i = selectedLogIndex(frame);
	return commitItem(frame, i);
}

void MainWindow::commit(RepositoryWrapperFrame *frame, bool amend)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QString message;
	QString previousMessage;

	if (amend) {
		message = getCommitLog(frame)[0].message;
	} else {
		QString id = g->getCherryPicking();
		if (Git::isValidID(id)) {
			message = g->getMessage(id);
		} else {
			for (Git::CommitItem const &item : getCommitLog(frame)) {
				if (item.commit_id.isValid()) {
					previousMessage = item.message;
					break;
				}
			}
		}
	}

	while (1) {
		Git::User user = g->getUser(Git::Source::Default);
		QString sign_id = g->signingKey(Git::Source::Default);
		gpg::Data key;
		{
			QList<gpg::Data> keys;
			gpg::listKeys(global->appsettings.gpg_command, &keys);
			for (gpg::Data const &k : keys) {
				if (k.id == sign_id) {
					key = k;
				}
			}
		}
		CommitDialog dlg(this, currentRepositoryName(), user, key, previousMessage);
		dlg.setText(message);
		if (dlg.exec() == QDialog::Accepted) {
			QString text = dlg.text();
			if (text.isEmpty()) {
				QMessageBox::warning(this, tr("Commit"), tr("Commit message can not be omitted."));
				continue;
			}
			bool sign = dlg.isSigningEnabled();
			bool ok;
			if (amend || dlg.isAmend()) {
				ok = g->commit_amend_m(text, sign, getPtyProcess());
			} else {
				ok = g->commit(text, sign, getPtyProcess());
			}
			if (ok) {
				setForceFetch(true);
				updateStatusBarText(frame);
				openRepository(true);
			} else {
				QString err = g->errorMessage().trimmed();
				err += "\n*** ";
				err += tr("Failed to commit");
				err += " ***\n";
				writeLog(err, true);
			}
		}
		break;
	}
}

void MainWindow::commitAmend(RepositoryWrapperFrame *frame)
{
	commit(frame, true);
}

void MainWindow::pushSetUpstream(bool set_upstream, const QString &remote, const QString &branch, bool force)
{
	if (set_upstream) {
		if (remote.isEmpty()) return;
		if (branch.isEmpty()) return;
	}

	int exitcode = 0;
	QString errormsg;

	reopenRepository(true, [&](GitPtr g){
		g->push_u(set_upstream, remote, branch, force, getPtyProcess());
		while (1) {
			if (getPtyProcess()->wait(1)) break;
			QApplication::processEvents();
		}
		exitcode = getPtyProcess()->getExitCode();
		errormsg = getPtyProcess()->getMessage();
	});

	if (exitcode == 128) {
		if (errormsg.indexOf("Connection refused") >= 0) {
			QMessageBox::critical(this, qApp->applicationName(), tr("Connection refused."));
			return;
		}
	}

	updateRemoteInfo();
}

bool MainWindow::pushSetUpstream()
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

	PushDialog dlg(this, remotes, branches, PushDialog::RemoteBranch(QString(), current_branch));
	if (dlg.exec() == QDialog::Accepted) {
		bool set_upstream = dlg.isSetUpStream();
		QString remote = dlg.remote();
		QString branch = dlg.branch();
		bool force = dlg.isForce();
		pushSetUpstream(set_upstream, remote, branch, force);
		return true;
	}

	return false;
}

void MainWindow::deleteBranch(RepositoryWrapperFrame *frame, const Git::CommitItem *commit)
{
	if (!commit) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QStringList all_branch_names;
	QStringList current_local_branch_names;
	{
		NamedCommitList named_commits = namedCommitItems(frame, Branches);
		for (NamedCommitItem const &item : named_commits) {
			if (item.name == "HEAD") continue;
			if (item.id == commit->commit_id.toQString()) {
				current_local_branch_names.push_back(item.name);
			}
			all_branch_names.push_back(item.name);
		}
	}

	DeleteBranchDialog dlg(this, false, all_branch_names, current_local_branch_names);
	if (dlg.exec() == QDialog::Accepted) {
		setLogEnabled(g, true);
		QStringList names = dlg.selectedBranchNames();
		int count = 0;
		for (QString const &name : names) {
			if (g->git(QString("branch -D \"%1\"").arg(name))) {
				count++;
			} else {
				writeLog(tr("Failed to delete the branch '%1'").arg(name) + '\n', true);
			}
		}
		if (count > 0) {
			openRepository(true, true, true);
		}
	}
}

void MainWindow::deleteBranch(RepositoryWrapperFrame *frame)
{
	deleteBranch(frame, selectedCommitItem(frame));
}

void MainWindow::resetFile(const QStringList &paths)
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

void MainWindow::clearAuthentication()
{
	clearSshAuthentication();
	m->http_uid.clear();
	m->http_pwd.clear();
}

void MainWindow::clearSshAuthentication()
{
	m->ssh_passphrase_user.clear();
	m->ssh_passphrase_pass.clear();
}

void MainWindow::internalDeleteTags(const QStringList &tagnames)
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

bool MainWindow::internalAddTag(RepositoryWrapperFrame *frame, const QString &name)
{
	if (name.isEmpty()) return false;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return false;

	QString commit_id;

	Git::CommitItem const *commit = selectedCommitItem(frame);
	if (commit && commit->commit_id.isValid()) {
		commit_id = commit->commit_id.toQString();
	}

	if (!Git::isValidID(commit_id)) return false;

	bool ok = false;
	reopenRepository(false, [&](GitPtr g){
		ok = g->tag(name, commit_id);
	});

	return ok;
}

void MainWindow::createRepository(const QString &dir)
{
	CreateRepositoryDialog dlg(this, dir);
	if (dlg.exec() == QDialog::Accepted) {
		QString path = dlg.path();
		if (QFileInfo(path).isDir()) {
			if (Git::isValidWorkingCopy(path)) {
				// A valid git repository already exists there.
			} else {
				GitPtr g = git(path, {}, {});
				if (g->init()) {
					QString name = dlg.name();
					if (!name.isEmpty()) {
						addExistingLocalRepository(path, name, {}, true);
					}
					QString remote_name = dlg.remoteName();
					QString remote_url = dlg.remoteURL();
					QString ssh_key = dlg.overridedSshKey();
					if (!remote_name.isEmpty() && !remote_url.isEmpty()) {
						Git::Remote r;
						r.name = remote_name;
						r.url = remote_url;
						r.ssh_key = ssh_key;
						g->addRemoteURL(r);
						changeSshKey(path, ssh_key, true);
					}
				}
			}
		} else {
			// not dir
		}
	}
}

void MainWindow::initRepository(QString const &path, QString const &reponame, Git::Remote const &remote)
{
	if (QFileInfo(path).isDir()) {
		if (Git::isValidWorkingCopy(path)) {
			// A valid git repository already exists there.
		} else {
			GitPtr g = git(path, {}, remote.ssh_key);
			if (g->init()) {
				if (!remote.name.isEmpty() && !remote.url.isEmpty()) {
					g->addRemoteURL(remote);
					changeSshKey(path, remote.ssh_key, false);
				}
				addExistingLocalRepository(path, reponame, remote.ssh_key, true);
			}
		}
	}
}

// experimental:
void MainWindow::addRepository(const QString &dir)
{
	AddRepositoryDialog dlg(this, dir);
	if (dlg.exec() == QDialog::Accepted) {
		if (dlg.mode() == AddRepositoryDialog::Clone) {
			auto cdata = dlg.makeCloneData();
			auto rdata = dlg.makeRepositoryData();
			cloneRepository(cdata, rdata);
		} else if (dlg.mode() == AddRepositoryDialog::AddExisting) {
			QString dir = dlg.localPath(false);
			addExistingLocalRepository(dir, true);
		} else if (dlg.mode() == AddRepositoryDialog::Initialize) {
			QString dir = dlg.localPath(false);
			QString name = dlg.repositoryName();
			Git::Remote r;
			r.name = dlg.remoteName();
			r.url = dlg.remoteURL();
			r.ssh_key = dlg.overridedSshKey();
			initRepository(dir, name, r);
		}
	}
}

void MainWindow::setLogEnabled(GitPtr g, bool f)
{
	if (f) {
		g->setLogCallback(git_log_callback, this);
	} else {
		g->setLogCallback(nullptr, nullptr);
	}
}

void MainWindow::doGitCommand(const std::function<void (GitPtr)> &callback)
{
	GitPtr g = git();
	if (isValidWorkingCopy(g)) {
		OverrideWaitCursor;
		callback(g);
		openRepository(false, false);
	}
}

void MainWindow::updateAvatar(const Git::User &user, bool request)
{
	m->current_git_user = user;

	QImage icon;
	if (isAvatarEnabled()) {
		icon = global->avatar_loader.fetch(user.email, request);
	}
	ui->widget_avatar_icon->setImage(icon);
}

void MainWindow::avatarReady()
{
	updateAvatar(currentGitUser(), false);
}

void MainWindow::setWindowTitle_(const Git::User &user)
{
	updateAvatar(user, true);

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

void MainWindow::setUnknownRepositoryInfo()
{
	setRepositoryInfo("---", "");

	Git g(m->gcx, {}, {}, {});
	Git::User user = g.getUser(Git::Source::Global);
	setWindowTitle_(user);
}

void MainWindow::setCurrentRemoteName(const QString &name)
{
	m->current_remote_name = name;
}

void MainWindow::deleteTags(RepositoryWrapperFrame *frame, const Git::CommitItem &commit)
{
	auto it = ptrCommitToTagMap(frame)->find(commit.commit_id.toQString());
	if (it != ptrCommitToTagMap(frame)->end()) {
		QStringList names;
		QList<Git::Tag> const &tags = it->second;
		for (Git::Tag const &tag : tags) {
			names.push_back(tag.name);
		}
		deleteTags(frame, names);
	}
}

bool MainWindow::isAvatarEnabled() const
{
	return appsettings()->get_avatar_icon_from_network_enabled;
}

QStringList MainWindow::remotes() const
{
	return m->remotes;
}

QList<Git::Branch> MainWindow::findBranch(RepositoryWrapperFrame *frame, Git::CommitID const &id)
{
	auto it = commitToBranchMapRef(frame).find(id);
	if (it != commitToBranchMapRef(frame).end()) {
		return it->second;
	}
	return QList<Git::Branch>();
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

/**
 * @brief MainWindow::idFromTag
 * @param frame
 * @param tag
 * @return
 *
 * タグ名からコミットIDを取得する
 */
Git::CommitID MainWindow::idFromTag(RepositoryWrapperFrame *frame, const QString &tag)
{
	return getObjCache(frame)->getCommitIdFromTag(tag);
}

QString MainWindow::newTempFilePath()
{
	QString tmpdir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
	QString path = tmpdir / tempfileHeader() + QString::number(m->temp_file_counter);
	m->temp_file_counter++;
	return path;
}

int MainWindow::limitLogCount() const
{
	const int n = appsettings()->maximum_number_of_commit_item_acquisitions;
	return (n >= 1 && n <= 100000) ? n : 10000;
}

bool MainWindow::isThereUncommitedChanges() const
{
	return m->uncommited_changes;
}

int MainWindow::repositoryIndex_(QTreeWidgetItem const *item) const
{
	if (item) {
		int i = item->data(0, IndexRole).toInt();
		if (i >= 0 && i < cRepositories().size()) {
			return i;
		}
	}
	return -1;
}

RepositoryData const *MainWindow::repositoryItem(QTreeWidgetItem const *item) const
{
	int row = repositoryIndex_(item);
	QList<RepositoryData> const &repos = cRepositories();
	return (row >= 0 && row < repos.size()) ? &repos[row] : nullptr;
}

RepositoryData const *MainWindow::selectedRepositoryItem() const
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

/**
 * @brief リポジトリリストを更新
 */
void MainWindow::updateRepositoriesList()
{
	QString path = getBookmarksFilePath();

	setRepos(RepositoryBookmark::load(path));
	auto const *repos = &cRepositories();

	QString filter = getRepositoryFilterText();

	ui->treeWidget_repos->clear();

	std::map<QString, QTreeWidgetItem *> parentmap;

	for (int i = 0; i < repos->size(); i++) {
		RepositoryData const &repo = repos->at(i);
		if (!filter.isEmpty() && repo.name.indexOf(filter, 0, Qt::CaseInsensitive) < 0) {
			continue;
		}
		QTreeWidgetItem *parent = nullptr;

		QString group = repo.group;
		if (group.startsWith('/')) {
			group = group.mid(1);
		}
		if (group == "") {
			group = "Default";
		}
		auto it = parentmap.find(group);
		if (it != parentmap.end()) {
			parent = it->second;
		} else {
			QStringList list = group.split('/', _SkipEmptyParts);
			if (list.isEmpty()) {
				list.push_back("Default");
			}
			QString groupPath = "", groupPathWithCurrent;
			for (QString const &name : list) {
				if (name.isEmpty()) continue;
				groupPathWithCurrent = groupPath + name;
				auto it = parentmap.find(groupPathWithCurrent);
				if (it != parentmap.end()) {
					parent = it->second;
				} else {
					QTreeWidgetItem *newItem = newQTreeWidgetFolderItem(name);
					if (!parent) {
						ui->treeWidget_repos->addTopLevelItem(newItem);
					} else {
						parent->addChild(newItem);
					}
					parent = newItem;
					parentmap[groupPathWithCurrent] = newItem;
					newItem->setExpanded(true);
				}
				groupPath = groupPathWithCurrent + '/';
			}
			Q_ASSERT(parent);
		}
		parent->setData(0, FilePathRole, "");

		QTreeWidgetItem *child = newQTreeWidgetItem();
		child->setText(0, repo.name);
		child->setData(0, IndexRole, i);
		child->setIcon(0, getRepositoryIcon());
		child->setFlags(child->flags() & ~Qt::ItemIsDropEnabled);
		parent->addChild(child);
		parent->setExpanded(true);
	}
}

/**
 * @brief ファイルリストの表示切り替え
 * @param files_list_type
 */
void MainWindow::showFileList(FilesListType files_list_type)
{
	clearDiffView();

	switch (files_list_type) {
	case FilesListType::SingleList:
		ui->stackedWidget_filelist->setCurrentWidget(ui->page_files); // 1列表示
		break;
	case FilesListType::SideBySide:
		ui->stackedWidget_filelist->setCurrentWidget(ui->page_uncommited); // 2列表示
		break;
	}
}

/**
 * @brief ファイルリストを消去
 * @param frame
 */
void MainWindow::clearFileList(RepositoryWrapperFrame *frame)
{
	showFileList(FilesListType::SingleList);
	frame->unstagedFileslistwidget()->clear();
	frame->stagedFileslistwidget()->clear();
	frame->fileslistwidget()->clear();
}

/**
 * @brief 差分ビューを消去
 * @param frame
 */
void MainWindow::clearDiffView(RepositoryWrapperFrame *frame)
{
	frame->filediffwidget()->clearDiffView();
}

/**
 * @brief 差分ビューを消去
 * @param frame
 */
void MainWindow::clearDiffView()
{
	clearDiffView(ui->frame_repository_wrapper);
}

/**
 * @brief リポジトリ情報を消去
 */
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

/**
 * @brief 指定のコミットにおけるサブモジュールリストを取得
 * @param g
 * @param id
 * @param out
 */
void MainWindow::updateSubmodules(GitPtr g, QString const &id, QList<Git::SubmoduleItem> *out)
{
//	{
//		GitObjectCache objcache;
//		objcache.setup(g);
//		GitCommit tree;
//		GitCommit::parseCommit(&objcache, id, &tree);
//	}
	*out = {};
	QList<Git::SubmoduleItem> submodules;
	if (id.isEmpty()) {
		submodules = g->submodules();
	} else {
		GitTreeItemList list;
		GitObjectCache objcache;
		objcache.setup(g);
		// サブモジュールリストを取得する
		{
			GitCommit tree;
			GitCommit::parseCommit(&objcache, id, &tree);
			parseGitTreeObject(&objcache, tree.tree_id, {}, &list);
			for (GitTreeItem const &item : list) {
				if (item.type == GitTreeItem::Type::BLOB && item.name == ".gitmodules") {
					Git::Object obj = objcache.catFile(item.id);
					if (!obj.content.isEmpty()) {
						parseGitSubModules(obj.content, &submodules);
					}
				}
			}
		}
		// サブモジュールに対応するIDを求める
		for (int i = 0; i < submodules.size(); i++) {
			QStringList vars = submodules[i].path.split('/');
			for (int j = 0; j < vars.size(); j++) {
				for (int k = 0; k < list.size(); k++) {
					if (list[k].name == vars[j]) {
						if (list[k].type == GitTreeItem::Type::BLOB) {
							if (j + 1 == vars.size()) {
								submodules[i].id = list[k].id;
								goto done;
							}
						} else if (list[k].type == GitTreeItem::Type::TREE) {
							Git::Object obj = objcache.catFile(list[k].id);
							parseGitTreeObject(obj.content, {}, &list);
							break;
						}
					}
				}
			}
done:;
		}
	}
	*out = submodules;
}

void MainWindow::saveRepositoryBookmark(RepositoryData item)
{
	if (item.local_dir.isEmpty()) return;

	if (item.name.isEmpty()) {
		item.name = tr("Unnamed");
	}

	auto repos = cRepositories();

	bool done = false;
	for (auto &repo : repos) {
		RepositoryData *p = &repo;
		if (item.local_dir == p->local_dir) {
			*p = item;
			done = true;
			break;
		}
	}
	if (!done) {
		repos.push_back(item);
	}
	setRepos(repos);
	saveRepositoryBookmarks();
	updateRepositoriesList();
}

void MainWindow::changeRepositoryBookmarkName(RepositoryData item, QString new_name)
{
	item.name = new_name;
	saveRepositoryBookmark(item);
}

int MainWindow::rowFromCommitId(RepositoryWrapperFrame *frame, Git::CommitID const &id)
{
	auto const &logs = getCommitLog(frame);
	for (size_t i = 0; i < logs.size(); i++) {
		Git::CommitItem const &item = logs[i];
		if (item.commit_id == id) {
			return (int)i;
		}
	}
	return -1;
}

QList<Git::Tag> MainWindow::findTag(RepositoryWrapperFrame *frame, const QString &id)
{
	auto it = ptrCommitToTagMap(frame)->find(id);
	if (it != ptrCommitToTagMap(frame)->end()) {
		return it->second;
	}
	return QList<Git::Tag>();
}

void MainWindow::sshSetPassphrase(const std::string &user, const std::string &pass)
{
	m->ssh_passphrase_user = user;
	m->ssh_passphrase_pass = pass;
}

std::string MainWindow::sshPassphraseUser() const
{
	return m->ssh_passphrase_user;
}

std::string MainWindow::sshPassphrasePass() const
{
	return m->ssh_passphrase_pass;
}

void MainWindow::httpSetAuthentication(const std::string &user, const std::string &pass)
{
	m->http_uid = user;
	m->http_pwd = pass;
}

std::string MainWindow::httpAuthenticationUser() const
{
	return m->http_uid;
}

std::string MainWindow::httpAuthenticationPass() const
{
	return m->http_pwd;
}

const QList<BranchLabel> *MainWindow::label(const RepositoryWrapperFrame *frame, int row) const
{
	auto it = getLabelMap(frame)->find(row);
	if (it != getLabelMap(frame)->end()) {
		return &it->second;
	}
	return nullptr;
}

ApplicationSettings *MainWindow::appsettings()
{
	return &global->appsettings;
}

const ApplicationSettings *MainWindow::appsettings() const
{
	return &global->appsettings;
}

const Git::CommitItem *MainWindow::getLog(RepositoryWrapperFrame const *frame, int index) const
{
	Git::CommitItemList const &logs = frame->commit_log;
	return (index >= 0 && index < (int)logs.size()) ? &logs[index] : nullptr;
}

/**
 * @brief 樹形図情報を構築する
 * @param frame
 */
void MainWindow::updateCommitGraph(RepositoryWrapperFrame *frame)
{
	auto const &logs = getCommitLog(frame);
	auto *logsp = getCommitLogPtr(frame);

	const int LogCount = (int)logs.size();
	if (LogCount > 0) {
		auto LogItem = [&](int i)->Git::CommitItem &{ return logsp->at((size_t)i); };
		enum { // 有向グラフを構築するあいだ CommitItem::marker_depth をフラグとして使用する
			UNKNOWN = 0,
			KNOWN = 1,
		};
		for (Git::CommitItem &item : *logsp) {
			item.marker_depth = UNKNOWN;
		}
		// コミットハッシュを検索して、親コミットのインデックスを求める
		for (int i = 0; i < LogCount; i++) {
			Git::CommitItem *item = &LogItem(i);
			item->parent_lines.clear();
			if (item->parent_ids.empty()) {
				item->resolved = true;
			} else {
				for (int j = 0; j < item->parent_ids.size(); j++) { // 親の数だけループ
					Git::CommitID const &parent_id = item->parent_ids[j]; // 親のハッシュ値
					for (int k = i + 1; k < (int)LogCount; k++) { // 親を探す
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
				for (int i = 0; i < LogCount; i++) {
					Git::CommitItem *item = &LogItem((int)i);
					if (item->marker_depth == UNKNOWN) {
						int n = (int)item->parent_lines.size(); // 最初のコミットアイテム
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
				while (index > 0 && index < LogCount) { // 最後に到達するまでループ
					e.indexes.push_back(index); // インデックスを追加
					size_t n = LogItem(index).parent_lines.size(); // 親の数
					if (n == 0) break; // 親がないなら終了
					Git::CommitItem *item = &LogItem(index);
					if (item->marker_depth == KNOWN) break; // 既知のアイテムに到達したら終了
					item->marker_depth = KNOWN; // 既知のアイテムにする
					for (int i = 1; i < (int)n; i++) {
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
		for (Git::CommitItem &item : *logsp) {
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
					for (int i = 0; i + 1 < (int)elements.size(); i++) {
						Element *e = &elements[i];
						int index1 = e->indexes.front();
						if (index1 > 0 && !LogItem(index1).has_child) { // 子がない
							// 新しいコミットを探す
							for (int j = i + 1; j < (int)elements.size(); j++) { // 現在位置より後ろを探す
								Element *f = &elements[j];
								int index2 = f->indexes.front();
								if (index1 == index2 + 1) { // 一つだけ新しいコミット
									Element t = std::move(*f);
									elements.erase(elements.begin() + j); // 移動元を削除
									elements.insert(elements.begin() + i, std::move(t)); // 現在位置に挿入
								}
							}
							// 古いコミットを探す
							int j = 0;
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
			for (auto &e : elements) {
				auto ColorNumber = [&](){ return e.depth; };
				size_t count = e.indexes.size();
				for (size_t i = 0; i + 1 < count; i++) {
					int curr = e.indexes[i];
					int next = e.indexes[i + 1];
					TreeLine line(next, e.depth);
					line.color_number = ColorNumber();
					line.bend_early = (i + 2 < count || !LogItem(next).resolved);
					if (i + 2 == count) {
						int join = false;
						if (count > 2) { // 直結ではない
							join = true;
						} else if (!LogItem(curr).has_child) { // 子がない
							join = true;
							int d = LogItem(curr).marker_depth; // 開始点の深さ
							for (int j = curr + 1; j < next; j++) {
								Git::CommitItem *item = &LogItem(j);
								if (item->marker_depth == d) { // 衝突する
									join = false; // 迂回する
									break;
								}
							}
						}
						if (join) {
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

void MainWindow::initNetworking()
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
	global->webcx.set_http_proxy(http_proxy);
	global->webcx.set_https_proxy(https_proxy);
}

bool MainWindow::saveRepositoryBookmarks() const
{
	QString path = getBookmarksFilePath();
	return RepositoryBookmark::save(path, &cRepositories());
}

QString MainWindow::getBookmarksFilePath() const
{
	return global->app_config_dir / "bookmarks.xml";
}

void MainWindow::stopPtyProcess()
{
	getPtyProcess()->stop();
	QDir::setCurrent(m->starting_dir);
}

void MainWindow::abortPtyProcess()
{
	stopPtyProcess();
	setPtyProcessOk(false);
	setInteractionCanceled(true);
}

Git::CommitItemList *MainWindow::getCommitLogPtr(RepositoryWrapperFrame *frame)
{
	return &frame->commit_log;
}

const Git::CommitItemList &MainWindow::getCommitLog(RepositoryWrapperFrame const *frame) const
{
	return frame->commit_log;
}

void MainWindow::setCommitLog(RepositoryWrapperFrame *frame, const Git::CommitItemList &logs)
{
	frame->commit_log = logs;
}

void MainWindow::clearCommitLog(RepositoryWrapperFrame *frame)
{
	frame->commit_log.clear();
}

PtyProcess *MainWindow::getPtyProcess()
{
	return &m->pty_process;
}

bool MainWindow::getPtyProcessOk() const
{
	return m->pty_process_ok;
}

MainWindow::PtyCondition MainWindow::getPtyCondition()
{
	return m->pty_condition;
}

void MainWindow::setPtyUserData(const QVariant &userdata)
{
	m->pty_process.setVariant(userdata);
}

void MainWindow::setPtyProcessOk(bool pty_process_ok)
{
	m->pty_process_ok = pty_process_ok;
}

bool MainWindow::fetch(GitPtr g, bool prune)
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

bool MainWindow::fetch_tags_f(GitPtr g)
{
	setPtyCondition(PtyCondition::Fetch);
	setPtyProcessOk(true);
	g->fetch_tags_f(getPtyProcess());
	while (1) {
		if (getPtyProcess()->wait(1)) break;
		QApplication::processEvents();
	}
	return getPtyProcessOk();
}

void MainWindow::setPtyCondition(const MainWindow::PtyCondition &ptyCondition)
{
	m->pty_condition = ptyCondition;
}

const QList<RepositoryData> &MainWindow::cRepositories() const
{
	return m->repos;
}

QList<RepositoryData> *MainWindow::pRepositories()
{
	return &m->repos;
}

void MainWindow::setRepos(QList<RepositoryData> const &list)
{
	m->repos = list;
}

bool MainWindow::interactionCanceled() const
{
	return m->interaction_canceled;
}

void MainWindow::setInteractionCanceled(bool canceled)
{
	m->interaction_canceled = canceled;
}

MainWindow::InteractionMode MainWindow::interactionMode() const
{
	return m->interaction_mode;
}

void MainWindow::setInteractionMode(const MainWindow::InteractionMode &im)
{
	m->interaction_mode = im;
}

QString MainWindow::getRepositoryFilterText() const
{
	return m->repository_filter_text;
}

void MainWindow::setRepositoryFilterText(const QString &text)
{
	m->repository_filter_text = text;
}

void MainWindow::setUncommitedChanges(bool uncommited_changes)
{
	m->uncommited_changes = uncommited_changes;
}

QList<Git::Diff> *MainWindow::diffResult()
{
	return &m->diff_result;
}

std::map<QString, Git::Diff> *MainWindow::getDiffCacheMap(RepositoryWrapperFrame *frame)
{
	return &frame->diff_cache;
}

GitHubRepositoryInfo *MainWindow::ptrGitHub()
{
	return &m->github;
}

std::map<int, QList<BranchLabel> > *MainWindow::getLabelMap(RepositoryWrapperFrame *frame)
{
	return &frame->label_map;
}

const std::map<int, QList<BranchLabel> > *MainWindow::getLabelMap(const RepositoryWrapperFrame *frame) const
{
	return &frame->label_map;
}

void MainWindow::clearLabelMap(RepositoryWrapperFrame *frame)
{
	frame->label_map.clear();
}

GitObjectCache *MainWindow::getObjCache(RepositoryWrapperFrame *frame)
{
	return &frame->objcache;
}

bool MainWindow::getForceFetch() const
{
	return m->force_fetch;
}

void MainWindow::setForceFetch(bool force_fetch)
{
	m->force_fetch = force_fetch;
}

std::map<Git::CommitID, QList<Git::Tag> > *MainWindow::ptrCommitToTagMap(RepositoryWrapperFrame *frame)
{
	return &frame->tag_map;
}

Git::CommitID MainWindow::getHeadId() const
{
	return m->head_id;
}

void MainWindow::setHeadId(Git::CommitID const &head_id)
{
	m->head_id = head_id;
}

void MainWindow::setPtyProcessCompletionData(const QVariant &value)
{
	m->pty_process_completion_data = value;
}

const QVariant &MainWindow::getTempRepoForCloneCompleteV() const
{
	return m->pty_process_completion_data;
}

void MainWindow::msgNoRepositorySelected()
{
	QMessageBox::warning(this, qApp->applicationName(), tr("No repository selected"));
}

bool MainWindow::isRepositoryOpened() const
{
	return Git::isValidWorkingCopy(currentWorkingCopyDir());
}

QString MainWindow::gitCommand() const
{
	return m->gcx.git_command;
}

QPixmap MainWindow::getTransparentPixmap()
{
	return m->transparent_pixmap;
}

/**
 * @brief リストウィジェット用ファイルアイテムを作成する
 * @param data
 * @return
 */
QListWidgetItem *MainWindow::newListWidgetFileItem(MainWindow::ObjectData const &data)
{
	const bool issubmodule = data.submod; // サブモジュール

	QString header = data.header; // ヘッダ（バッジ識別子）
	if (header.isEmpty()) {
		header = "(??\?) "; // damn trigraph
	}

	QString text = data.path; // テキスト
	if (issubmodule) {
		QString msg = misc::collapseWhitespace(data.submod_commit.message);
		text += QString(" <%0> [%1] %2")
				.arg(data.submod.id.toQString(7))
				.arg(misc::makeDateTimeString(data.submod_commit.commit_date))
				.arg(msg)
				;
	}

	QListWidgetItem *item = new QListWidgetItem(text);
	item->setSizeHint(QSize(item->sizeHint().width(), 18));
	item->setData(FilePathRole, data.path);
	item->setData(DiffIndexRole, data.idiff);
	item->setData(ObjectIdRole, data.id);
	item->setData(HeaderRole, header);
	item->setData(SubmodulePathRole, data.submod.path);
	item->setData(SubmoduleCommitIdRole, data.submod.id.toQString());
	if (issubmodule) {
		item->setToolTip(text); // ツールチップ
	}
	return item;
}

/**
 * @brief 差分リスト情報をもとにリストウィジェットへアイテムを追加する
 * @param diff_list
 * @param fn_add_item
 */
void MainWindow::addDiffItems(const QList<Git::Diff> *diff_list, const std::function<void (ObjectData const &data)> &fn_add_item)
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
		default: header = "() "; break;
		}

		ObjectData data;
		data.id = diff.blob.b_id_or_path;
		data.path = diff.path;
		data.submod = diff.b_submodule.item;
		data.submod_commit = diff.b_submodule.commit;
		data.header = header;
		data.idiff = idiff;
		fn_add_item(data);
	}
}

/**
 * @brief コミットログを取得
 * @param g
 * @return
 */
Git::CommitItemList MainWindow::retrieveCommitLog(GitPtr g)
{
	Git::CommitItemList list = g->log(limitLogCount());

	// 親子関係を調べて、順番が狂っていたら、修正する。

	std::set<QString> set;

	const size_t count = list.size();
	size_t limit = count;

	size_t i = 0;
	while (i < count) {
		size_t newpos = (size_t)-1;
		for (Git::CommitID const &parent : list[i].parent_ids) {
			if (set.find(parent.toQString()) != set.end()) {
				for (size_t j = 0; j < i; j++) {
					if (parent == list[j].commit_id.toQString()) {
						if (newpos == (size_t)-1 || j < newpos) {
							newpos = j;
						}
						qDebug() << "fix commit order" << list[i].commit_id.toQString();
						break;
					}
				}
			}
		}
		set.insert(set.end(), list[i].commit_id.toQString());
		if (newpos != (size_t)-1) {
			if (limit == 0) break; // まず無いと思うが、もし、無限ループに陥ったら
			Git::CommitItem t = list[i];
			t.strange_date = true;
			list.erase(list.begin() + (int)i);
			list.insert(list.begin() + (int)newpos, t);
			i = newpos;
			limit--;
		}
		i++;
	}

	return list;
}

std::map<Git::CommitID, QList<Git::Branch>> &MainWindow::commitToBranchMapRef(RepositoryWrapperFrame *frame)
{
	return frame->branch_map;
}

/**
 * @brief コミットログを更新（100ms遅延）
 */
void MainWindow::updateCommitLogTableLater(RepositoryWrapperFrame *frame, int ms_later)
{
	postUserFunctionEvent([&](QVariant const &, void *ptr){
		if (ptr) {
			RepositoryWrapperFrame *frame = reinterpret_cast<RepositoryWrapperFrame *>(ptr);
			frame->logtablewidget()->viewport()->update();
		}
	}, {}, reinterpret_cast<void *>(frame), ms_later);
}

void MainWindow::updateWindowTitle(GitPtr g)
{
	if (isValidWorkingCopy(g)) {
		Git::User user = g->getUser(Git::Source::Default);
		setWindowTitle_(user);
	} else {
		setUnknownRepositoryInfo();
	}
}

/**
 * @brief コミット情報のテキストを作成
 * @param frame
 * @param row
 * @param label_list
 * @return
 */
QString MainWindow::makeCommitInfoText(RepositoryWrapperFrame *frame, int row, QList<BranchLabel> *label_list)
{
	QString message_ex;
	Git::CommitItem const *commit = &getCommitLog(frame)[row];
	{ // branch
		if (label_list) {
			if (commit->commit_id.toQString() == getHeadId()) {
				BranchLabel label(BranchLabel::Head);
				label.text = "HEAD";
				label_list->push_back(label);
			}
		}
		QList<Git::Branch> list = findBranch(frame, commit->commit_id);
		for (Git::Branch const &b : list) {
			if (b.flags & Git::Branch::HeadDetachedAt) continue;
			if (b.flags & Git::Branch::HeadDetachedFrom) continue;
			BranchLabel label(BranchLabel::LocalBranch);
			label.text = b.name;
			if (!b.remote.isEmpty()) {
				label.kind = BranchLabel::RemoteBranch;
				label.text = "remotes" / b.remote / label.text;
			}
			if (b.ahead > 0) {
				label.info += tr(", %1 ahead").arg(b.ahead);
			}
			if (b.behind > 0) {
				label.info += tr(", %1 behind").arg(b.behind);
			}
			message_ex += " {" + label.text + label.info + '}';
			if (label_list) label_list->push_back(label);
		}
	}
	{ // tag
		QList<Git::Tag> list = findTag(frame, commit->commit_id.toQString());
		for (Git::Tag const &t : list) {
			BranchLabel label(BranchLabel::Tag);
			label.text = t.name;
			message_ex += QString(" {#%1}").arg(label.text);
			if (label_list) label_list->push_back(label);
		}
	}
	return message_ex;
}

/**
 * @brief リポジトリをブックマークから消去
 * @param index 消去するリポジトリのインデックス
 * @param ask trueならユーザーに問い合わせる
 */
void MainWindow::removeRepositoryFromBookmark(int index, bool ask)
{
	if (ask) { // ユーザーに問い合わせ
		int r = QMessageBox::warning(this, tr("Confirm Remove"), tr("Are you sure you want to remove the repository from bookmarks?") + '\n' + tr("(Files will NOT be deleted)"), QMessageBox::Ok, QMessageBox::Cancel);
		if (r != QMessageBox::Ok) return;
	}
	auto *repos = pRepositories();
	if (index >= 0 && index < repos->size()) {
		repos->erase(repos->begin() + index); // 消す
		saveRepositoryBookmarks(); // 保存
		updateRepositoriesList(); // リスト更新
	}
	setRepos(*repos);
}

/**
 * @brief リポジトリをブックマークから消去
 * @param ask trueならユーザーに問い合わせる
 */
void MainWindow::removeSelectedRepositoryFromBookmark(bool ask)
{
	int i = indexOfRepository(ui->treeWidget_repos->currentItem());
	removeRepositoryFromBookmark(i, ask);
}

/**
 * @brief コマンドプロンプトを開く
 * @param repo
 */
void MainWindow::openTerminal(const RepositoryData *repo)
{
	runOnRepositoryDir([](QString dir, QString ssh_key){
#ifdef Q_OS_MAC
		if (!isValidDir(dir)) return;
		QString app = "/Applications/Utilities/Terminal.app";
		if (!QFileInfo(app).exists()) {
			app = "/System/Applications/Utilities/Terminal.app";
			if (!QFileInfo(app).exists()) {
				return;
			}
		}
		QString cmd = "/usr/bin/open -n -a \"%1\" --args \"%2\"";
		cmd = cmd.arg(app).arg(dir);
		system(cmd.toStdString().c_str());
#else
		Terminal::open(dir, ssh_key);
#endif
	}, repo);
}

/**
 * @brief ファイルマネージャを開く
 * @param repo
 */
void MainWindow::openExplorer(const RepositoryData *repo)
{
	runOnRepositoryDir([](QString dir, QString ssh_key){
		(void)ssh_key;
#ifdef Q_OS_MAC
		if (!isValidDir(dir)) return;
		QString cmd = "open \"%1\"";
		cmd = cmd.arg(dir);
		system(cmd.toStdString().c_str());
#else
		QString url = QString::fromLatin1(QUrl::toPercentEncoding(dir));
#ifdef Q_OS_WIN
		QString scheme = "file:///";
#else
		QString scheme = "file://";
#endif
		url = scheme + url.replace("%2F", "/");
		QDesktopServices::openUrl(url);
#endif
	}, repo);
}

/**
 * @brief コマンドを実行していいか、ユーザーに尋ねる
 * @param title ウィンドウタイトル
 * @param command コマンド名
 * @return
 */
bool MainWindow::askAreYouSureYouWantToRun(const QString &title, const QString &command)
{
	QString message = tr("Are you sure you want to run the following command?");
	QString text = "%1\n\n%2";
	text = text.arg(message).arg(command);
	return QMessageBox::warning(this, title, text, QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok;
}

/**
 * @brief テキストファイルを編集する
 * @param path
 * @param title
 * @return
 */
bool MainWindow::editFile(const QString &path, const QString &title)
{
	return TextEditDialog::editFile(this, path, title);
}

void MainWindow::setAppSettings(const ApplicationSettings &appsettings)
{
	global->appsettings = appsettings;
}

QIcon MainWindow::getRepositoryIcon() const
{
	return m->repository_icon;
}

QIcon MainWindow::getFolderIcon() const
{
	return m->folder_icon;
}

QIcon MainWindow::getSignatureGoodIcon() const
{
	return m->signature_good_icon;
}

QIcon MainWindow::getSignatureDubiousIcon() const
{
	return m->signature_dubious_icon;
}

QIcon MainWindow::getSignatureBadIcon() const
{
	return m->signature_bad_icon;
}

QPixmap MainWindow::getTransparentPixmap() const
{
	return m->transparent_pixmap;
}

QStringList MainWindow::findGitObject(const QString &id) const
{
	QStringList list;
	std::set<QString> set;
	if (Git::isValidID(id)) {
		{
			QString a = id.mid(0, 2);
			QString b = id.mid(2);
			QString dir = m->current_repo.local_dir / ".git/objects" / a;
			QDirIterator it(dir);
			while (it.hasNext()) {
				it.next();
				QFileInfo info = it.fileInfo();
				if (info.isFile()) {
					QString c = info.fileName();
					if (c.startsWith(b)) {
						set.insert(set.end(), a + c);
					}
				}
			}
		}
		{
			QString dir = m->current_repo.local_dir / ".git/objects/pack";
			QDirIterator it(dir);
			while (it.hasNext()) {
				it.next();
				QFileInfo info = it.fileInfo();
				if (info.isFile() && info.fileName().startsWith("pack-") && info.fileName().endsWith(".idx")) {
					GitPackIdxV2 idx;
					idx.parse(info.absoluteFilePath(), false);
					idx.each([&](GitPackIdxItem const *item){
						QString item_id = GitPackIdxItem::qid(*item);
						if (item_id.startsWith(id)) {
							set.insert(item_id);
						}
						return true;
					});
				}
			}
		}
		for (QString const &s : set) {
			list.push_back(s);
		}
	}
	return list;
}

void MainWindow::writeLog(const char *ptr, int len, bool record)
{
	internalWriteLog(ptr, len, record);
}

void MainWindow::writeLog(const QString &str, bool record)
{
	std::string s = str.toStdString();
	writeLog(s.c_str(), (int)s.size(), record);
}

QList<BranchLabel> MainWindow::sortedLabels(RepositoryWrapperFrame *frame, int row) const
{
	QList<BranchLabel> list;
	auto const *p = const_cast<MainWindow *>(this)->label(frame, row);
	if (p && !p->empty()) {
		list = *p;
		std::sort(list.begin(), list.end(), [](BranchLabel const &l, BranchLabel const &r){
			auto Compare = [](BranchLabel const &l, BranchLabel const &r){
				if (l.kind < r.kind) return -1;
				if (l.kind > r.kind) return 1;
				if (l.text < r.text) return -1;
				if (l.text > r.text) return 1;
				return 0;
			};
			return Compare(l, r) < 0;
		});
	}
	return list;
}

void MainWindow::saveApplicationSettings()
{
	SettingsDialog::saveSettings(appsettings());
}

void MainWindow::loadApplicationSettings()
{
	SettingsDialog::loadSettings(appsettings());
}

void MainWindow::setDiffResult(const QList<Git::Diff> &diffs)
{
	m->diff_result = diffs;
}

const QList<Git::SubmoduleItem> &MainWindow::submodules() const
{
	return m->submodules;
}

void MainWindow::setSubmodules(const QList<Git::SubmoduleItem> &submodules)
{
	m->submodules = submodules;
}

bool MainWindow::runOnRepositoryDir(const std::function<void (QString, QString)> &callback, const RepositoryData *repo)
{
	if (!repo) {
		repo = &m->current_repo;
	}
	QString dir = repo->local_dir;
	dir.replace('\\', '/');
	if (QFileInfo(dir).isDir()) {
		callback(dir, repo->ssh_key);
		return true;
	}
	msgNoRepositorySelected();
	return false;
}

NamedCommitList MainWindow::namedCommitItems(RepositoryWrapperFrame *frame, int flags)
{
	NamedCommitList items;
	if (flags & Branches) {
		for (auto const &pair : commitToBranchMapRef(frame)) {
			QList<Git::Branch> const &list = pair.second;
			for (Git::Branch const &b : list) {
				if (b.isHeadDetached()) continue;
				if (flags & NamedCommitFlag::Remotes) {
					// nop
				} else {
					if (!b.remote.isEmpty()) continue;
				}
				NamedCommitItem item;
				if (b.remote.isEmpty()) {
					if (b.name == "HEAD") {
						item.type = NamedCommitItem::Type::None;
					} else {
						item.type = NamedCommitItem::Type::BranchLocal;
					}
				} else {
					item.type = NamedCommitItem::Type::BranchRemote;
					item.remote = b.remote;
				}
				item.name = b.name;
				item.id = b.id;
				items.push_back(item);
			}
		}
	}
	if (flags & Tags) {
		for (auto const &pair: *ptrCommitToTagMap(frame)) {
			QList<Git::Tag> const &list = pair.second;
			for (Git::Tag const &t : list) {
				NamedCommitItem item;
				item.type = NamedCommitItem::Type::Tag;
				item.name = t.name;
				item.id = t.id;
				if (item.name.startsWith("refs/tags/")) {
					item.name = item.name.mid(10);
				}
				items.push_back(item);
			}
		}
	}
	return items;
}

QString MainWindow::getObjectID(QListWidgetItem *item)
{
	if (!item) return {};
	return item->data(ObjectIdRole).toString();
}

QString MainWindow::getFilePath(QListWidgetItem *item)
{
	if (!item) return {};
	return item->data(FilePathRole).toString();
}

QString MainWindow::getSubmodulePath(QListWidgetItem *item)
{
	if (!item) return {};
	return item->data(SubmodulePathRole).toString();
}

QString MainWindow::getSubmoduleCommitId(QListWidgetItem *item)
{
	if (!item) return {};
	return item->data(SubmoduleCommitIdRole).toString();
}

bool MainWindow::isGroupItem(QTreeWidgetItem *item)
{
	if (item) {
		int index = item->data(0, IndexRole).toInt();
		if (index == GroupItem) {
			return true;
		}
	}
	return false;
}

int MainWindow::indexOfLog(QListWidgetItem *item)
{
	if (!item) return -1;
	return item->data(IndexRole).toInt();
}

int MainWindow::indexOfDiff(QListWidgetItem *item)
{
	if (!item) return -1;
	return item->data(DiffIndexRole).toInt();
}

/**
 * @brief ファイルリストを更新
 * @param id コミットID
 * @param wait
 */
void MainWindow::updateFilesList(RepositoryWrapperFrame *frame, QString const &id, bool wait)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	if (!wait) return;

	clearFileList(frame);

	Git::FileStatusList stats = g->status_s();
	setUncommitedChanges(!stats.empty());

	FilesListType files_list_type = FilesListType::SingleList;

	bool staged = false;
	auto AddItem = [&](ObjectData const &data){
		QListWidgetItem *item = newListWidgetFileItem(data);
		switch (files_list_type) {
		case FilesListType::SingleList:
			frame->fileslistwidget()->addItem(item);
			break;
		case FilesListType::SideBySide:
			if (staged) {
				frame->stagedFileslistwidget()->addItem(item);
			} else {
				frame->unstagedFileslistwidget()->addItem(item);
			}
			break;
		}
	};

	if (id.isEmpty()) { // Uncommited changed が選択されているとき

		bool uncommited = isThereUncommitedChanges();
		if (uncommited) {
			files_list_type = FilesListType::SideBySide;
		}
		bool ok = false;
		auto diffs = makeDiffs(frame, uncommited ? QString() : id, &ok);
		setDiffResult(diffs);
		if (!ok) return;

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
			Git::Diff const *diff = nullptr;
			if (it != diffmap.end()) {
				idiff = it->second;
				diff = &diffResult()->at(idiff);
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
			ObjectData data;
			data.path = path;
			data.header = header;
			data.idiff = idiff;
			if (diff) {
				data.submod = diff->b_submodule.item; // TODO:
				if (data.submod) {
					GitPtr g = git(data.submod);
					auto sc = g->queryCommit(data.submod.id);
					if (sc) {
						data.submod_commit = *sc;
					}
				}
			}
			AddItem(data);
		}
	} else {
		bool ok = false;
		auto diffs = makeDiffs(frame, id, &ok);
		setDiffResult(diffs);
		if (!ok) return;

		showFileList(files_list_type);
		addDiffItems(diffResult(), AddItem);
	}

	for (Git::Diff const &diff : *diffResult()) {
		QString key = GitDiff::makeKey(diff);
		(*getDiffCacheMap(frame))[key] = diff;
	}
}

/**
 * @brief ファイルリストを更新
 * @param id
 * @param diff_list
 * @param listwidget
 */
void MainWindow::updateFilesList2(RepositoryWrapperFrame *frame, Git::CommitID const &id, QList<Git::Diff> *diff_list, QListWidget *listwidget)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	listwidget->clear();

	auto AddItem = [&](ObjectData const &data){
		QListWidgetItem *item = newListWidgetFileItem(data);
		listwidget->addItem(item);
	};

	GitDiff dm(getObjCache(frame));
	if (!dm.diff(id, submodules(), diff_list)) return;

	addDiffItems(diff_list, AddItem);
}

void MainWindow::execCommitViewWindow(const Git::CommitItem *commit)
{
	CommitViewWindow win(this, commit);
	win.exec();
}

void MainWindow::updateFilesList(RepositoryWrapperFrame *frame, Git::CommitItem const &commit, bool wait)
{
	QString id;
	if (Git::isUncommited(commit)) {
		// empty id for uncommited changes
	} else {
		id = commit.commit_id.toQString();
	}
	updateFilesList(frame, id, wait);
}

void MainWindow::updateCurrentFilesList(RepositoryWrapperFrame *frame)
{
	auto logs = getCommitLog(frame);
	QTableWidgetItem *item = frame->logtablewidget()->item(selectedLogIndex(frame), 0);
	if (!item) return;
	int index = item->data(IndexRole).toInt();
	int count = (int)logs.size();
	if (index < count) {
		updateFilesList(frame, logs[index], true);
	}
}

void MainWindow::detectGitServerType(GitPtr g)
{
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
		int i = (int)push_url.indexOf(s);
		if (i > 0) return i + (int)s.size();
		return 0;
	};

	// check GitHub
	auto pos = Check("@github.com:");
	if (pos == 0) {
		pos = Check("://github.com/");
	}
	if (pos > 0) {
		auto end = push_url.size();
		{
			QString s = ".git";
			if (push_url.endsWith(s)) {
				end -= s.size();
			}
		}
		QString s = push_url.mid(pos, end - pos);
		auto i = s.indexOf('/');
		if (i > 0) {
			auto *p = ptrGitHub();
			QString user = s.mid(0, i);
			QString repo = s.mid(i + 1);
			p->owner_account_name = user;
			p->repository_name = repo;
		}
	}
}

void MainWindow::clearLog(RepositoryWrapperFrame *frame)
{
	clearCommitLog(frame);
	clearLabelMap(frame);
	setUncommitedChanges(false);
	frame->clearLogContents();
}

void MainWindow::openRepository_(GitPtr g, bool keep_selection)
{
	openRepository_(frame(), g, keep_selection);
}

MainWindow::GitFile MainWindow::catFile(Git::CommitID const &id, GitPtr g)
{
	GitFile file;
	if (0) { //@TODO:いつか消す
		auto ba = g->cat_file(id);
		if (ba) {
			file.data = *ba;
		}
	} else {
		Git::Object::Type type;
		GitObjectManager om(g);
		om.catFile(id, &file.data, &file.type);
	}
	return file;
}

Git::Object MainWindow::cat_file_(RepositoryWrapperFrame *frame, GitPtr g, const QString &id) //@TODO:
{
	if (isValidWorkingCopy(g)) {
		QString path_prefix = PATH_PREFIX;
		if (id.startsWith(path_prefix)) {
			QString path = g->workingDir();
			path = path / id.mid(path_prefix.size());
			QFile file(path);
			if (file.open(QFile::ReadOnly)) {
				Git::Object obj;
				obj.content = file.readAll();
				return obj;
			}
		} else if (Git::isValidID(id)) {
			return getObjCache(frame)->catFile(id);;
		}
	}
	return Git::Object();
}

Git::Object MainWindow::cat_file(RepositoryWrapperFrame *frame, const QString &id) //@TODO:
{
	return cat_file_(frame, git(), id);
}

void MainWindow::openRepository_(RepositoryWrapperFrame *frame, GitPtr g, bool keep_selection)
{
	getObjCache(frame)->setup(g);

	int scroll_pos = -1;
	int select_row = -1;

	if (keep_selection) {
		scroll_pos = frame->logtablewidget()->verticalScrollBar()->value();
		select_row = frame->logtablewidget()->currentRow();
	}

	{
		bool do_fetch = isOnlineMode() && (getForceFetch() || appsettings()->automatically_fetch_when_opening_the_repository);
		setForceFetch(false);
		if (do_fetch) {
			if (!fetch(g, false)) {
				return;
			}
		}

		clearLog(frame);
		clearRepositoryInfo();

		detectGitServerType(g);

		updateFilesList(frame, QString(), true);

		bool canceled = false;
		frame->logtablewidget()->setEnabled(false);

		// ログを取得
		setCommitLog(frame, retrieveCommitLog(g));
		// ブランチを取得
		queryBranches(frame, g);
		// タグを取得
		ptrCommitToTagMap(frame)->clear();
		QList<Git::Tag> tags = g->tags();
		for (Git::Tag const &tag : tags) {
			Git::Tag t = tag;
			// t.id = idFromTag(frame, tag.id.toQString()); //@TODO: これいらないのでは？
			(*ptrCommitToTagMap(frame))[t.id].push_back(t);
		}

		frame->logtablewidget()->setEnabled(true);
		updateCommitLogTableLater(frame, 100); // ミコットログを更新（100ms後）
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
	}

	if (!g) return;

	updateRemoteInfo();

	updateWindowTitle(g);

	setHeadId(getObjCache(frame)->revParse("HEAD"));

	if (isThereUncommitedChanges()) {
		Git::CommitItem item;
		item.parent_ids.push_back(currentBranch().id);
		item.message = tr("Uncommited changes");
		auto p = getCommitLogPtr(frame);
		p->insert(p->begin(), item);
	}

	frame->prepareLogTableWidget();

	auto const &logs = getCommitLog(frame);
	const int count = (int)logs.size();

	frame->logtablewidget()->setRowCount(count);

	int selrow = 0;

	for (int row = 0; row < count; row++) {
		Git::CommitItem const *commit = &logs[row];
		{
			auto *item = new QTableWidgetItem;
			item->setData(IndexRole, row);
			frame->logtablewidget()->setItem(row, 0, item);
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
			frame->logtablewidget()->setItem(row, col, item);
			col++;
		};
		QString commit_id;
		QString datetime;
		QString author;
		QString message;
		QString message_ex;
		bool isHEAD = (commit->commit_id.toQString() == getHeadId());
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
			message_ex = makeCommitInfoText(frame, row, &(*getLabelMap(frame))[row]);
		}
		AddColumn(commit_id, false, QString());
		AddColumn(datetime, false, QString());
		AddColumn(author, false, QString());
		AddColumn(message, bold, message + message_ex);
		frame->logtablewidget()->setRowHeight(row, 24);
	}
	int t = frame->logtablewidget()->columnWidth(0);
	frame->logtablewidget()->resizeColumnsToContents();
	frame->logtablewidget()->setColumnWidth(0, t);
	frame->logtablewidget()->horizontalHeader()->setStretchLastSection(false);
	frame->logtablewidget()->horizontalHeader()->setStretchLastSection(true);

	m->last_focused_file_list = nullptr;

	frame->logtablewidget()->setFocus();

	if (select_row < 0) {
		setCurrentLogRow(frame, selrow);
	} else {
		setCurrentLogRow(frame, select_row);
		frame->logtablewidget()->verticalScrollBar()->setValue(scroll_pos >= 0 ? scroll_pos : 0);
	}

	updateUI();
}

/**
 * @brief ネットワークを使用するコマンドの可否をUIに反映する
 * @param enabled
 */
void MainWindow::setNetworkingCommandsEnabled(bool enabled)
{
	ui->action_clone->setEnabled(enabled); // クローンコマンドの有効性を設定

	if (!Git::isValidWorkingCopy(currentWorkingCopyDir())) { // 現在のディレクトリが有効なgitリポジトリなら
		enabled = false; // その他のコマンドは無効
	}

	bool opened = !currentRepository().name.isEmpty(); // 開いてる？
	ui->action_fetch->setEnabled(enabled || opened);
	ui->toolButton_fetch->setEnabled(enabled || opened);

	if (isOnlineMode()) {
		ui->action_fetch->setText(tr("Fetch"));
		ui->toolButton_fetch->setText(tr("Fetch"));
	} else {
		ui->action_fetch->setText(tr("Update"));
		ui->toolButton_fetch->setText(tr("Update"));
	}

	ui->action_fetch_prune->setEnabled(enabled);
	ui->action_pull->setEnabled(enabled);
	ui->action_push->setEnabled(enabled);
	ui->action_push_all_tags->setEnabled(enabled);

	ui->toolButton_pull->setEnabled(enabled);
	ui->toolButton_push->setEnabled(enabled);
}

void MainWindow::updateUI()
{
	GitPtr g = git();
	bool isopened = isValidWorkingCopy(g);

	setNetworkingCommandsEnabled(isOnlineMode());

	Git::Branch b = currentBranch();
	ui->toolButton_push->setNumber(b.ahead > 0 ? b.ahead : -1);
	ui->toolButton_pull->setNumber(b.behind > 0 ? b.behind : -1);

	ui->toolButton_status->setEnabled(isopened);
	ui->toolButton_terminal->setEnabled(isopened);
	ui->toolButton_explorer->setEnabled(isopened);
	ui->action_repository_status->setEnabled(isopened);
	ui->action_terminal->setEnabled(isopened);
	ui->action_explorer->setEnabled(isopened);
	if (m->action_detect_profile) {
		m->action_detect_profile->setEnabled(isopened);
	}
}

void MainWindow::updateStatusBarText(RepositoryWrapperFrame *frame)
{
	QString text;

	QWidget *w = qApp->focusWidget();
	if (w == ui->treeWidget_repos) {
		RepositoryData const *repo = selectedRepositoryItem();
		if (repo) {
			text = QString("%1 : %2")
					.arg(repo->name)
					.arg(misc::normalizePathSeparator(repo->local_dir))
					;
		}
	} else if (w == frame->logtablewidget()) {
		QTableWidgetItem *item = frame->logtablewidget()->item(selectedLogIndex(frame), 0);
		if (item) {
			auto const &logs = getCommitLog(frame);
			int row = item->data(IndexRole).toInt();
			if (row < (int)logs.size()) {
				Git::CommitItem const &commit = logs[row];
				if (Git::isUncommited(commit)) {
					text = tr("Uncommited changes");
				} else {
					QString id = commit.commit_id.toQString();
					text = QString("%1 : %2%3")
							.arg(id.mid(0, 7))
							.arg(commit.message)
							.arg(makeCommitInfoText(frame, row, nullptr))
							;
				}
			}
		}
	}

	setStatusBarText(text);
}

void MainWindow::mergeBranch(QString const &commit, Git::MergeFastForward ff, bool squash)
{
	if (commit.isEmpty()) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	g->mergeBranch(commit, ff, squash);
	openRepository(true);
}

void MainWindow::mergeBranch(Git::CommitItem const *commit, Git::MergeFastForward ff, bool squash)
{
	if (!commit) return;
	mergeBranch(commit->commit_id.toQString(), ff, squash);
}

void MainWindow::rebaseBranch(Git::CommitItem const *commit)
{
	if (!commit) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QString text = tr("Are you sure you want to rebase the commit?");
	text += "\n\n";
	text += "> git rebase " + commit->commit_id.toQString();
	int r = QMessageBox::information(this, tr("Rebase"), text, QMessageBox::Ok, QMessageBox::Cancel);
	if (r == QMessageBox::Ok) {
		g->rebaseBranch(commit->commit_id.toQString());
		openRepository(true);
	}
}

void MainWindow::cherrypick(Git::CommitItem const *commit)
{
	if (!commit) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;



	int n = commit->parent_ids.size();
	if (n == 1) {
		g->cherrypick(commit->commit_id.toQString());
	} else if (n > 1) {
		auto head = g->queryCommit(g->rev_parse("HEAD"));
		auto pick = g->queryCommit(commit->commit_id.toQString());
		QList<Git::CommitItem> parents;
		for (int i = 0; i < n; i++) {
			QString id = commit->commit_id.toQString() + QString("^%1").arg(i + 1);
			Git::CommitID id2 = g->rev_parse(id);
			auto item = g->queryCommit(id2);
			parents.push_back(*item);
		}
		CherryPickDialog dlg(this, *head, *pick, parents);
		if (dlg.exec() == QDialog::Accepted) {
			QString cmd = "-m %1 ";
			cmd = cmd.arg(dlg.number());
			if (dlg.allowEmpty()) {
				cmd += "--allow-empty ";
			}
			cmd += commit->commit_id.toQString();
			g->cherrypick(cmd);
		} else {
			return;
		}
	}

	openRepository(true);
}

void MainWindow::merge(RepositoryWrapperFrame *frame, Git::CommitItem const *commit)
{
	if (isThereUncommitedChanges()) return;

	if (!commit) {
		int row = selectedLogIndex(frame);
		commit = commitItem(frame, row);
		if (!commit) return;
	}

	if (!Git::isValidID(commit->commit_id.toQString())) return;

	static const char MergeFastForward[] = "MergeFastForward";

	QString fastforward;
	{
		MySettings s;
		s.beginGroup("Behavior");
		fastforward = s.value(MergeFastForward).toString();
		s.endGroup();
	}

	std::vector<QString> labels;
	{
		int row = selectedLogIndex(frame);
		QList<BranchLabel> const *v = label(frame, row);
		for (BranchLabel const &label : *v) {
			if (label.kind == BranchLabel::LocalBranch || label.kind == BranchLabel::Tag) {
				labels.push_back(label.text);
			}
		}
		std::sort(labels.begin(), labels.end());
		labels.erase(std::unique(labels.begin(), labels.end()), labels.end());
	}

	labels.push_back(commit->commit_id.toQString());

	QString branch_name = currentBranchName();

	MergeDialog dlg(fastforward, labels, branch_name, this);
	if (dlg.exec() == QDialog::Accepted) {
		fastforward = dlg.getFastForwardPolicy();
		bool squash = dlg.isSquashEnabled();
		{
			MySettings s;
			s.beginGroup("Behavior");
			s.setValue(MergeFastForward, fastforward);
			s.endGroup();
		}
		QString from = dlg.mergeFrom();
		mergeBranch(from, MergeDialog::ff(fastforward), squash);
	}
}

void MainWindow::showStatus()
{
	auto g = git();
	if (!g->isValidWorkingCopy()) {
		msgNoRepositorySelected();
		return;
	}
	QString s = g->status();
	TextEditDialog dlg(this);
	dlg.setWindowTitle(tr("Status"));
	dlg.setText(s, true);
	dlg.exec();
}

void MainWindow::on_action_commit_triggered()
{
	commit(frame());
}

void MainWindow::on_action_fetch_triggered()
{
	if (isOnlineMode()) {
		reopenRepository(true, [&](GitPtr g){
			fetch(g, false);
		});
	} else {
		updateRepository();
	}
}

void MainWindow::on_action_fetch_prune_triggered()
{
	if (!isOnlineMode()) return;

	reopenRepository(true, [&](GitPtr g){
		fetch(g, true);
	});
}

void MainWindow::on_action_push_triggered()
{
	pushSetUpstream();
}

void MainWindow::on_toolButton_push_clicked()
{
	ui->action_push->trigger();
}

void MainWindow::on_action_pull_triggered()
{
	if (!isOnlineMode()) return;

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

void MainWindow::on_toolButton_pull_clicked()
{
	ui->action_pull->trigger();
}

void MainWindow::on_toolButton_status_clicked()
{
	showStatus();
}

void MainWindow::on_action_repository_status_triggered()
{
	showStatus();
}

void MainWindow::on_treeWidget_repos_currentItemChanged(QTreeWidgetItem * /*current*/, QTreeWidgetItem * /*previous*/)
{
	updateStatusBarText(frame());
}

void MainWindow::on_treeWidget_repos_itemDoubleClicked(QTreeWidgetItem * /*item*/, int /*column*/)
{
	openSelectedRepository();
}

void MainWindow::execCommitPropertyDialog(QWidget *parent, Git::CommitItem const *commit)
{
	CommitPropertyDialog dlg(parent, this, commit);
	dlg.exec();
}

void MainWindow::execCommitExploreWindow(RepositoryWrapperFrame *frame, QWidget *parent, const Git::CommitItem *commit)
{
	CommitExploreWindow win(parent, this, getObjCache(frame), commit);
	win.exec();
}

void MainWindow::execFileHistory(const QString &path)
{
	if (path.isEmpty()) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	FileHistoryWindow dlg(this);
	dlg.prepare(g, path);
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

	RepositoryData const *repo = repositoryItem(treeitem);

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
		a_open_terminal->setIcon(QIcon(":/image/terminal.svg"));

		QAction *a_open_folder = menu.addAction(tr("Open &folder"));
		a_open_folder->setIcon(QIcon(":/image/explorer.svg"));

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
				execRepositoryPropertyDialog(*repo);
				return;
			}
		}
	}
}

void MainWindow::on_tableWidget_log_customContextMenuRequested(const QPoint &pos)
{
	int row = selectedLogIndex(frame());
	Git::CommitItem const *commit = commitItem(frame(), row);
	if (commit) {
		bool is_valid_commit_id = Git::isValidID(commit->commit_id.toQString());

		QMenu menu;

		QAction *a_copy_id_7letters = is_valid_commit_id ? menu.addAction(tr("Copy commit id (7 letters)")) : nullptr;
		QAction *a_copy_id_complete = is_valid_commit_id ? menu.addAction(tr("Copy commit id (completely)")) : nullptr;

		std::set<QAction *> copy_label_actions;
		{
			QList<BranchLabel> v = sortedLabels(frame(), row);
			if (!v.isEmpty()) {
				auto *copy_lebel_menu = menu.addMenu("Copy label");
				for (BranchLabel const &l : v) {
					QAction *a = copy_lebel_menu->addAction(l.text);
					copy_label_actions.insert(copy_label_actions.end(), a);
				}
			}
		}

		menu.addSeparator();

		QAction *a_checkout = menu.addAction(tr("Checkout/Branch..."));

		menu.addSeparator();

		QAction *a_edit_message = nullptr;

		auto canEditMessage = [&](){
			if (commit->has_child) return false; // 子がないこと
			if (Git::isUncommited(*commit)) return false; // 未コミットがないこと
			bool is_head = false;
			bool has_remote_branch = false;
			QList<BranchLabel> const *labels = label(frame(), row);
			for (const BranchLabel &label : *labels) {
				if (label.kind == BranchLabel::Head) {
					is_head = true;
				} else if (label.kind == BranchLabel::RemoteBranch) {
					has_remote_branch = true;
				}
			}
			return is_head && !has_remote_branch; // HEAD && リモートブランチ無し
		};
		if (canEditMessage()) {
			a_edit_message = menu.addAction(tr("Edit message..."));
		}

		QAction *a_merge = is_valid_commit_id ? menu.addAction(tr("Merge")) : nullptr;
		QAction *a_rebase = is_valid_commit_id ? menu.addAction(tr("Rebase")) : nullptr;
		QAction *a_cherrypick = is_valid_commit_id ? menu.addAction(tr("Cherry-pick")) : nullptr;
		QAction *a_edit_tags = is_valid_commit_id ? menu.addAction(tr("Edit tags...")) : nullptr;
		QAction *a_revert = is_valid_commit_id ? menu.addAction(tr("Revert")) : nullptr;

		menu.addSeparator();

		QAction *a_delbranch = is_valid_commit_id ? menu.addAction(tr("Delete branch...")) : nullptr;
		QAction *a_delrembranch = remoteBranches(frame(), commit->commit_id.toQString(), nullptr).isEmpty() ? nullptr : menu.addAction(tr("Delete remote branch..."));

		menu.addSeparator();

		QAction *a_explore = is_valid_commit_id ? menu.addAction(tr("Explore")) : nullptr;
		QAction *a_properties = addMenuActionProperty(&menu);

		QAction *a = menu.exec(frame()->logtablewidget()->viewport()->mapToGlobal(pos) + QPoint(8, -8));
		if (a) {
			if (a == a_copy_id_7letters) {
				qApp->clipboard()->setText(commit->commit_id.toQString(7));
				return;
			}
			if (a == a_copy_id_complete) {
				qApp->clipboard()->setText(commit->commit_id.toQString());
				return;
			}
			if (a == a_properties) {
				execCommitPropertyDialog(this, commit);
				return;
			}
			if (a == a_edit_message) {
				commitAmend(frame());
				return;
			}
			if (a == a_checkout) {
				checkout(frame(), this, commit);
				return;
			}
			if (a == a_delbranch) {
				deleteBranch(frame(), commit);
				return;
			}
			if (a == a_delrembranch) {
				deleteRemoteBranch(frame(), commit);
				return;
			}
			if (a == a_merge) {
				merge(frame(), commit);
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
				revertCommit(frame());
				return;
			}
			if (a == a_explore) {
				execCommitExploreWindow(frame(), this, commit);
				return;
			}
			if (copy_label_actions.find(a) != copy_label_actions.end()) {
				QString text = a->text();
				QApplication::clipboard()->setText(text);
				return;
			}
		}
	}
}

void MainWindow::on_listWidget_files_customContextMenuRequested(const QPoint &pos)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QListWidgetItem *item = frame()->fileslistwidget()->currentItem();

	QString submodpath = getSubmodulePath(item);

	QMenu menu;
	QAction *a_delete = menu.addAction(tr("Delete"));
	QAction *a_untrack = menu.addAction(tr("Untrack"));
	QAction *a_history = menu.addAction(tr("History"));
	QAction *a_blame = nullptr;
	QAction *a_clean = nullptr;

	if (submodpath.isEmpty()) { // not submodule
		a_blame = menu.addAction(tr("Blame"));
	} else { // if submodule
		a_clean = menu.addAction(tr("Clean"));
	}

	QAction *a_properties = addMenuActionProperty(&menu);

	QPoint pt = frame()->fileslistwidget()->mapToGlobal(pos) + QPoint(8, -8);
	QAction *a = menu.exec(pt);
	if (a) {
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
		} else if (a == a_clean) {
			cleanSubModule(item);
		} else if (a == a_properties) {
			showObjectProperty(item);
		}
	}
}

void MainWindow::on_listWidget_unstaged_customContextMenuRequested(const QPoint &pos)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QList<QListWidgetItem *> items = frame()->unstagedFileslistwidget()->selectedItems();
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
		QPoint pt = frame()->unstagedFileslistwidget()->mapToGlobal(pos) + QPoint(8, -8);
		QAction *a = menu.exec(pt);
		if (a) {
			QListWidgetItem *item = frame()->unstagedFileslistwidget()->currentItem();
			if (a == a_stage) {
				for_each_selected_files([&](QString const &path){
					g->stage(path);
				});
				updateCurrentFilesList(frame());
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

							int n = text.size();
							if (n > 0 && text[int(n - 1)] != '\n') {
								text += '\n'; // 最後に改行を追加
							}

							text += appending + '\n';

							{
								QFile file(path);
								if (file.open(QFile::WriteOnly)) {
									file.write(text.toUtf8());
								}
							}
							updateCurrentFilesList(frame());
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
					updateCurrentFilesList(frame());
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
				showObjectProperty(item);
				return;
			}
		}
	}
}

void MainWindow::on_listWidget_staged_customContextMenuRequested(const QPoint &pos)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QListWidgetItem *item = frame()->stagedFileslistwidget()->currentItem();
	if (item) {
		QString path = getFilePath(item);
		QString fullpath = currentWorkingCopyDir() / path;
		if (QFileInfo(fullpath).isFile()) {
			QMenu menu;
			QAction *a_unstage = menu.addAction(tr("Unstage"));
			QAction *a_history = menu.addAction(tr("History"));
			QAction *a_blame = menu.addAction(tr("Blame"));
			QAction *a_properties = addMenuActionProperty(&menu);
			QPoint pt = frame()->stagedFileslistwidget()->mapToGlobal(pos) + QPoint(8, -8);
			QAction *a = menu.exec(pt);
			if (a) {
				QListWidgetItem *item = frame()->unstagedFileslistwidget()->currentItem();
				if (a == a_unstage) {
					g->unstage(path);
					openRepository(false);
				} else if (a == a_history) {
					execFileHistory(item);
				} else if (a == a_blame) {
					blame(item);
				} else if (a == a_properties) {
					showObjectProperty(item);
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

void MainWindow::for_each_selected_files(std::function<void(QString const&)> const &fn)
{
	for (QString const &path : selectedFiles()) {
		fn(path);
	}
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

/**
 * @brief オブジェクトプロパティ
 * @param item
 */
void MainWindow::showObjectProperty(QListWidgetItem *item)
{
	if (item) {
		QString submodpath = getSubmodulePath(item);
		if (!submodpath.isEmpty()) {
#if 0
			// サブモジュールウィンドウを表示する
			Git::SubmoduleItem submod;
			submod.path = submodpath;
			submod.id = getObjectID(item);
			if (submod) {
				OverrideWaitCursor;
				GitPtr g = git(submod);
				SubmoduleMainWindow *w = new SubmoduleMainWindow(this, g);
				w->show();
				w->reset();
			}
#else
			QString commit_id = getSubmoduleCommitId(item);
			QString path = currentWorkingCopyDir() / submodpath;
			qDebug() << path << commit_id;
			QProcess::execute(global->this_executive_program, {path, "--commit-id", commit_id});
#endif
		} else {
			// ファイルプロパティダイアログを表示する
			QString path = getFilePath(item);
			QString id = getObjectID(item);
			FilePropertyDialog dlg(this);
			dlg.exec(this, path, id);
		}
	}
}

void MainWindow::cleanSubModule(QListWidgetItem *item)
{
	QString submodpath = getSubmodulePath(item);
	if (submodpath.isEmpty()) return;

	Git::SubmoduleItem submod;
	submod.path = submodpath;
	submod.id = getObjectID(item);

	CleanSubModuleDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		auto opt = dlg.options();
		GitPtr g = git(submod);
		if (opt.reset_hard) {
			g->reset_hard();
		}
		if (opt.clean_df) {
			g->clean_df();
		}
	}
}

bool MainWindow::testRemoteRepositoryValidity(const QString &url, const QString &sshkey)
{
	bool ok;
	{
		OverrideWaitCursor;
		ok = isValidRemoteURL(url, sshkey);
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

QString MainWindow::selectGitCommand(bool save)
{
	char const *exe = GIT_COMMAND;

	QString path = gitCommand();

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

QString MainWindow::selectGpgCommand(bool save)
{
	QString path = global->appsettings.gpg_command;

	auto fn = [&](QString const &path){
		setGpgCommand(path, save);
	};

	QStringList list = whichCommand_(GPG_COMMAND, GPG2_COMMAND);

	QStringList cmdlist;
	cmdlist.push_back(GPG_COMMAND);
	cmdlist.push_back(GPG2_COMMAND);
	return selectCommand_("GPG", cmdlist, list, path, fn);
}

QString MainWindow::selectSshCommand(bool save)
{
	QString path = m->gcx.ssh_command;

	auto fn = [&](QString const &path){
		setSshCommand(path, save);
	};

	QStringList list = whichCommand_(SSH_COMMAND);

	QStringList cmdlist;
	cmdlist.push_back(SSH_COMMAND);
	return selectCommand_("ssh", cmdlist, list, path, fn);
}

const Git::Branch &MainWindow::currentBranch() const
{
	return m->current_branch;
}

void MainWindow::setCurrentBranch(const Git::Branch &b)
{
	m->current_branch = b;
}

const RepositoryData &MainWindow::currentRepository() const
{
	return m->current_repo;
}

QString MainWindow::currentRepositoryName() const
{
	return currentRepository().name;
}

QString MainWindow::currentRemoteName() const
{
	return m->current_remote_name;
}

QString MainWindow::currentBranchName() const
{
	return currentBranch().name;
}

GitPtr MainWindow::git(const QString &dir, const QString &submodpath, const QString &sshkey) const
{
	GitPtr g = std::make_shared<Git>(m->gcx, dir, submodpath, sshkey);
	if (g && QFileInfo(g->gitCommand()).isExecutable()) {
		g->setLogCallback(git_log_callback, (void *)this);
		return g;
	} else {
		QString text = tr("git command not specified") + '\n';
		const_cast<MainWindow *>(this)->writeLog(text, true);
		return GitPtr();
	}
}

GitPtr MainWindow::git()
{
	RepositoryData const &item = currentRepository();
	return git(item.local_dir, {}, item.ssh_key);
}

GitPtr MainWindow::git(const Git::SubmoduleItem &submod)
{
	if (!submod) return {};
	RepositoryData const &item = currentRepository();
	return git(item.local_dir, submod.path, item.ssh_key);
}

Git::User MainWindow::currentGitUser() const
{
	return m->current_git_user;
}

void MainWindow::autoOpenRepository(QString dir, QString const &commit_id)
{
	auto Open = [&](RepositoryData const &item, QString const &commit_id){
		setCurrentRepository(item, true);
		openRepository(false, true);
		if (!commit_id.isEmpty()) {
			if (!locateCommitID(frame(), commit_id)) {
				QMessageBox::information(this, tr("Open Repository"), tr("The specified commit ID was not found."));
			}
		}
	};

	RepositoryData const *repo = findRegisteredRepository(&dir);
	if (repo) {
		Open(*repo, commit_id);
		return;
	}

	RepositoryData newitem;
	GitPtr g = git(dir, {}, {});
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
			newitem.name = QString::fromUtf16((char16_t const *)p, int(right - p));
			saveRepositoryBookmark(newitem);
			Open(newitem, commit_id);
			return;
		}
	} else {
		DoYouWantToInitDialog dlg(this, dir);
		if (dlg.exec() == QDialog::Accepted) {
			createRepository(dir);
		}
	}
}

std::optional<Git::CommitItem> MainWindow::queryCommit(const QString &id)
{
	return git()->queryCommit(id);
}

void MainWindow::checkout(RepositoryWrapperFrame *frame, QWidget *parent, const Git::CommitItem *commit, std::function<void ()> accepted_callback)
{
	if (!commit) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QStringList tags;
	QStringList all_local_branches;
	QStringList local_branches;
	QStringList remote_branches;
	{
		NamedCommitList named_commits = namedCommitItems(frame, Branches | Tags | Remotes);
		for (NamedCommitItem const &item : named_commits) {
			QString name = item.name;
			if (item.id == commit->commit_id.toQString()) {
				if (item.type == NamedCommitItem::Type::Tag) {
					tags.push_back(name);
				} else if (item.type == NamedCommitItem::Type::BranchLocal || item.type == NamedCommitItem::Type::BranchRemote) {
					int i = name.lastIndexOf('/');
					if (i < 0 && name == "HEAD") continue;
					if (i > 0 && name.mid(i + 1) == "HEAD") continue;
					if (item.type == NamedCommitItem::Type::BranchLocal) {
						local_branches.push_back(name);
					} else if (item.type == NamedCommitItem::Type::BranchRemote) {
						remote_branches.push_back(name);
					}
				}
			}
			if (item.type == NamedCommitItem::Type::BranchLocal) {
				all_local_branches.push_back(name);
			}
		}
	}

	CheckoutDialog dlg(parent, tags, all_local_branches, local_branches, remote_branches);
	if (dlg.exec() == QDialog::Accepted) {
		if (accepted_callback) {
			accepted_callback();
		}
		CheckoutDialog::Operation op = dlg.operation();
		QString name = dlg.branchName();
		Git::CommitID id = commit->commit_id.toQString();
		if (!id.isValid() && !commit->parent_ids.isEmpty()) {
			id = commit->parent_ids.front();
		}
		bool ok = false;
		setLogEnabled(g, true);
		if (op == CheckoutDialog::Operation::HeadDetached) {
			if (id.isValid()) {
				ok = g->git(QString("checkout \"%1\"").arg(id.toQString()), true);
			}
		} else if (op == CheckoutDialog::Operation::CreateLocalBranch) {
			if (!name.isEmpty() && id.isValid()) {
				ok = g->git(QString("checkout -b \"%1\" \"%2\"").arg(name).arg(id.toQString()), true);
			}
		} else if (op == CheckoutDialog::Operation::ExistingLocalBranch) {
			if (!name.isEmpty()) {
				ok = g->git(QString("checkout \"%1\"").arg(name), true);
			}
		}
		if (ok) {
			openRepository(true);
		}
	}
}

void MainWindow::checkout(RepositoryWrapperFrame *frame)
{
	checkout(frame, this, selectedCommitItem(frame));
}

void MainWindow::jumpToCommit(RepositoryWrapperFrame *frame, QString id)
{
	GitPtr g = git();
	Git::CommitID id2 = g->rev_parse(id);
	if (id2.isValid()) {
		int row = rowFromCommitId(frame, id2);
		setCurrentLogRow(frame, row);
	}
}

bool MainWindow::addExistingLocalRepository(const QString &dir, bool open)
{
	return addExistingLocalRepository(dir, {}, {}, open);
}

bool MainWindow::saveAs(RepositoryWrapperFrame *frame, const QString &id, const QString &dstpath)
{
	if (id.startsWith(PATH_PREFIX)) {
		return saveFileAs(id.mid(1), dstpath);
	} else {
		return saveBlobAs(frame, id, dstpath);
	}
}

QString MainWindow::determinFileType(QByteArray in)
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

	QString mimetype;
	if (!in.isEmpty()) {
		std::string s = global->filetype.mime_by_data(in.data(), in.size());
		auto i = s.find(';');
		if (i != std::string::npos) {
			s = s.substr(0, i);
		}
		mimetype = QString::fromStdString(s).trimmed();
	}
	return mimetype;
}

QList<Git::Tag> MainWindow::queryTagList(RepositoryWrapperFrame *frame)
{
	QList<Git::Tag> list;
	Git::CommitItem const *commit = selectedCommitItem(frame);
	if (commit && Git::isValidID(commit->commit_id.toQString())) {
		list = findTag(frame, commit->commit_id.toQString());
	}
	return list;
}

TextEditorThemePtr MainWindow::themeForTextEditor()
{
	return global->theme->text_editor_theme;
}

bool MainWindow::isValidWorkingCopy(GitPtr g) const
{
	return g && g->isValidWorkingCopy();
}

void MainWindow::emitWriteLog(const QByteArray &ba, bool receive)
{
	emit signalWriteLog(ba, receive);
}

QString MainWindow::findFileID(RepositoryWrapperFrame *frame, const QString &commit_id, const QString &file)
{
	return lookupFileID(getObjCache(frame), commit_id, file);
}

const Git::CommitItem *MainWindow::commitItem(RepositoryWrapperFrame *frame, int row) const
{
	auto const &logs = getCommitLog(frame);
	if (row >= 0 && row < (int)logs.size()) {
		return &logs[row];
	}
	return nullptr;
}

QImage MainWindow::committerIcon(RepositoryWrapperFrame *frame, int row, QSize size) const
{
	QImage icon;
	if (isAvatarEnabled() && isOnlineMode()) {
		auto const &logs = getCommitLog(frame);
		if (row >= 0 && row < (int)logs.size()) {
			Git::CommitItem const &commit = logs[row];
			if (misc::isValidMailAddress(commit.email)) {
				icon = global->avatar_loader.fetch(commit.email, true); // from gavatar
				if (!size.isValid()) {
					icon = icon.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
				}
			}
		}
	}
	return icon;
}

void MainWindow::changeSshKey(const QString &local_dir, const QString &ssh_key, bool save)
{
	bool changed = false;

	QString locdir = local_dir;
#ifdef Q_OS_WIN
	locdir = locdir.toLower().replace('\\', '/');
#endif

	auto repos = cRepositories();
	for (int i = 0; i < repos.size(); i++) {
		RepositoryData *item = &(repos)[i];
		QString repodir = item->local_dir;
#ifdef Q_OS_WIN
		repodir = repodir.toLower().replace('\\', '/');
#endif
		if (locdir == repodir) {
			item->ssh_key = ssh_key;
			changed = true;
		}
	}
	setRepos(repos);

	if (save && changed) {
		saveRepositoryBookmarks();
	}

	if (m->current_repo.local_dir == local_dir) {
		m->current_repo.ssh_key = ssh_key;
	}
}

QString MainWindow::abbrevCommitID(const Git::CommitItem &commit)
{
	return commit.commit_id.toQString(7);
}

/**
 * @brief コミットログの選択が変化した
 */
void MainWindow::doLogCurrentItemChanged(RepositoryWrapperFrame *frame)
{
	clearFileList(frame);

	int row = selectedLogIndex(frame);
	QTableWidgetItem *item = frame->logtablewidget()->item(row, 0);
	if (item) {
		auto const &logs = getCommitLog(frame);
		int index = item->data(IndexRole).toInt();
		if (index < (int)logs.size()) {
			// ステータスバー更新
			updateStatusBarText(frame);
			// 少し待ってファイルリストを更新する
			postUserFunctionEvent([&](QVariant const &, void *p){
				RepositoryWrapperFrame *frame = reinterpret_cast<RepositoryWrapperFrame *>(p);
				updateCurrentFilesList(frame);
			}, {}, reinterpret_cast<void *>(frame), 300); // 300ms後（キーボードのオートリピート想定）
		}
	} else {
		row = -1;
	}
	updateAncestorCommitMap(frame);
	frame->logtablewidget()->viewport()->update();
}

void MainWindow::findNext(RepositoryWrapperFrame *frame)
{
	if (m->search_text.isEmpty()) {
		return;
	}
	auto const &logs = getCommitLog(frame);
	for (int pass = 0; pass < 2; pass++) {
		int row = 0;
		if (pass == 0) {
			row = selectedLogIndex(frame);
			if (row < 0) {
				row = 0;
			} else if (m->searching) {
				row++;
			}
		}
		while (row < (int)logs.size()) {
			Git::CommitItem const commit = logs[row];
			if (!Git::isUncommited(commit)) {
				if (commit.message.indexOf(m->search_text, 0, Qt::CaseInsensitive) >= 0) {
					bool b = frame->logtablewidget()->blockSignals(true);
					setCurrentLogRow(frame, row);
					frame->logtablewidget()->blockSignals(b);
					m->searching = true;
					return;
				}
			}
			row++;
		}
	}
}

bool MainWindow::locateCommitID(RepositoryWrapperFrame *frame, QString const &commit_id)
{
	auto const &logs = getCommitLog(frame);
	int row = 0;
	while (row < (int)logs.size()) {
		Git::CommitItem const commit = logs[row];
		if (!Git::isUncommited(commit)) {
			if (commit.commit_id.toQString().startsWith(commit_id)) {
				bool b = frame->logtablewidget()->blockSignals(true);
				setCurrentLogRow(frame, row);
				frame->logtablewidget()->blockSignals(b);
				return true;
			}
		}
		row++;
	}
	return false;
}

void MainWindow::findText(QString const &text)
{
	m->search_text = text;
}

bool MainWindow::isAncestorCommit(QString const &id)
{
	auto it = m->ancestors.find(id);
	return it != m->ancestors.end();
}

void MainWindow::updateAncestorCommitMap(RepositoryWrapperFrame *frame)
{
	m->ancestors.clear();

	auto const &logs = getCommitLog(frame);
	const int LogCount = (int)logs.size();
	const int index = selectedLogIndex(frame);
	if (index >= 0 && index < LogCount) {
		// ok
	} else {
		return;
	}

	auto *logsp = getCommitLogPtr(frame);
	auto LogItem = [&](int i)->Git::CommitItem &{ return logsp->at((size_t)i); };

	std::map<QString, size_t> commit_to_index_map;

	int end = LogCount;

	if (index < end) {
		for (int i = index; i < end; i++) {
			Git::CommitItem const &commit = LogItem(i);
			commit_to_index_map[commit.commit_id.toQString()] = (size_t)i;
			auto *item = frame->logtablewidget()->item((int)i, 0);
			QRect r = frame->logtablewidget()->visualItemRect(item);
			if (r.y() >= frame->logtablewidget()->height()) {
				end = i + 1;
				break;
			}
		}
	}

	Git::CommitItem *item = &LogItem(index);
	if (item) {
		m->ancestors.insert(m->ancestors.end(), item->commit_id.toQString());
	}

	for (int i = index; i < end; i++) {
		Git::CommitItem *item = &LogItem(i);
		if (isAncestorCommit(item->commit_id.toQString())) {
			for (Git::CommitID const &parent : item->parent_ids) {
				m->ancestors.insert(m->ancestors.end(), parent.toQString());
			}
		}
	}
}

void MainWindow::refresh()
{
	openRepository(true);
}

void MainWindow::writeLog_(QByteArray ba, bool receive)
{
	if (!ba.isEmpty()) {
		writeLog(ba.data(), ba.size(), receive);
	}
}

void MainWindow::on_action_view_refresh_triggered()
{
	refresh();
}

void MainWindow::on_tableWidget_log_currentItemChanged(QTableWidgetItem * /*current*/, QTableWidgetItem * /*previous*/)
{
	doLogCurrentItemChanged(frame());
	m->searching = false;
}

void MainWindow::on_toolButton_stage_clicked()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	if (m->last_focused_file_list == ui->listWidget_unstaged) {
		QList<QListWidgetItem *> items = ui->listWidget_unstaged->selectedItems();
		if (items.size() == ui->listWidget_unstaged->count()) {
			g->add_A();
		} else {
			QStringList list;
			for (QListWidgetItem *item : items) {
				QString path = getFilePath(item);
				list.push_back(path);
			}
			g->stage(list);
		}
		updateCurrentFilesList(frame());
	}
}

void MainWindow::on_toolButton_unstage_clicked()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	if (m->last_focused_file_list == ui->listWidget_staged) {
		QList<QListWidgetItem *> items = ui->listWidget_staged->selectedItems();
		if (items.size() == ui->listWidget_staged->count()) {
			g->unstage_all();
		} else {
			g->unstage(selectedFiles());
		}
		updateCurrentFilesList(frame());
	}
}

void MainWindow::on_toolButton_select_all_clicked()
{
	if (frame()->unstagedFileslistwidget()->count() > 0) {
		frame()->unstagedFileslistwidget()->setFocus();
		frame()->unstagedFileslistwidget()->selectAll();
	} else if (frame()->stagedFileslistwidget()->count() > 0) {
		frame()->stagedFileslistwidget()->setFocus();
		frame()->stagedFileslistwidget()->selectAll();
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
		updateCurrentFilesList(frame());
	}
}

int MainWindow::selectedLogIndex(RepositoryWrapperFrame *frame) const
{
	auto const &logs = getCommitLog(frame);
	int i = frame->logtablewidget()->currentRow();
	if (i >= 0 && i < (int)logs.size()) {
		return i;
	}
	return -1;
}

/**
 * @brief ファイル差分表示を更新する
 * @param item
 */
void MainWindow::updateDiffView(RepositoryWrapperFrame *frame, QListWidgetItem *item)
{
	clearDiffView(frame);

	m->last_selected_file_item = item;

	if (!item) return;

	int idiff = indexOfDiff(item);
	if (idiff >= 0 && idiff < diffResult()->size()) {
		Git::Diff const &diff = diffResult()->at(idiff);
		QString key = GitDiff::makeKey(diff);
		auto it = getDiffCacheMap(frame)->find(key);
		if (it != getDiffCacheMap(frame)->end()) {
			auto const &logs = getCommitLog(frame);
			int row = frame->logtablewidget()->currentRow();
			bool uncommited = (row >= 0 && row < (int)logs.size() && Git::isUncommited(logs[row]));
			frame->filediffwidget()->updateDiffView(it->second, uncommited);
		}
	}
}

void MainWindow::updateDiffView(RepositoryWrapperFrame *frame)
{
	updateDiffView(frame, m->last_selected_file_item);
}

void MainWindow::updateUnstagedFileCurrentItem(RepositoryWrapperFrame *frame)
{
	updateDiffView(frame, frame->unstagedFileslistwidget()->currentItem());
}

void MainWindow::updateStagedFileCurrentItem(RepositoryWrapperFrame *frame)
{
	updateDiffView(frame, frame->stagedFileslistwidget()->currentItem());
}

void MainWindow::on_listWidget_unstaged_currentRowChanged(int /*currentRow*/)
{
	updateUnstagedFileCurrentItem(frame());
}

void MainWindow::on_listWidget_staged_currentRowChanged(int /*currentRow*/)
{
	updateStagedFileCurrentItem(frame());
}

void MainWindow::on_listWidget_files_currentRowChanged(int /*currentRow*/)
{
	updateDiffView(frame(), frame()->fileslistwidget()->currentItem());
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
	if (QApplication::modalWindow()) return;

	if (event->mimeData()->hasUrls()) {
		event->acceptProposedAction();
		event->accept();
	}
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
	int c = event->key();
	if (c == Qt::Key_T && (event->modifiers() & Qt::ControlModifier)) {
		test();
		return;
	}
	if (QApplication::focusWidget() == ui->widget_log) {

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
		setupExternalPrograms();
		updateAvatar(currentGitUser(), true);
	}
}

void MainWindow::onCloneCompleted(bool success, QVariant const &userdata)
{
	if (success) {
		RepositoryData r = userdata.value<RepositoryData>();
		saveRepositoryBookmark(r);
		setCurrentRepository(r, false);
		openRepository(true);
	}
}

void MainWindow::onPtyProcessCompleted(bool /*ok*/, QVariant const &userdata)
{
	updatePocessLog(false);
	switch (getPtyCondition()) {
	case PtyCondition::Clone:
		onCloneCompleted(getPtyProcessOk(), userdata);
		break;
	}
	setPtyCondition(PtyCondition::None);
}

void MainWindow::on_action_add_repository_triggered()
{
	addRepository(QString());
}

void MainWindow::on_toolButton_addrepo_clicked()
{
	ui->action_add_repository->trigger();
}

//void MainWindow::on_action_clone_triggered()
//{
//	clone();
//}

void MainWindow::on_action_about_triggered()
{
	AboutDialog dlg(this);
	dlg.exec();
}

//void MainWindow::on_toolButton_clone_clicked()
//{
//	ui->action_clone->trigger();
//}





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
	ui->lineEdit_filter->setText(getRepositoryFilterText() + QChar(c));
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

void MainWindow::deleteTags(RepositoryWrapperFrame *frame, QStringList const &tagnames)
{
	int row = frame->logtablewidget()->currentRow();

	internalDeleteTags(tagnames);

	frame->logtablewidget()->selectRow(row);
}

void MainWindow::revertCommit(RepositoryWrapperFrame *frame)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	Git::CommitItem const *commit = selectedCommitItem(frame);
	if (commit) {
		g->revert(commit->commit_id.toQString());
		openRepository(false);
	}
}

bool MainWindow::addTag(RepositoryWrapperFrame *frame, QString const &name)
{
	int row = frame->logtablewidget()->currentRow();

	bool ok = internalAddTag(frame, name);

	frame->selectLogTableRow(row);
	return ok;
}

void MainWindow::on_action_push_all_tags_triggered()
{
	reopenRepository(false, [&](GitPtr g){
		g->push_tags();
	});
}

void MainWindow::on_tableWidget_log_itemDoubleClicked(QTableWidgetItem *)
{
	Git::CommitItem const *commit = selectedCommitItem(frame());
	if (commit) {
		execCommitPropertyDialog(this, commit);
	}
}

void MainWindow::on_listWidget_unstaged_itemDoubleClicked(QListWidgetItem * item)
{
	showObjectProperty(item);
}

void MainWindow::on_listWidget_staged_itemDoubleClicked(QListWidgetItem *item)
{
	showObjectProperty(item);
}

void MainWindow::on_listWidget_files_itemDoubleClicked(QListWidgetItem *item)
{
	showObjectProperty(item);
}

QListWidgetItem *MainWindow::currentFileItem() const
{
	QListWidget *listwidget = nullptr;
	if (ui->stackedWidget_filelist->currentWidget() == ui->page_uncommited) {
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

void MainWindow::on_action_configure_user_triggered()
{
	Git::User global_user;
	Git::User local_user;
	bool enable_local_user = false;

	GitPtr g = git();

	// グローバルユーザーを取得
	global_user = g->getUser(Git::Source::Global);

	// ローカルユーザーを取得
	if (isValidWorkingCopy(g)) {
		local_user = g->getUser(Git::Source::Local);
		enable_local_user = true;
	}

	// ダイアログを開く
	execConfigUserDialog(global_user, local_user, enable_local_user, currentRepositoryName());
}

void MainWindow::showLogWindow(bool show)
{
	ui->dockWidget_log->setVisible(show);
}

bool MainWindow::isValidRemoteURL(const QString &url, const QString &sshkey)
{
	if (url.indexOf('\"') >= 0) {
		return false;
	}
	stopPtyProcess();
	GitPtr g = git({}, {}, sshkey);
	QString cmd = "ls-remote \"%1\" HEAD";
	cmd = cmd.arg(url);
	bool f = g->git(cmd, false, false, getPtyProcess());
	{
		QElapsedTimer time;
		time.start();
		while (!getPtyProcess()->wait(10)) {
			if (time.elapsed() > 10000) {
				f = false;
				break;
			}
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		}
		stopPtyProcess();
	}
	if (f) {
		f = (getPtyProcess()->getExitCode() == 0);
	}
	QString line;
	{
		std::vector<char> v;
		getPtyProcess()->readResult(&v);
		if (!v.empty()) {
			line = QString::fromUtf8(&v[0], (int)v.size()).trimmed();
		}
	}
	if (f) {
		qDebug() << "This is a valid repository.";
		int i = -1;
		for (int j = 0; j < line.size(); j++) {
			ushort c = line.utf16()[j];
			if (QChar(c).isSpace()) {
				i = j;
				break;
			}
		}
		QString head;
		if (i == GIT_ID_LENGTH) {
			QString id = line.mid(0, i);
			QString name = line.mid(i + 1).trimmed();
			qDebug() << id << name;
			if (name == "HEAD" && Git::isValidID(id)) {
				head = id;
			}
		}
		if (head.isEmpty()) {
			qDebug() << "But HEAD not found";
		}
		return true;
	}
	qDebug() << "This is not a repository.";
	return false;
}

QStringList MainWindow::whichCommand_(const QString &cmdfile1, const QString &cmdfile2)
{
	QStringList list;

	if (!cmdfile1.isEmpty()){
		std::vector<std::string> vec;
		FileUtil::qwhich(cmdfile1.toStdString(), &vec);
		for (std::string const &s : vec) {
			list.push_back(QString::fromStdString(s));
		}
	}
	if (!cmdfile2.isEmpty()){
		std::vector<std::string> vec;
		FileUtil::qwhich(cmdfile2.toStdString(), &vec);
		for (std::string const &s : vec) {
			list.push_back(QString::fromStdString(s));
		}
	}

	return list;
}

QString MainWindow::selectCommand_(const QString &cmdname, const QStringList &cmdfiles, const QStringList &list, QString path, const std::function<void (const QString &)> &callback)
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

QString MainWindow::selectCommand_(const QString &cmdname, const QString &cmdfile, const QStringList &list, const QString &path, const std::function<void (const QString &)> &callback)
{
	QStringList cmdfiles;
	cmdfiles.push_back(cmdfile);
	return selectCommand_(cmdname, cmdfiles, list, path, callback);
}

void MainWindow::on_action_window_log_triggered(bool checked)
{
	showLogWindow(checked);
}

void MainWindow::on_action_repo_jump_triggered()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	NamedCommitList items = namedCommitItems(frame(), Branches | Tags | Remotes);
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
		Git::CommitID id = g->rev_parse(text);
		if (!id.isValid() && Git::isValidID(text)) {
			QStringList list = findGitObject(text);
			if (list.isEmpty()) {
				QMessageBox::warning(this, tr("Jump"), QString("%1\n\n").arg(text) + tr("No such commit"));
				return;
			}
			ObjectBrowserDialog dlg2(this, list);
			if (dlg2.exec() == QDialog::Accepted) {
				id = dlg2.text();
				if (!id.isValid()) return;
			}
		}
		if (g->objectType(id) == "tag") {
			id = getObjCache(frame())->getCommitIdFromTag(id.toQString());
		}
		int row = rowFromCommitId(frame(), id);
		if (row < 0) {
			QMessageBox::warning(this, tr("Jump"), QString("%1\n(%2)\n\n").arg(text).arg(id.toQString()) + tr("No such commit"));
		} else {
			setCurrentLogRow(frame(), row);
		}
	}
}

void MainWindow::on_action_repo_checkout_triggered()
{
	checkout(frame());
}

void MainWindow::on_action_delete_branch_triggered()
{
	deleteBranch(frame());
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



bool MainWindow::isOnlineMode() const
{
	return m->is_online_mode;
}

void MainWindow::setRemoteOnline(bool f, bool save)
{
	m->is_online_mode = f;

	{
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
	}

	if (save) {
		MySettings s;
		s.beginGroup("Remote");
		s.setValue("Online", f);
		s.endGroup();
	}
}

void MainWindow::on_radioButton_remote_online_clicked()
{
	setRemoteOnline(true, true);
}

void MainWindow::on_radioButton_remote_offline_clicked()
{
	setRemoteOnline(false, true);
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
	execRepositoryPropertyDialog(currentRepository());
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

void MainWindow::deleteRemoteBranch(RepositoryWrapperFrame *frame, const Git::CommitItem *commit)
{
	if (!commit) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QStringList all_branches;
	QStringList remote_branches = remoteBranches(frame, commit->commit_id.toQString(), &all_branches);
	if (remote_branches.isEmpty()) return;

	DeleteBranchDialog dlg(this, true, all_branches, remote_branches);
	if (dlg.exec() == QDialog::Accepted) {
		setLogEnabled(g, true);
		QStringList names = dlg.selectedBranchNames();
		for (QString const &name : names) {
			int i = name.indexOf('/');
			if (i > 0) {
				QString remote = name.mid(0, i);
				QString branch = ':' + name.mid(i + 1);
				pushSetUpstream(true, remote, branch, false);
			}
		}
	}
}

QStringList MainWindow::remoteBranches(RepositoryWrapperFrame *frame, const QString &id, QStringList *all)
{
	if (all) all->clear();

	QStringList list;

	GitPtr g = git();
	if (isValidWorkingCopy(g)) {
		NamedCommitList named_commits = namedCommitItems(frame, Branches | Remotes);
		for (NamedCommitItem const &item : named_commits) {
			if (item.id == id && !item.remote.isEmpty()) {
				list.push_back(item.remote / item.name);
			}
			if (all && !item.remote.isEmpty() && item.name != "HEAD") {
				all->push_back(item.remote / item.name);
			}
		}
	}

	return list;
}

void MainWindow::onLogIdle()
{
	if (interactionCanceled()) return;
	if (interactionMode() != InteractionMode::None) return;

	static char const are_you_sure_you_want_to_continue_connecting[] = "Are you sure you want to continue connecting (yes/no)?";
	static char const enter_passphrase[] = "Enter passphrase: ";
	static char const enter_passphrase_for_key[] = "Enter passphrase for key '";
	static char const fatal_authentication_failed_for[] = "fatal: Authentication failed for '";
	static char const remote_host_identification_has_changed[] = "WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!";

	std::vector<std::string> lines = getLogHistoryLines();
	clearLogHistory();

	if (lines.empty()) return;

	auto Contains = [&](char const *kw){
		for (std::string const &line : lines) {
			if (strstr(line.c_str(), kw)) {
				return true;
			}
		}
		return false;
	};

	if (Contains(remote_host_identification_has_changed)) {
		QString text;
		{
			for (std::string const &line : lines) {
				QString qline = QString::fromStdString(line);
				text += qline + '\n';
			}
		}
		TextEditDialog dlg(this);
		dlg.setWindowTitle(remote_host_identification_has_changed);
		dlg.setText(text, true);
		dlg.exec();
		return;
	}

	{
		std::string line = lines.back();
		if (!line.empty()) {
			auto ExecLineEditDialog = [&](QWidget *parent, QString const &title, QString const &prompt, QString const &val, bool password){
				LineEditDialog dlg(parent, title, prompt, val, password);
				if (dlg.exec() == QDialog::Accepted) {
					std::string ret = dlg.text().toStdString();
					std::string str = ret + '\n';
					getPtyProcess()->writeInput(str.c_str(), (int)str.size());
					return ret;
				}
				abortPtyProcess();
				return std::string();
			};

			auto Match = [&](char const *str){
				size_t n = strlen(str);
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
					size_t i = strlen(enter_passphrase_for_key);
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
						msg = QString::fromUtf8(begin, int(end - begin - 1));
					} else if (memcmp(end - 3, "': ", 3) == 0) {
						msg = QString::fromUtf8(begin, int(end - begin - 2));
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
	Git::CommitItem const *commit = selectedCommitItem(frame());
	if (commit && Git::isValidID(commit->commit_id.toQString())) {
		EditTagsDialog dlg(this, commit);
		dlg.exec();
	}
}

void MainWindow::on_action_delete_remote_branch_triggered()
{
	deleteRemoteBranch(frame(), selectedCommitItem(frame()));
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
	postUserFunctionEvent([&](QVariant const &v, void *){
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

void MainWindow::on_action_repositories_panel_triggered()
{
	bool checked = ui->action_repositories_panel->isChecked();
	ui->stackedWidget_leftpanel->setCurrentWidget(checked ? ui->page_repos : ui->page_collapsed);

	if (checked) {
		ui->stackedWidget_leftpanel->setFixedWidth(m->repos_panel_width);
		ui->stackedWidget_leftpanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
		ui->stackedWidget_leftpanel->setMinimumWidth(QWIDGETSIZE_MAX);
		ui->stackedWidget_leftpanel->setMaximumWidth(QWIDGETSIZE_MAX);
	} else {
		m->repos_panel_width = ui->stackedWidget_leftpanel->width();
		ui->stackedWidget_leftpanel->setFixedWidth(24);
	}
}

void MainWindow::on_action_find_triggered()
{
	m->searching = false;

	if (getCommitLog(frame()).empty()) {
		return;
	}

	FindCommitDialog dlg(this, m->search_text);
	if (dlg.exec() == QDialog::Accepted) {
		m->search_text = dlg.text();
		frame()->setFocusToLogTable();
		findNext(frame());
	}
}

void MainWindow::on_action_find_next_triggered()
{
	if (m->search_text.isEmpty()) {
		on_action_find_triggered();
	} else {
		findNext(frame());
	}
}

void MainWindow::on_action_repo_jump_to_head_triggered()
{
	QString name = "HEAD";
	GitPtr g = git();
	auto id = g->rev_parse(name);
	int row = rowFromCommitId(frame(), id);
	if (row < 0) {
		qDebug() << "No such commit";
	} else {
		setCurrentLogRow(frame(), row);
	}

}

void MainWindow::on_action_repo_merge_triggered()
{
	merge(frame());
}

void MainWindow::on_action_expand_commit_log_triggered()
{
	ui->splitter_h->setSizes({10000, 1, 1});
}

void MainWindow::on_action_expand_file_list_triggered()
{
	ui->splitter_h->setSizes({1, 10000, 1});
}

void MainWindow::on_action_expand_diff_view_triggered()
{
	ui->splitter_h->setSizes({1, 1, 10000});
}

void MainWindow::on_action_sidebar_triggered()
{
	bool f = ui->stackedWidget_leftpanel->isVisible();
	f = !f;
	ui->stackedWidget_leftpanel->setVisible(f);
	ui->action_sidebar->setChecked(f);
}

#if 0
void MainWindow::on_action_wide_triggered()
{
	QWidget *w = focusWidget();

	if (w == m->focused_widget) {
		ui->splitter_h->setSizes(m->splitter_h_sizes);
		m->focused_widget = nullptr;
	} else {
		m->focused_widget = w;
		m->splitter_h_sizes = ui->splitter_h->sizes();

		if (w == frame->logtablewidget()) {
			ui->splitter_h->setSizes({10000, 1, 1});
		} else if (ui->stackedWidget_filelist->isAncestorOf(w)) {
			ui->splitter_h->setSizes({1, 10000, 1});
		} else if (ui->frame_diff_view->isAncestorOf(w)) {
			ui->splitter_h->setSizes({1, 1, 10000});
		}
	}
}
#endif

void MainWindow::setShowLabels(bool show, bool save)
{
	ApplicationSettings *as = appsettings();
	as->show_labels = show;

	bool b = ui->action_show_labels->blockSignals(true);
	ui->action_show_labels->setChecked(show);
	ui->action_show_labels->blockSignals(b);

	if (save) {
		saveApplicationSettings();
	}
}

void MainWindow::setShowGraph(bool show, bool save)
{
	ApplicationSettings *as = appsettings();
	as->show_graph = show;

	bool b = ui->action_show_graph->blockSignals(true);
	ui->action_show_graph->setChecked(show);
	ui->action_show_graph->blockSignals(b);

	if (save) {
		saveApplicationSettings();
	}
}

bool MainWindow::isLabelsVisible() const
{
	return appsettings()->show_labels;
}

bool MainWindow::isGraphVisible() const
{
	return appsettings()->show_graph;
}

void MainWindow::on_action_show_labels_triggered()
{
	bool f = ui->action_show_labels->isChecked();
	setShowLabels(f, true);
	frame()->updateLogTableView();
}

void MainWindow::on_action_show_graph_triggered()
{
	bool f = ui->action_show_graph->isChecked();
	setShowGraph(f, true);
	frame()->updateLogTableView();
}

void MainWindow::on_action_submodules_triggered()
{
	GitPtr g = git();
	QList<Git::SubmoduleItem> mods = g->submodules();

	std::vector<SubmodulesDialog::Submodule> mods2;
	mods2.resize((size_t)mods.size());

	for (size_t i = 0; i < (size_t)mods.size(); i++) {
		const Git::SubmoduleItem mod = mods[(int)i];
		mods2[i].submodule = mod;

		GitPtr g2 = git(g->workingDir(), mod.path, g->sshKey());
		auto commit = g2->queryCommit(mod.id);
		if (commit) {
			mods2[i].head = *commit;
		}
	}

	SubmodulesDialog dlg(this, mods2);
	dlg.exec();
}

void MainWindow::on_action_submodule_add_triggered()
{
	QString dir = currentRepository().local_dir;
	submodule_add({}, dir);
}

void MainWindow::on_action_submodule_update_triggered()
{
	SubmoduleUpdateDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		GitPtr g = git();
		Git::SubmoduleUpdateData data;
		data.init = dlg.isInit();
		data.recursive = dlg.isRecursive();
		g->submodule_update(data, getPtyProcess());
		refresh();
	}
}

/**
 * @brief アイコンの読み込みが完了した
 */
void MainWindow::onAvatarUpdated(RepositoryWrapperFrameP frame)
{
	updateCommitLogTableLater(frame.pointer, 100); // コミットログを更新（100ms後）
}

void MainWindow::on_action_create_desktop_launcher_file_triggered()
{
#ifdef Q_OS_UNIX
	QString exec = QApplication::applicationFilePath();

	QString home = QDir::home().absolutePath();
	QString icon_dir = home / ".local/share/icons/jp.soramimi/";
	QString launcher_dir = home / ".local/share/applications/";
	QString name = "jp.soramimi.Guitar";
	QString iconfile = icon_dir / name + ".svg";
	QString launcher_path = launcher_dir / name + ".desktop";
	launcher_path = QFileDialog::getSaveFileName(this, tr("Save Launcher File"), launcher_path, "Launcher files (*.desktop)");

	bool ok = false;

	if (!launcher_path.isEmpty()) {
		QFile out(launcher_path);
		if (out.open(QFile::WriteOnly)) {
QString data = R"---([Desktop Entry]
Type=Application
Name=Guitar
Categories=Development
Exec=%1
Icon=%2
Terminal=false
)---";
			data = data.arg(exec).arg(iconfile);
			std::string s = data.toStdString();
			out.write(s.c_str(), s.size());
			out.close();
			std::string path = launcher_path.toStdString();
			chmod(path.c_str(), 0755);
			ok = true;
		}
	}

	if (ok) {
		QFile src(":/image/Guitar.svg");
		if (src.open(QFile::ReadOnly)) {
			QByteArray ba = src.readAll();
			src.close();
			QDir().mkpath(icon_dir);
			QFile dst(iconfile);
			if (dst.open(QFile::WriteOnly)) {
				dst.write(ba);
				dst.close();
			}
		}
	}

#endif

#ifdef Q_OS_WIN
	QString desktop_dir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
	QString target_path = QApplication::applicationFilePath();

	Win32ShortcutData data;
	data.targetpath = (wchar_t const *)target_path.utf16();

	QString home = QDir::home().absolutePath();
	QString launcher_dir = desktop_dir;
	QString name = APPLICATION_NAME;
	QString iconpath = target_path;//icon_dir / name + ".ico";
	QString launcher_path = launcher_dir / name + ".lnk";
	QString lnkpath = QFileDialog::getSaveFileName(this, tr("Save Launcher File"), launcher_path, "Launcher files (*.lnk)");
	data.iconpath = (wchar_t const *)iconpath.utf16();
	data.lnkpath = (wchar_t const *)lnkpath.utf16();

//	QFile::copy(":/Guitar.ico", iconpath);

	if (!launcher_path.isEmpty()) {
		createWin32Shortcut(data);
	}


#endif
}

void MainWindow::test()
{
	QElapsedTimer t;
	t.start();
	std::vector<Git::CommitID> ids;
	ids.emplace_back("48b37ac7b4119ae01180db65477613297971889c");
	std::map<Git::CommitID, Git::CommitItem> map;
	int total = 0;
	while (total < 10000 && !ids.empty()) {
		Git::CommitID id = ids.back();
		ids.pop_back();
		if (map.find(id) != map.end()) continue;
		GitFile file = catFile(id, git());
		Git::CommitItem commit = Git::parseCommit(file.data);
		ids.insert(ids.end(), commit.parent_ids.begin(), commit.parent_ids.end());
		map[id] = commit;
		total++;
	}
	qDebug() << total << t.elapsed();
}


