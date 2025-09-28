#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "AboutDialog.h"
#include "AddRepositoriesCollectivelyDialog.h"
#include "AddRepositoryDialog.h"
#include "ApplicationGlobal.h"
#include "AreYouSureYouWantToContinueConnectingDialog.h"
#include "BlameWindow.h"
#include "CheckoutDialog.h"
#include "CherryPickDialog.h"
#include "CleanSubModuleDialog.h"
#include "CommitDetailGetter.h"
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
#include "GitConfigGlobalAddSafeDirectoryDialog.h"
#include "GitDiffManager.h"
#include "GitHubAPI.h"
#include "GitProcessThread.h"
#include "JumpDialog.h"
#include "LineEditDialog.h"
#include "MergeDialog.h"
#include "MySettings.h"
#include "OverrideWaitCursor.h"
#include "Profile.h"
#include "ProgressWidget.h"
#include "PushDialog.h"
#include "ReflogWindow.h"
#include "RepositoryModel.h"
#include "RepositoryPropertyDialog.h"
#include "SelectCommandDialog.h"
#include "SetGlobalUserDialog.h"
#include "SetGpgSigningDialog.h"
#include "SettingsDialog.h"
#include "StatusLabel.h"
#include "SubmoduleAddDialog.h"
#include "SubmoduleUpdateDialog.h"
#include "SubmodulesDialog.h"
#include "Terminal.h"
#include "TextEditDialog.h"
#include "UserEvent.h"
#include "Util.h"
#include "WelcomeWizardDialog.h"
#include "common/joinpath.h"
#include "common/misc.h"
#include "platform.h"
#include <QBuffer>
#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QProcess>
#include <QShortcut>
#include <QStandardPaths>
#include <QTimer>
#include <cctype>
#include <fcntl.h>
#include <sys/stat.h>
#include <variant>

#ifdef UNSAFE_ENABLED
#include "sshsupport/ConfirmRemoteSessionDialog.h"
#include "sshsupport/GitRemoteSshSession.h"
#include "sshsupport/SshDialog.h"
#endif

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

constexpr int ASCII_BACKSPACE = 0x08;

struct MainWindow::Private {

	QString starting_dir;

	RepositoryInfo current_repository;

	std::mutex repository_mutex;
	RepositoryData current_repository_data{&repository_mutex};
	void clearCurrentRepositoryData()
	{
		current_repository_data = {&repository_mutex};
	}

	// GitCommandCache git_command_cache;
	GitRunner unassosiated_git_runner;
	// GitRunner current_git_runner;

	GitUser current_git_user;

	QList<RepositoryInfo> repos;
	QList<GitDiff> diff_result;
	QList<GitSubmoduleItem> submodules;

	QStringList added;
	QStringList remotes;
	QString current_remote_name;
	GitBranch current_branch;
	unsigned int temp_file_counter = 0;

	std::string ssh_passphrase_user;
	std::string ssh_passphrase_pass;

	std::string http_uid;
	std::string http_pwd;

	std::map<QString, GitHubAPI::User> committer_map; // key is email

	PtyProcess pty_process;
	bool pty_process_ok = false;

	bool interaction_enabled = false;
	MainWindow::InteractionMode interaction_mode = MainWindow::InteractionMode::None;

	MainWindow::FilterTarget filter_target = MainWindow::FilterTarget::RepositorySearch;
	QString incremental_search_text;
	int before_search_row = -1;
	bool uncommited_changes = false;
	std::vector<GitFileStatus> uncommited_changes_file_list;

	// QString commit_log_filter_text;

	GitHash head_id;

	RepositoryInfo temp_repo_for_clone_complete;
	QVariant pty_process_completion_data;

	std::vector<EventItem> event_item_list;

	bool is_online_mode = true;
	QTimer interval_10ms_timer;
	QImage graph_color;
	StatusLabel *status_bar_label;

	QObject *last_focused_file_list = nullptr;

	QListWidgetItem *last_selected_file_item = nullptr;

	bool searching = false;
	QString search_text;

	int repos_panel_width = 0;

	std::set<GitHash> ancestors;

	QWidget *focused_widget = nullptr;
	QList<int> splitter_h_sizes;

	std::vector<char> log_history_bytes;

	QAction *action_edit_profile = nullptr;
	QAction *action_detect_profile = nullptr;

	int current_account_profiles = -1;

	CommitDetailGetter commit_detail_getter;

	QString add_repository_into_group;

	GitProcessThread git_process_thread;

	std::function<void (QVariant const &var)> retry_function;
	QVariant retry_variant;

	const GitCommitItem null_commit_item;
	const TagList null_tag_list;

	QTimer update_commit_log_timer;
	QTimer update_file_list_timer;

	bool background_process_work_in_progress = false;
};

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, m(new Private)
{
	ui->setupUi(this);

	global->mainwindow = this;
	connect(this, &MainWindow::sigWriteLog, this, &MainWindow::internalWriteLog);
	global->gitopt.log = true;

	setupShowFileListHandler();
	setupStatusInfoHandler();
	setupAddFileObjectData();

	ui->tableWidget_log->setup(this);

	setupExternalPrograms();

	setUnknownRepositoryInfo();

	m->starting_dir = QDir::current().absolutePath();

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
	m->status_bar_label->setAlignment((Qt::Alignment)Qt::TextSingleLine);
	ui->statusBar->addWidget(m->status_bar_label);

	progress_widget()->setVisible(false);

	ui->widget_diff_view->init();

	qApp->installEventFilter(this);

	setShowLabels(appsettings()->show_labels, false);
	setShowGraph(appsettings()->show_graph, false);
	setShowAvatars(appsettings()->show_avatars, false);

	{
		QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		ui->widget_log->view()->setTextFont(font);
		ui->widget_log->view()->setSomethingBadFlag(true); // TODO:
	}
	ui->widget_log->view()->setupForLogWidget(themeForTextEditor());
	onLogVisibilityChanged();

	platform::initNetworking();

	m->graph_color = global->theme->graphColorMap();

	ui->widget_log->view()->setTextFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

	connect(ui->dockWidget_log, &QDockWidget::visibilityChanged, this, &MainWindow::onLogVisibilityChanged);
	connect(ui->widget_log->view(), &TextEditorView::idle, this, &MainWindow::onLogIdle);

	connect(ui->treeWidget_repos, &RepositoryTreeWidget::dropped, this, &MainWindow::onRepositoriesTreeDropped);

	connectPtyProcessCompleted();

	// 右上のアイコンがクリックされたとき、ConfigUserダイアログを表示
	connect(ui->widget_avatar_icon, &SimpleImageWidget::clicked, this, &MainWindow::on_action_configure_user_triggered);

	connect(new QShortcut(QKeySequence("Ctrl+T"), this), &QShortcut::activated, this, &MainWindow::test);

	connect(&m->commit_detail_getter, &CommitDetailGetter::ready, this, &MainWindow::onCommitDetailGetterReady);

	connect(&m->update_commit_log_timer, &QTimer::timeout, [&](){
		_updateCommitLogTableView(0);
	});

	connect(this, &MainWindow::remoteInfoChanged, this, &MainWindow::onRemoteInfoChanged);

	connectSetCommitLog();

	connect(ui->tableWidget_log, &CommitLogTableWidget::currentRowChanged, this, &MainWindow::onCommitLogCurrentRowChanged);

	initUpdateFileListTimer();

	//

	QString path = getBookmarksFilePath();
	setRepositoryList(RepositoryBookmark::load(path));
	updateRepositoryList();

	// アイコン取得機能
	global->avatar_loader.connectAvatarReady(this, &MainWindow::onAvatarReady);

	connect(ui->widget_diff_view, &FileDiffWidget::textcodecChanged, [&](){ updateDiffView(); });

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

	m->git_process_thread.start();

	ui->action_sidebar->setChecked(true);

	showFileList(FileListType::MessagePanel);

	ui->action_restart_trace_logger->setVisible(global->appsettings.enable_trace_log);

	startTimers();
}

MainWindow::~MainWindow()
{
	m->git_process_thread.stop();

	global->avatar_loader.disconnectAvatarReady(this, &MainWindow::onAvatarReady);

	cancelPendingUserEvents();

	stopPtyProcess();

	deleteTempFiles();

	delete m;
	delete ui;
}

ProgressWidget *MainWindow::progress_widget() const
{
	return ui->widget_progress;
}

RepositoryData *MainWindow::currentRepositoryData()
{
	return &m->current_repository_data;
}

RepositoryData const *MainWindow::currentRepositoryData() const
{
	return &m->current_repository_data;
}

const GitBranch &MainWindow::currentBranch() const
{
	return m->current_branch;
}

void MainWindow::setCurrentBranch(const GitBranch &b)
{
	m->current_branch = b;
}

const RepositoryInfo &MainWindow::currentRepository() const
{
	return m->current_repository;
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

/**
 * @brief MainWindow::currentWorkingCopyDir
 * @return 現在の作業ディレクトリ
 *
 * 現在の作業ディレクトリを返す
 */
QString MainWindow::currentWorkingCopyDir() const
{
	return currentRepository().local_dir;
}

/**
 * @brief MainWindow::defaultWorkingDir
 *
 * デフォルトの作業ディレクトリを返す
 */
QString MainWindow::defaultWorkingDir() const
{
	return appsettings()->default_working_dir;
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
 * @brief ユーザーイベントをポストする
 * @param v データ
 * @param ms_later 遅延時間（0なら即座）
 */
void MainWindow::postUserEvent(UserEventHandler::variant_t &&v, int ms_later)
{
	postEvent(this, new UserEvent(std::move(v)), ms_later);
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
	postUserEvent(StartEventData(), ms_later);
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
		emitWriteLog({tmp, len});
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
			ui->tableWidget_log->setColumnWidth(0, n);
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
		global->writeLog(AboutDialog::appVersion() + '\n');
		// gitコマンドバージョン表示
		logGitVersion();
	}
}

void UserEventHandler::operator () (StartEventData const &)
{
	mainwindow->onStartEvent();
}

bool MainWindow::setCurrentLogRow(int row)
{
	if (row >= 0 && row < ui->tableWidget_log->rowCount()) {
		ui->tableWidget_log->setFocus();
		ui->tableWidget_log->setCurrentRow(row);
		return true;
	}
	return false;
}

int MainWindow::rowFromCommitId(GitHash const &id)
{
	ASSERT_MAIN_THREAD();

	if (id.isValid()) {
		auto it = commitlog().index.find(id);
		if (it != commitlog().index.end()) {
			return (int)it->second;
		}
	}
	return -1;
}

bool MainWindow::jumpToCommit(GitHash const &id)
{
	int row = rowFromCommitId(id);
	return setCurrentLogRow(row);
}

bool MainWindow::jumpToCommit(QString const &id)
{
	int row = rowFromCommitId(git().revParse(id));
	return setCurrentLogRow(row);
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
			// qDebug() << et << QString::asprintf("%08x", e->key());
			const int k = e->key();
			const bool alt = (e->modifiers() & Qt::AltModifier);
			const bool ctrl = (e->modifiers() & Qt::ControlModifier);
			const bool shift = (e->modifiers() & Qt::ShiftModifier);
			const bool enter = (k == Qt::Key_Enter || k == Qt::Key_Return);
			if (k == Qt::Key_Escape) {
				clearAllFilters();
				if (shift && centralWidget()->isAncestorOf(qApp->focusWidget())) {
					ui->treeWidget_repos->setFocus();
					return true;
				}
				return true;
			}
			if (k == Qt::Key_Tab || k == Qt::Key_Backtab) {
				clearAllFilters();
			}
			if (alt) {
				if (k == Qt::Key_1) {
					clearAllFilters();
					ui->treeWidget_repos->setFocus();
					return true;
				}
				if (k == Qt::Key_2) {
					clearAllFilters();
					ui->tableWidget_log->setFocus();
					return true;
				}
			}
			if (ctrl) {
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
			if (watched == ui->treeWidget_repos) { // リポジトリツリー
				if (enter) {
					if (ui->treeWidget_repos->isFiltered()) {
						chooseRepository();
						return true;
					}
					openSelectedRepository();
					return true;
				}
				if (ctrl) {
					if (k == Qt::Key_R) {
						onRepositoryTreeSortRecent(true);
						return true;
					}
				} else if (appendCharToFilterText(k, MainWindow::FilterTarget::RepositorySearch)) {
					return true;
				}
			} else if (watched == ui->tableWidget_log) {
				if (enter) {
					if (!m->incremental_search_text.isEmpty()) {
						applyFilter();
						return true;
					}
				}
				if (k == Qt::Key_Home) {
					setCurrentLogRow(0);
					return true;
				} else if (shift && k == Qt::Key_Escape) {
					ui->treeWidget_repos->setFocus();
					return true;
				} else if (appendCharToFilterText(k, MainWindow::FilterTarget::CommitLogSearch)) {
					return true;
				}
			} else if (watched == ui->listWidget_files || watched == ui->listWidget_unstaged || watched == ui->listWidget_staged) {
				if (shift && k == Qt::Key_Escape) {
					ui->tableWidget_log->setFocus();
					return true;
				}
			}
		}
	} else if (et == QEvent::FocusIn) {
		// ファイルリストがフォーカスを得たとき、diffビューを更新する。（コンテキストメニュー対応）
		if (watched == ui->listWidget_unstaged) {
			m->last_focused_file_list = watched;
			updateStatusBarText();
			return true;
		}
		if (watched == ui->listWidget_staged) {
			m->last_focused_file_list = watched;
			updateStatusBarText();
			return true;
		}
		if (watched == ui->listWidget_files) {
			m->last_focused_file_list = watched;
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
	} else if (et == (QEvent::Type)UserEventType) {
		UserEventHandler(this).go(static_cast<UserEvent *>(event));
		return true;
	}
	return QMainWindow::event(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	MySettings settings;

	m->update_file_list_timer.stop();
	m->update_commit_log_timer.stop();

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
		settings.setValue("FirstColumnWidth", ui->tableWidget_log->columnWidth(0));
		settings.endGroup();
	}

	QMainWindow::closeEvent(event);
}

void MainWindow::toggleMaximized()
{
	auto state = windowState();
	state ^= Qt::WindowMaximized;
	setWindowState(state);
}

void MainWindow::onLogVisibilityChanged()
{
	ui->action_window_log->setChecked(ui->dockWidget_log->isVisible());
}

void MainWindow::appendLogHistory(QByteArray const &str)
{
	m->log_history_bytes.insert(m->log_history_bytes.begin(), str.begin(), str.end());
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
	m->log_history_bytes.clear();
}

void MainWindow::internalWriteLog(LogData const &logdata)
{
	QByteArray ba = logdata.data;

	appendLogHistory(ba);

	if (ba.size() > 0) {
		int i = ba.size() - 1;
		if (ba[i] == '\n') {
			ba = ba.mid(0, i);
		}
	}

	std::string str;
	{
		std::string_view v(ba.data(), ba.size());
		std::vector<std::string> lines = misc::splitLines(v, false);
		for (std::string const &line : lines) {
			auto percent = line.find_last_of('%');
			auto colon = line.find_last_of(':', percent);
			if (colon != std::string::npos && percent != std::string::npos) {
				std::string title(misc::trimmed(line.substr(0, colon)));
				unsigned long long num, den;
				if (sscanf(line.data() + percent, "%% (%llu/%llu)", &num, &den) == 2) {
					showProgress(QString::fromStdString(title));
					setProgress((float)num / den);
				}
			}
			str += line;
			str += '\n';
		}
	}

	ui->widget_log->view()->logicalMoveToBottom();
	ui->widget_log->view()->appendBulk(str);
	ui->widget_log->view()->setChanged(false);
	ui->widget_log->updateLayoutAndMoveToBottom();

	setInteractionEnabled(true);
}

void MainWindow::setupStatusInfoHandler()
{
	connect(this, &MainWindow::signalShowStatusInfo, this, &MainWindow::onShowStatusInfo);
}

void MainWindow::onShowStatusInfo(StatusInfo const &info)
{
	ASSERT_MAIN_THREAD();
	if (info.progress_) {
		m->status_bar_label->hide();
		m->background_process_work_in_progress = true;
		if (info.message_) {
			// ui->statusBar->clearMessage();
			progress_widget()->setVisible(true);
			progress_widget()->setText(info.message_->text);
		}
		progress_widget()->setProgress(*info.progress_);
		internalShowPanel(FileListType::MessagePanel);
	} else {
		m->background_process_work_in_progress = false;
		progress_widget()->clear();
		progress_widget()->setVisible(false);
		if (info.message_) {
			m->status_bar_label->setTextFormat(info.message_->format);
			m->status_bar_label->setText(info.message_->text);
			m->status_bar_label->show();
		}
	}
}

void MainWindow::clearStatusInfo()
{
	setStatusInfo(StatusInfo::Clear());
}

void MainWindow::setProgress(float progress)
{
	StatusInfo info = StatusInfo::progress(progress);
	emit signalShowStatusInfo(info);
}

void MainWindow::showProgress(QString const &text, float progress)
{
	StatusInfo info = StatusInfo::progress(text, progress);
	emit signalShowStatusInfo(info);
}

void MainWindow::hideProgress()
{
	clearStatusInfo();
}

void MainWindow::setStatusInfo(StatusInfo const &info)
{
	emit signalShowStatusInfo(info);
}

void MainWindow::buildRepoTree(QString const &group, QTreeWidgetItem *item, QList<RepositoryInfo> *repos)
{
	QString name = RepositoryTreeWidget::treeItemName(item);
	if (RepositoryTreeWidget::isGroupItem(item)) {
		int n = item->childCount();
		for (int i = 0; i < n; i++) {
			QTreeWidgetItem *child = item->child(i);
			QString sub = group / name;
			buildRepoTree(sub, child, repos);
		}
	} else {
		RepositoryTreeIndex index = repositoryTreeIndex(item);
		std::optional<RepositoryInfo> repo = repositoryItem(index);
		if (repo) {
			RepositoryInfo newrepo = *repo;
			newrepo.name = name;
			newrepo.group = group;
			item->setData(0, RepositoryTreeWidgetItem::IndexRole, repos->size());
			repos->push_back(newrepo);
		}
	}
}

std::map<GitHash, TagList> const &MainWindow::tagmap() const
{
	return currentRepositoryData()->tag_map;
}

TagList MainWindow::findTag(std::map<GitHash, TagList> const &tagmap, GitHash const &id)
{
	auto it = tagmap.find(id);
	if (it != tagmap.end()) {
		return it->second;
	}
	return {};
}

TagList MainWindow::findTag(GitHash const &id) const
{
	return findTag(tagmap(), id);
}

TagList const &MainWindow::queryCurrentCommitTagList() const
{
	GitCommitItem const &commit = selectedCommitItem();
	return findTag(commit.commit_id);
}

QList<RepositoryInfo> const &MainWindow::repositoryList() const
{
	return m->repos;
}

void MainWindow::setRepositoryList(QList<RepositoryInfo> &&list)
{
	m->repos = std::move(list);
}

/**
 * @brief MainWindow::saveRepositoryBookmarks
 *
 * リポジトリブックマークをファイルに保存する
 */
bool MainWindow::saveRepositoryBookmarks()
{
	QString path = getBookmarksFilePath();
	return RepositoryBookmark::save(path, &repositoryList());
}

/**
 * @brief MainWindow::refrectRepositories()
 * @param repos
 *
 * リポジトリツリーの状態をリポジトリリストに反映する
 */
void MainWindow::refrectRepositories()
{
	QList<RepositoryInfo> newrepos;
	int n = ui->treeWidget_repos->topLevelItemCount();
	for (int i = 0; i < n; i++) {
		QTreeWidgetItem *item = ui->treeWidget_repos->topLevelItem(i);
		buildRepoTree(QString(), item, &newrepos);
	}
	setRepositoryList(std::move(newrepos));
	saveRepositoryBookmarks();
}

void MainWindow::onRepositoriesTreeDropped()
{
	refrectRepositories();
	QTreeWidgetItem *item = ui->treeWidget_repos->currentItem();
	if (item) item->setExpanded(true);
}

/**
 * @brief MainWindow::drawDigit
 * @param pr QPainter
 * @param x X座標
 * @param y Y座標
 * @param n 数字
 * @return
 *
 * 0〜9の小さい数字を描画する
 */
void MainWindow::drawDigit(QPainter *pr, int x, int y, int n) const
{
	int w = DIGIT_WIDTH;
	int h = DIGIT_HEIGHT;
	pr->drawPixmap(x, y, w, h, global->graphics->small_digits, n * w, 0, w, h);
}

/**
 * @brief MainWindow::signatureVerificationIcon
 * @param id コミットID
 *
 * コミットの署名検証結果に応じたアイコンを返す
 */
QIcon MainWindow::signatureVerificationIcon(GitHash const &id) const
{
	if (m->background_process_work_in_progress) {
		return {};
	}

	char sign_state = 0;

	{
		GitCommitItem const &commit = commitItem(id);
		if (commit.sign.verify) {
			sign_state = commit.sign.verify;
		} else if (commit.has_gpgsig) {
			auto a = m->commit_detail_getter.query(commit.commit_id, true);
			sign_state = a.sign_verify;
		}
	}

	GitSignatureGrade sg = Git::evaluateSignature(sign_state);
	switch (sg) {
	case GitSignatureGrade::Good: // 署名あり、検証OK
		return global->graphics->signature_good_icon;
	case GitSignatureGrade::Bad: // 署名あり、検証NG
		return global->graphics->signature_bad_icon;
	case GitSignatureGrade::Unknown:
	case GitSignatureGrade::Dubious:
	case GitSignatureGrade::Missing:
		return global->graphics->signature_dubious_icon; // 署名あり、検証不明
	}

	return {}; // 署名なし
}

/**
 * @brief MainWindow::addMenuActionProperty
 * @param menu メニュー
 *
 * プロパティメニューを追加する
 */
QAction *MainWindow::addMenuActionProperty(QMenu *menu)
{
	return menu->addAction(tr("&Property"));
}

/**
 * @brief MainWindow::color
 * @param depth 階層の深さ
 * @return 色
 *
 * 階層の深さに応じた色を返す
 */
QColor MainWindow::color(unsigned int depth)
{
	unsigned int n = (unsigned int)m->graph_color.width();
	if (n > 0) {
		n--;
		if (depth > n) depth = n;
		QRgb const *p = reinterpret_cast<QRgb const *>(m->graph_color.scanLine(0));
		return QColor(qRed(p[depth]), qGreen(p[depth]), qBlue(p[depth]));
	}
	return Qt::black;
}

/**
 * @brief MainWindow::findRegisteredRepository
 * @param workdir 作業ディレクトリ
 *
 * 登録済みのリポジトリを探す
 */
RepositoryInfo const *MainWindow::findRegisteredRepository(QString *workdir) const
{
	*workdir = QDir(*workdir).absolutePath();
	workdir->replace('\\', '/');

	if (const_cast<MainWindow *>(this)->isValidWorkingCopy(*workdir)) {
		for (RepositoryInfo const &item : repositoryList()) {
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

/**
 * @brief MainWindow::revertAllFiles
 *
 * すべてのファイルをHEADに戻す
 */
void MainWindow::revertAllFiles()
{
	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return;

	QString cmd = "git reset --hard HEAD";
	if (askAreYouSureYouWantToRun(tr("Revert all files"), "> " + cmd)) {
		g.resetAllFiles();
		reopenRepository(true);
	}
}

/**
 * @brief MainWindow::execSetGlobalUserDialog
 *
 * グローバルユーザー設定ダイアログを表示する
 */
bool MainWindow::execSetGlobalUserDialog()
{
	SetGlobalUserDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		GitRunner g = git();
		GitUser user = dlg.user();
		g.setUser(user, true);
		updateWindowTitle(g);
		return true;
	}
	return false;
}

QString MainWindow::preferredRepositoryGroup() const
{
	return m->add_repository_into_group;
}

/**
 * @brief MainWindow::setPreferredRepositoryGroup
 * @param group グループ
 *
 * リポジトリ追加時のデフォルトグループを設定する
 */
void MainWindow::setPreferredRepositoryGroup(QString const &group)
{
	m->add_repository_into_group = group;
}

/**
 * @brief MainWindow::saveRepositoryBookmark
 * @param item リポジトリ情報
 *
 * リポジトリブックマークを保存する
 */
void MainWindow::saveRepositoryBookmark(RepositoryInfo item)
{
	if (item.local_dir.isEmpty()) return;

	if (item.name.isEmpty()) {
		item.name = tr("Unnamed");
	}

	item.group = preferredRepositoryGroup();

	QList<RepositoryInfo> repos = repositoryList();

	bool done = false;
	for (auto &repo : repos) {
		RepositoryInfo *p = &repo;
		if (item.local_dir == p->local_dir) {
			*p = item;
			done = true;
			break;
		}
	}
	if (!done) {
		repos.push_back(item);
	}
	setRepositoryList(std::move(repos));
	saveRepositoryBookmarks();
	updateRepositoryList();
}

void UserEventHandler::operator () (CloneRepositoryEventData const &e)
{
	AddRepositoryDialog dlg(mainwindow);
	if (dlg.execClone(e.remote_url) == QDialog::Accepted) {
		mainwindow->addRepositoryAccepted(dlg);
	}
}

/**
 * @brief MainWindow::clearAuthentication
 *
 * 認証情報をクリアする
 */
void MainWindow::clearAuthentication()
{
	clearSshAuthentication();
	m->http_uid.clear();
	m->http_pwd.clear();
}

/**
 * @brief MainWindow::clearSshAuthentication
 *
 * SSH認証情報をクリアする
 */
void MainWindow::clearSshAuthentication()
{
	m->ssh_passphrase_user.clear();
	m->ssh_passphrase_pass.clear();
}

/**
 * @brief MainWindow::setCurrentRepository
 * @param repo リポジトリ
 * @param clear_authentication 認証情報をクリアする
 *
 * 現在のリポジトリを設定する
 */
void MainWindow::setCurrentRepository(const RepositoryInfo &repo, bool clear_authentication)
{
	if (clear_authentication) {
		clearAuthentication();
	}
	m->current_repository = repo;
	clearGitCommandCache();
	// getObjCache()->clear();
	clearGitObjectCache();
}

void MainWindow::endSession()
{
	// qDebug() << Q_FUNC_INFO;
	setCurrentGitRunner({});
	clearGitCommandCache();
	// getObjCache()->clear();
	clearGitObjectCache();
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

ApplicationSettings *MainWindow::appsettings()
{
	return &global->appsettings;
}

const ApplicationSettings *MainWindow::appsettings() const
{
	return &global->appsettings;
}

const GitCommitItem *MainWindow::getLog(int index) const
{
	GitCommitItemList const &logs = commitlog();
	return (index >= 0 && index < (int)logs.size()) ? &logs[index] : nullptr;
}

void MainWindow::onSetCommitLog(CommitLogExchangeData const &log)
{
	ASSERT_MAIN_THREAD();

	if (log.p->commit_log) currentRepositoryData()->commit_log = *log.p->commit_log;
	if (log.p->branch_map) currentRepositoryData()->branch_map = *log.p->branch_map;
	if (log.p->tag_map) currentRepositoryData()->tag_map = *log.p->tag_map;
	if (log.p->label_map) currentRepositoryData()->label_map = *log.p->label_map;

	_updateCommitLogTableView(0); // コミットログテーブルの表示を更新

	updateStatusBarText();
}

void MainWindow::setCommitLog(const CommitLogExchangeData &exdata)
{
	emit sigSetCommitLog(exdata);
}

void MainWindow::connectSetCommitLog()
{
	connect(this, &MainWindow::sigSetCommitLog, this, &MainWindow::onSetCommitLog);
}

void MainWindow::updateUncommitedChanges(GitRunner g)
{
	TraceLogger trace("updateUncommitedChanges", {});
	m->uncommited_changes_file_list = g.status_s();
	setUncommitedChanges(!m->uncommited_changes_file_list.empty());
}

std::map<GitHash, TagList> MainWindow::queryTags(GitRunner g)
{
	TraceLogger trace("queryTags", {});
	std::map<GitHash, TagList> tag_map;

	TagList tags = g.tags();
	for (GitTag const &tag : tags) {
		tag_map[tag.id].push_back(tag);
	}

	return tag_map;
}

std::optional<GitCommitItem> MainWindow::getCommitItem(GitRunner g, GitHash const &commit_id) const
{
	auto obj = g.catFile(commit_id);
	if (obj.type == GitObject::Type::COMMIT) {
		std::optional<GitCommitItem> item = Git::parseCommit(obj.content);
		if (item) {
			item->commit_id = GitHash(commit_id);
			return item;
		}
	}
	return std::nullopt;
}

GitCommitItemList MainWindow::log_all2(GitRunner g, GitHash const &id, int maxcount) const
{
	TraceLogger trace("log_all2", {});
#if 1
	return g.log_all(id, maxcount);
#else
	GitCommitItemList items;

	std::vector<GitHash> revlist = g.rev_list_all(id, maxcount);

	if (0) { // シングルスレッド版
		for (GitHash const &hash : revlist) {
			auto item = getCommitItem(g, hash);
			if (item) {
				items.list.push_back(*item);
			}
		}
	} else { // マルチスレッド版
		std::vector<std::pair<size_t, GitCommitItem>> vec(revlist.size());
		std::atomic_size_t in = 0;
		std::atomic_size_t to = 0;
		std::vector<std::thread> threads(8);
		for (size_t i = 0; i < threads.size(); i++) {
			threads[i] = std::thread([&](){
				while (1) {
					size_t j = in++;
					if (j >= revlist.size()) break;
					GitHash const &hash = revlist[j];
#if 1
					auto item = getCommitItem(g, hash);
					if (item) {
						vec.at(to++) = {j, *item};
					}
#else
					GitCommitItem item;
					item.commit_id = GitHash(hash);
					vec.at(to++) = {j, item};
#endif
				}
			});
		}
		for (size_t i = 0; i < threads.size(); i++) {
			threads[i].join();
		}
		vec.resize(to);
		std::sort(vec.begin(), vec.end(), [](auto const &a, auto const &b){
			return a.first < b.first;
		});
		items.list.reserve(vec.size());
		for (auto const &pair : vec) {
			items.list.push_back(pair.second);
		}
	}

	return items;
#endif
}

static void fixCommitLogOrder(GitCommitItemList *list)
{
	GitCommitItemList list2;
	std::swap(list2.list, list->list);

	const size_t count = list2.size();

	std::vector<size_t> index(count);
	std::iota(index.begin(), index.end(), 0);

	auto LISTITEM = [&](size_t i)->GitCommitItem &{
		return list2[index[i]];
	};

	auto MOVEITEM = [&](size_t from, size_t to){
		Q_ASSERT(from > to);
		// 親子関係が整合しないコミットをリストの上の方へ移動する
		LISTITEM(from).order_fixed = true;
		auto a = index[from];
		index.erase(index.begin() + from);
		index.insert(index.begin() + to, a);
	};

	// 親子関係を調べて、順番が狂っていたら、修正する。

	std::set<GitHash> set;
	size_t limit = count;
	size_t i = 0;
	while (i < count) {
		size_t newpos = (size_t)-1;
		for (GitHash const &parent : LISTITEM(i).parent_ids) {
			if (set.find(parent) != set.end()) {
				for (size_t j = 0; j < i; j++) {
					if (parent == LISTITEM(j).commit_id) {
						if (newpos == (size_t)-1 || j < newpos) {
							newpos = j;
						}
						// qDebug() << "fix commit order" << list[i].commit_id.toQString();
						break;
					}
				}
			}
		}
		set.insert(set.end(), LISTITEM(i).commit_id);
		if (newpos != (size_t)-1) {
			if (limit == 0) break; // まず無いと思うが、もし、無限ループに陥ったら
			MOVEITEM(i, newpos);
			i = newpos;
			limit--;
		}
		i++;
	}

	//

	list->list.reserve(count);
	for (size_t i : index) {
		list->list.push_back(LISTITEM(i));
	}
}

GitCommitItemList MainWindow::retrieveCommitLog(GitRunner g) const
{
	TraceLogger trace("retrieveCommitLog", {});
	GitCommitItemList list = log_all2(g, {}, limitLogCount());
	fixCommitLogOrder(&list);
	list.updateIndex();
	return list;
}

/**
 * @brief MainWindow::queryCommitLog
 * @param p
 * @param g
 *
 * コミットログとブランチ情報を取得
 */
CommitLogExchangeData MainWindow::queryCommitLog(GitRunner g)
{
	TraceLogger trace("quercyCommitLog", {});
	auto async_branches = std::async(std::launch::async, [&](){
		return g.branches(); // ブランチを取得;
	});
	auto async_update_uncommited = std::async(std::launch::async, [&](){
		updateUncommitedChanges(g);
	});
	auto async_tags = std::async(std::launch::async, [&](){
		return queryTags(g);
	});

	GitCommitItemList commit_log = retrieveCommitLog(g); // コミットログを取得 (TODO: slow)

	std::map<GitHash, BranchList> branch_map;

	// Uncommited changes がある場合、その親を取得するためにブランチ情報が必要
	auto branches = async_branches.get();
	for (GitBranch const &b : branches) {
		if (b.isCurrent()) {
			setCurrentBranch(b);
		}
		branch_map[b.id].append(b);
	}

	// Uncommited changes の処理
	async_update_uncommited.wait();
	if (isThereUncommitedChanges()) {
		GitCommitItem item;
		item.parent_ids.push_back(currentBranch().id);
		item.message = tr("Uncommited changes");
		commit_log.list.insert(commit_log.list.begin(), item);
		commit_log.updateIndex();
	}

	CommitLogExchangeData exdata;
	exdata.p->commit_log = commit_log;
	exdata.p->branch_map = branch_map;
	exdata.p->tag_map = async_tags.get();
	return exdata;
}

/**
 * @brief コミットログテーブルウィジェットを構築
 */
void MainWindow::makeCommitLog(GitHash const &head, CommitLogExchangeData exdata, int scroll_pos, int select_row)
{
	ASSERT_MAIN_THREAD();

	setHeadId(head);

	GitCommitItemList *commit_log = &exdata.p->commit_log.value();
	Q_ASSERT(commit_log);

	std::map<GitHash, BranchList> const &branch_map = exdata.p->branch_map.value();
	std::map<GitHash, TagList> const &tag_map = exdata.p->tag_map.value();

	exdata.p->label_map = std::map<int, BranchLabelList>();
	std::map<int, BranchLabelList> *label_map = &exdata.p->label_map.value();

	auto UpdateCommitGraph = std::async(std::launch::async, [&](){
		updateCommitGraph(commit_log);
	});

	const int count = (int)commit_log->size(); // ログの数

	std::vector<CommitRecord> records;
	records.reserve(count);

	int selrow = 0;

	for (int row = 0; row < count; row++) {
		GitCommitItem const &commit = (*commit_log)[row];

		auto [message_ex, labels] = makeCommitLabels(commit, branch_map, tag_map); // コミットコメントのツールチップ用テキストとラベル
		(*label_map)[row] = labels;

		CommitRecord rec;

		bool isHEAD = (commit.commit_id == getHeadId());
		if (Git::isUncommited(commit)) { // 未コミットの時
			rec.bold = true; // 太字
			selrow = row;
		} else {
			bool uncommited_changes = isThereUncommitedChanges();
			if (isHEAD && !uncommited_changes) { // HEADで、未コミットがないとき
				rec.bold = true; // 太字
				selrow = row;
			}
			rec.commit_id = abbrevCommitID(commit);
		}

		rec.datetime = misc::makeDateTimeString(commit.commit_date);
		rec.author = commit.author;
		rec.message = commit.message;
		rec.tooltip = rec.message + message_ex;

		records.push_back(rec);
	}

	m->last_focused_file_list = nullptr;

	{
		bool b = ui->tableWidget_log->blockSignals(true); // surpress tableWidget_log's signals
		ui->tableWidget_log->setRecords(std::move(records));
		ui->tableWidget_log->setFocus();

		{
			if (select_row < 0) {
				setCurrentLogRow(selrow);
			} else {
				setCurrentLogRow(select_row);
				ui->tableWidget_log->verticalScrollBar()->setValue(scroll_pos >= 0 ? scroll_pos : 0);
			}
		}

		ui->tableWidget_log->blockSignals(b);
	}
	// qDebug() << __FILE__ << __LINE__;
	onLogCurrentItemChanged(false); // force update tableWidget_log

	updateUI();

	UpdateCommitGraph.wait();
	setCommitLog(exdata);
}

void MainWindow::openRepositoryMain(OpenRepositoryOption const &opt)
{
	ASSERT_MAIN_THREAD();
	TraceLogger trace("openRepositoryMain", {});

	cancelUpdateFileList();

	GitRunner g = git();

	if (opt.clear_log) { // ログをクリア
		m->clearCurrentRepositoryData();
		{ // コミットログをクリア
			ui->tableWidget_log->setRecords(std::vector<CommitRecord>());
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		}
	} else {
		// getObjCache()->clear();
		clearGitObjectCache();
	}

	if (opt.new_session) {
		endSession();
		// currentRepositoryData()->git_command_cache = {};

		RepositoryInfo const &item = currentRepository();
		g = new_git_runner(item.local_dir, item.ssh_key);
	}
	setCurrentGitRunner(g);
	if (!isValidWorkingCopy(g)) return;

	PtyProcess *pty = getPtyProcess();
	if (pty) {
		if (!pty->wait(5000)) /*return*/; //@ something wrong
	}

	// リポジトリ情報をクリア
	{
		clearLabelMap();
		setUncommitedChanges(false);
		clearLogContents();

		internalClearRepositoryInfo();
		ui->label_repo_name->setText(QString());
		ui->label_branch_name->setText(QString());
	}

	// HEAD を取得
	auto async_head = std::async(std::launch::async, [&](){
		return g.revParse("HEAD");
	});
	// ユーザー情報を取得
	auto async_user = std::async(std::launch::async, [&](){
		return g.getUser(GitSource::Default);
	});
	// コミットログを取得
	auto async_exdata = std::async(std::launch::async, [&](){
		return queryCommitLog(g);
	});

	// コミットログを作成
	{
		int scroll_pos = -1;
		int select_row = -1;
		if (opt.keep_selection) {
			scroll_pos = ui->tableWidget_log->verticalScrollBar()->value();
			select_row = ui->tableWidget_log->currentRow();
		}

		auto head = async_head.get();
		auto exdata = async_exdata.get();
		makeCommitLog(head, exdata, scroll_pos, select_row);
	}

	// ウィンドウタイトルを更新
	updateWindowTitle(async_user.get());

	// ポジトリの情報を設定
	{
		const QString br_name = currentBranchName();
		const QString repo_name = currentRepositoryName();

		QString info;
		if (currentBranch().flags & GitBranch::HeadDetachedAt) {
			info += QString("(HEAD detached at %1)").arg(br_name);
		}
		if (currentBranch().flags & GitBranch::HeadDetachedFrom) {
			info += QString("(HEAD detached from %1)").arg(br_name);
		}
		if (info.isEmpty()) {
			info = br_name;
		}

		setRepositoryInfo(repo_name, info);
	}

	bool do_fetch = opt.do_fetch;
	if (do_fetch) {
		do_fetch = isOnlineMode() && appsettings()->automatically_fetch_when_opening_the_repository;
	}
	if (do_fetch) {
		fetch(g, false);
	} else {
		// qDebug() << __FILE__ << __LINE__;
		onLogCurrentItemChanged(true); // ファイルリストを更新
	}

	m->commit_detail_getter.stop();
	m->commit_detail_getter.start(g.dup());
}

/**
 * @brief MainWindow::openRepository
 * @param validate バリデート
 * @param wait_cursor ウェイトカーソル
 * @param keep_selection 選択を保持する
 *
 * リポジトリを開く
 */
void MainWindow::openRepository(OpenRepositoryOption const &opt)
{
	struct DeferClearAllFilters {
		~DeferClearAllFilters()
		{
			global->mainwindow->clearFilterText();
		}
	} defer_clear_all_filters; // 関数終了時にフィルタをクリアする

	if (opt.validate) {
		QString dir = currentWorkingCopyDir();
		if (!QFileInfo(dir).isDir()) {
			int r = QMessageBox::warning(this, tr("Open Repository"), dir + "\n\n" + tr("No such folder") + "\n\n" + tr("Remove from bookmark?"), QMessageBox::Ok, QMessageBox::Cancel);
			if (r == QMessageBox::Ok) {
				removeSelectedRepositoryFromBookmark(false);
			}
			return;
		}
		if (!unassosiated_git_runner().isValidWorkingCopy(dir)) {
			QMessageBox::warning(this, tr("Open Repository"), tr("Not a valid git repository") + "\n\n" + dir);
			return;
		}
	}

	if (opt.wait_cursor) {
		OpenRepositoryOption opt2 = opt;
		opt2.validate = false;
		opt2.wait_cursor = false;
		openRepository(opt2);
		return;
	}

	openRepositoryMain(opt);
}

void MainWindow::reopenRepository(bool validate)
{
	OpenRepositoryOption opt;
	opt.new_session = false;
	opt.validate = validate;
	openRepository(opt);
}

/**
 * @brief MainWindow::openSelectedRepository
 *
 * 選択されたリポジトリを開く
 */
void MainWindow::openSelectedRepository()
{
	std::optional<RepositoryInfo> repo = selectedRepositoryItem();

	if (repo) {
		setCurrentRepository(*repo, true);
		// reopenRepository(true);
		OpenRepositoryOption opt;
		opt.validate = true;
		openRepository(opt);
	}
}

/**
 * @brief MainWindow::addExistingLocalRepository
 * @param dir ディレクトリ
 * @param name 名前
 * @param sshkey SSHキー
 * @param open 開く
 *
 * 既存のリポジトリを追加する
 */
bool MainWindow::_addExistingLocalRepository(QString dir, QString name, QString sshkey, bool open, bool save, bool msgbox_if_err)
{
#ifdef Q_OS_WIN
	dir = dir.replace('\\', '/');
#endif

	if (dir.endsWith(".git")) {
		auto i = dir.size();
		if (i > 4) {
			ushort c = dir.utf16()[i - 5];
			if (c == '/') {
				dir = dir.mid(0, i - 5);
			}
		}
	}

	dir = misc::normalizePathSeparator(dir);

	if (!unassosiated_git_runner().isValidWorkingCopy(dir)) {
		auto isBareRepository = [](QString const &dir){
			if (QFileInfo(dir).isDir()) {
				if (QFileInfo(dir / "config").isFile()) {
					if (QFileInfo(dir / "objects").isDir()) {
						if (QFileInfo(dir / "refs").isDir()) {
							return true;
						}
					}
				}
			}
			return false;
		};

		if (isBareRepository(dir)) {
			postUserEvent(CloneRepositoryEventData(dir), 0);
			return true;
		}

		if (msgbox_if_err && QFileInfo(dir).isDir()) {
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

	RepositoryInfo item;
	item.local_dir = dir;
	item.name = name;
	item.group = preferredRepositoryGroup();
	item.ssh_key = sshkey;
	if (save) {
		saveRepositoryBookmark(item);
	}

	if (open) {
		setCurrentRepository(item, true);
		OpenRepositoryOption opt;
		opt.clear_log = true;
		opt.do_fetch = false;
		opt.keep_selection = false;
		openRepositoryMain(opt);
	}

	return true;
}

/**
 * @brief MainWindow::addExistingLocalRepositoryWithGroup
 * @param dir ディレクトリ
 * @param group グループ
 *
 * 既存のリポジトリを追加する
 */
void MainWindow::addExistingLocalRepositoryWithGroup(const QString &dir, const QString &group)
{
	QFileInfo info1(dir);
	if (info1.isDir()) {
		QFileInfo info2(dir / ".git");
		if (info2.isDir()) {
			RepositoryInfo item;
			item.local_dir = info1.absoluteFilePath();
			item.name = makeRepositoryName(item.local_dir);
			item.group = group;
			m->repos.append(item);
		}
	}
}

bool MainWindow::addExistingLocalRepository(const QString &dir, bool open)
{
	return _addExistingLocalRepository(dir, {}, {}, open);
}

/**
 * @brief MainWindow::execWelcomeWizardDialog
 *
 * ようこそダイアログを表示する
 */
bool MainWindow::execWelcomeWizardDialog()
{
	WelcomeWizardDialog dlg(this);
	dlg.set_git_command_path(appsettings()->git_command);
	dlg.set_default_working_folder(appsettings()->default_working_dir);
	if (misc::isExecutable(appsettings()->git_command)) {
		GitRunner g = unassosiated_git_runner();
		// Git g(global->gcx(), {}, {}, {});
		GitUser user = g.getUser(GitSource::Global);
		dlg.set_user_name(user.name);
		dlg.set_user_email(user.email);
	}
	if (dlg.exec() == QDialog::Accepted) {
		setGitCommand(dlg.git_command_path(), false);
		appsettings()->default_working_dir = dlg.default_working_folder();
		saveApplicationSettings();

		if (misc::isExecutable(appsettings()->git_command)) {
			GitRunner g = git();
			GitUser user;
			user.name = dlg.user_name();
			user.email = dlg.user_email();
			g.setUser(user, true);

			QString old_default_branch_name = g.getDefaultBranch();
			QString new_default_branch_name = dlg.default_branch_name();
			if (old_default_branch_name != new_default_branch_name) {
				if (new_default_branch_name.isEmpty()) {
					g.unsetDefaultBranch();
				} else {
					g.setDefaultBranch(new_default_branch_name);
				}
			}
		}

		return true;
	}
	return false;
}

void MainWindow::onRemoteInfoChanged()
{
	ui->lineEdit_remote->setText(currentRemoteName());
}

/**
 * @brief MainWindow::execRepositoryPropertyDialog
 * @param repo リポジトリ
 * @param open_repository_menu リポジトリメニューを開く
 *
 * リポジトリプロパティダイアログを表示する
 */
void MainWindow::execRepositoryPropertyDialog(const RepositoryInfo &repo, bool open_repository_menu)
{
	QString workdir = repo.local_dir;

	if (workdir.isEmpty()) {
		workdir = currentWorkingCopyDir();
	}
	QString name = repo.name;
	if (name.isEmpty()) {
		name = makeRepositoryName(workdir);
	}
	GitRunner g = new_git_runner(workdir, repo.ssh_key);
	RepositoryPropertyDialog dlg(this, g, repo, open_repository_menu);
	dlg.exec();
	if (dlg.isRemoteChanged()) {
		emit remoteInfoChanged();
	}
	if (dlg.isNameChanged()) {
		this->changeRepositoryBookmarkName(repo, dlg.getName());
	}
}

/**
 * @brief MainWindow::execConfigUserDialog
 * @param global_user グローバルユーザー
 * @param local_user ローカルユーザー
 * @param enable_local_user ローカルユーザーを有効にする
 * @param reponame リポジトリ名
 *
 * ユーザー設定ダイアログを表示する
 */
void MainWindow::execConfigUserDialog(const GitUser &global_user, const GitUser &local_user, bool enable_local_user, const QString &reponame)
{
	ConfigUserDialog dlg(this, global_user, local_user, enable_local_user, reponame);
	if (dlg.exec() == QDialog::Accepted) {
		GitRunner g = git();
		GitUser user;

		// global
		user = dlg.user(true);
		if (user) {
			if (user.email != global_user.email || user.name != global_user.name) {
				g.setUser(user, true);
			}
		}

		// local
		if (dlg.isLocalUnset()) {
			g.setUser({}, false);
		} else {
			user = dlg.user(false);
			if (user) {
				if (user.email != local_user.email || user.name != local_user.name) {
					g.setUser(user, false);
				}
			}
		}

		updateWindowTitle(g);
	}
}

/**
 * @brief MainWindow::setGitCommand
 * @param path パス
 * @param save 保存する
 *
 * gitコマンドのパスを設定する
 */
void MainWindow::setGitCommand(QString const &path, bool save)
{
	appsettings()->git_command = executableOrEmpty(path);

	internalSaveCommandPath(path, save, "GitCommand");
}

/**
 * @brief MainWindow::setGpgCommand
 * @param path パス
 * @param save 保存する
 *
 * gpgコマンドのパスを設定する
 */
void MainWindow::setGpgCommand(QString const &path, bool save)
{
	appsettings()->gpg_command = executableOrEmpty(path);

	internalSaveCommandPath(appsettings()->gpg_command, save, "GpgCommand");
	if (!global->appsettings.gpg_command.isEmpty()) {
		GitRunner g = unassosiated_git_runner();
		g.configGpgProgram(global->appsettings.gpg_command, true);
	}
}

/**
 * @brief MainWindow::setSshCommand
 * @param path パス
 * @param save 保存する
 *
 * sshコマンドのパスを設定する
 */
void MainWindow::setSshCommand(QString const &path, bool save)
{
	appsettings()->ssh_command = executableOrEmpty(path);

	internalSaveCommandPath(path, save, "SshCommand");
}

/**
 * @brief MainWindow::checkGitCommand
 *
 * gitコマンドの有効性をチェックする
 */
bool MainWindow::checkGitCommand()
{
	while (1) {
		if (misc::isExecutable(global->appsettings.git_command)) {
			return true;
		}
		if (selectGitCommand(true).isEmpty()) {
			close();
			return false;
		}
	}
}

/**
 * @brief MainWindow::saveByteArrayAs
 * @param ba バイト配列
 * @param dstpath 保存先
 * @return
 *
 * バイト配列を保存する
 */
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

/**
 * @brief MainWindow::makeRepositoryName
 * @param loc ロケーション
 *
 * リポジトリ名を作成する
 */
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

/**
 * @brief MainWindow::saveFileAs
 * @param srcpath 保存元
 * @param dstpath 保存先
 *
 * ファイルを保存する
 */
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

/**
 * @brief MainWindow::saveBlobAs
 * @param id ID
 * @param dstpath 保存先
 *
 * ファイルを保存する
 */
bool MainWindow::saveBlobAs(const QString &id, const QString &dstpath)
{
	GitObject obj = git().catFile(GitHash(id));
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


/**
 * @brief MainWindow::checkExecutable
 * @param path パス
 * @return
 *
 * 実行可能かチェックする
 */
QString MainWindow::executableOrEmpty(const QString &path)
{
	if (!path.isEmpty() && checkExecutable(path)) {
		return path;
	}
	return QString();
}

/**
 * @brief MainWindow::checkExecutable
 * @param path パス
 * @return
 *
 * 実行可能かチェックする
 */
bool MainWindow::checkExecutable(const QString &path)
{
	if (QFileInfo(path).isExecutable()) {
		return true;
	}
	QString text = "The specified program '%1' is not executable.\n";
	text = text.arg(path);
	global->writeLog(text);
	return false;
}

/**
 * @brief MainWindow::internalSaveCommandPath
 * @param path パス
 * @param save 保存する
 * @param name 名前
 *
 * コマンドのパスを保存する
 */
void MainWindow::internalSaveCommandPath(const QString &path, bool save, const QString &name)
{
	if (save) {
		MySettings s;
		s.beginGroup("Global");
		s.setValue(name, path);
		s.endGroup();
	}
}

/**
 * @brief MainWindow::logGitVersion
 *
 * gitコマンドのバージョンをログに書き込む
 */
void MainWindow::logGitVersion()
{
	// GitRunner g = git();
	GitRunner g = unassosiated_git_runner();
	QString s = g.version();
	if (!s.isEmpty()) {
		s += '\n';
		global->writeLog(s);
	}
}

/**
 * @brief MainWindow::internalClearRepositoryInfo
 *
 * リポジトリ情報をクリアする
 */
void MainWindow::internalClearRepositoryInfo()
{
	setHeadId({});
	setCurrentBranch(GitBranch());
}

/**
 * @brief MainWindow::checkUser
 *
 * ユーザーをチェックする
 */
void MainWindow::checkUser()
{
	GitRunner g = unassosiated_git_runner();
	// Git g(global->gcx(), {}, {}, {});
	while (1) {
		GitUser user = g.getUser(GitSource::Global);
		if (!user.name.isEmpty() && !user.email.isEmpty()) {
			return; // ok
		}
		if (!execSetGlobalUserDialog()) { // ユーザー設定ダイアログを表示
			return;
		}
	}
}

/**
 * @brief MainWindow::makeDiffs
 * @param id ID
 *
 * 差分を作成する
 */
std::optional<QList<GitDiff>> MainWindow::makeDiffs(GitRunner g, GitHash id, std::future<QList<GitSubmoduleItem>> &&async_modules)
{
	if (!isValidWorkingCopy(g)) return std::nullopt;

	if (!id && !isThereUncommitedChanges()) {
		id = GitHash(getObjCache()->revParse(g, "HEAD"));
	}

	QList<GitSubmoduleItem> mods = async_modules.get();
	setSubmodules(mods);

	bool uncommited = (!id && isThereUncommitedChanges());

	GitDiffManager dm(getObjCache());
	if (uncommited) {
		return dm.diff_uncommited(g, submodules());
	} else {
		return dm.diff(g, id, submodules());
	}
}

/**
 * @brief MainWindow::updateRemoteInfo
 *
 * リモート情報を更新する
 */
void MainWindow::updateRemoteInfo()
{
	ASSERT_MAIN_THREAD();

	{
		GitRunner g = git();
		m->remotes = g.getRemotes();
		std::sort(m->remotes.begin(), m->remotes.end());
	}

	m->current_remote_name = QString();
	{
		GitBranch const &r = currentBranch();
		m->current_remote_name = r.remote;
	}
	if (m->current_remote_name.isEmpty()) {
		if (m->remotes.size() == 1) {
			m->current_remote_name = m->remotes[0];
		}
	}

	emit remoteInfoChanged();
}

void MainWindow::internalAfterFetch()
{
	PROFILE;
	ASSERT_MAIN_THREAD();

	clearGitCommandCache();

	updateRemoteInfo();

	OpenRepositoryOption opt;
	opt.new_session = false;
	opt.clear_log = false;
	opt.do_fetch = false;
	opt.keep_selection = true;
	openRepositoryMain(opt);
}

#define RUN_PTY_CALLBACK [this](ProcessStatus *status, QVariant const &userdata)
void MainWindow::runPtyGit(QString const &progress_message, GitRunner g, GitCommandRunner::variant_t var, std::function<void (ProcessStatus *status, QVariant const &userdata)> callback, QVariant const &userdata)
{
	showProgress(progress_message, -1.0f);

	GitCommandRunner runner;
	runner.d.process_name = progress_message;
	runner.d.elapsed.start();

	runner.d.run = [this, var](GitCommandRunner &runner){
		setCompletedHandler([this](bool ok, QVariant const &d){
			GitCommandRunner const &req = d.value<GitCommandRunner>();
			PtyProcessCompleted data;
			data.process_name = req.d.process_name;
			data.callback = req.callback;
			data.status->ok = ok;
			data.status->exit_code = req.pty()->getExitCode();
			data.status->log_message = req.pty_message();
			data.userdata = req.d.userdata;
			data.elapsed.start();
			emit sigPtyProcessCompleted(true, data);
		}, QVariant::fromValue(runner));
		setPtyProcessOk(true);

		std::visit(runner, var);

		if (runner.d.result) {
			// nop
		} else {
			emit sigPtyProcessCompleted(false, {});
		}
	};

	runner.d.g = g.dup();
	runner.callback = callback;
	runner.d.userdata = userdata;
	runner.d.pty = getPtyProcess();
	getPtyProcess()->clearMessage();
	m->git_process_thread.request(std::move(runner));
}

void MainWindow::onPtyProcessCompleted(bool ok, PtyProcessCompleted const &data)
{
	ASSERT_MAIN_THREAD();

	clearStatusInfo();

	if (ok) {
		if (data.callback) {
			data.callback(data.status.get(), data.userdata);
		}
	}

	// currentRepositoryData()->git_command_cache = {};
}

void MainWindow::connectPtyProcessCompleted()
{
	connect(this, &MainWindow::sigPtyProcessCompleted, this, &MainWindow::onPtyProcessCompleted);
}

/**
 * @brief MainWindow::doReopenRepository
 * @param status ステータス
 * @param userdata ユーザーデータ
 *
 * リポジトリを再オープンする
 */
void MainWindow::doReopenRepository(ProcessStatus *status, RepositoryInfo const &repodata)
{
	ASSERT_MAIN_THREAD();

	if (status->ok) {
		saveRepositoryBookmark(repodata);
		setCurrentRepository(repodata, false);
		reopenRepository(true);
	}
}

std::string MainWindow::parseDetectedDubiousOwnershipInRepositoryAt(std::vector<std::string> const &lines)
{
	static std::string git_config_global_add_safe_directory = "git config --global --add safe.directory";
	std::string dir;
	bool detected_dubious_ownership_in_repository_at = false;
	size_t i = 0;
	for (i = 0; i < lines.size(); i++) {
		std::string const &line = lines[i];
		if (misc::starts_with(line, "fatal: detected dubious ownership in repository at ")) {
			detected_dubious_ownership_in_repository_at = true;
		} else if (detected_dubious_ownership_in_repository_at) {
			auto pos = line.find(git_config_global_add_safe_directory);
			if (pos != std::string::npos) {
				dir = line.substr(pos + git_config_global_add_safe_directory.size());
				if (i < lines.size()) {
					std::string next = lines[i + 1];
					if (next[0] != 0 && !isspace((unsigned char)next[0]) && !misc::starts_with(next, "fatal:")) {
						dir += next;
					}
				}
				dir = misc::trimQuotes(dir);
				break;
			}
		}
	}
	return dir;
}

/**
 * @brief MainWindow::cloneRepository
 * @param clonedata クローンデータ
 * @param repodata リポジトリデータ
 * @return
 *
 * リポジトリをクローンする
 */
void MainWindow::clone(CloneParams const &a)
{
	GitRunner g = new_git_runner({}, a.repodata.ssh_key);
	runPtyGit(tr("Cloning..."), g, Git_clone{a.clonedata}, RUN_PTY_CALLBACK{
		CloneParams a = userdata.value<CloneParams>();
		std::vector<std::string> log = misc::splitLines(status->log_message, false);
		std::string dir = parseDetectedDubiousOwnershipInRepositoryAt(log);
		if (dir.empty()) {
			doReopenRepository(status, a.repodata);
			clearRetry();
		}
	}, QVariant::fromValue(a));
}

void MainWindow::setRetry(std::function<void (QVariant const &var)> fn, QVariant const &var)
{
	m->retry_function = fn;
	m->retry_variant = var;
}

void MainWindow::clearRetry()
{
	m->retry_function = nullptr;
	m->retry_variant = QVariant();
}

void MainWindow::retry()
{
	if (m->retry_function) {
		m->retry_function(m->retry_variant);
	}
}

bool MainWindow::isRetryQueued() const
{
	return m->retry_function != nullptr;
}

bool MainWindow::cloneRepository(GitCloneData const &clonedata, RepositoryInfo const &repodata)
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

	CloneParams a;
	a.clonedata = clonedata;
	a.repodata = repodata;
	QVariant var = QVariant::fromValue(a);
	setRetry([this](QVariant const &var){
		CloneParams a = var.value<CloneParams>();
		clone(a);
	}, var);
	retry();

	return true;
}

/**
 * @brief MainWindow::submodule_add
 * @param url URL
 * @param local_dir ローカルディレクトリ
 *
 * サブモジュールを追加する
 */
void MainWindow::submodule_add(QString url, QString const &local_dir)
{
	if (!isOnlineMode()) return;
	if (local_dir.isEmpty()) return;

	QString dir = local_dir;

	SubmoduleAddDialog dlg(this, url, dir);
	if (dlg.exec() != QDialog::Accepted) {
		return;
	}
	url = dlg.url();
	dir = dlg.dir();
	const QString ssh_key = dlg.overridedSshKey();

	RepositoryInfo repos_item_data;
	repos_item_data.local_dir = dir;
	repos_item_data.local_dir.replace('\\', '/');
	repos_item_data.name = makeRepositoryName(dir);
	repos_item_data.ssh_key = ssh_key;

	GitCloneData data = Git::preclone(url, dir);
	bool force = dlg.isForce();

	GitRunner g = new_git_runner(local_dir, repos_item_data.ssh_key);

	std::shared_ptr<Git_submodule_add> params = std::make_shared<Git_submodule_add>(data, force);
	runPtyGit(tr("Submodule..."), g, *params, nullptr, {});
}

/**
 * @brief MainWindow::selectedCommitItem
 * @return コミットアイテム
 *
 * 選択されたコミットアイテムを返す
 */
GitCommitItem const &MainWindow::selectedCommitItem() const
{
	int i = selectedLogIndex();
	return commitItem(i);
}

/**
 * @brief MainWindow::commit
 * @param amend
 *
 * コミットする
 */
void MainWindow::commit(bool amend)
{
	ASSERT_MAIN_THREAD();

	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return;

	QList<gpg::Data> gpg_keys;
	gpg::listKeys(global->appsettings.gpg_command, &gpg_keys);

	QString message;
	QString previousMessage;

	if (amend) {
		message = commitlog()[0].message;
	} else {
		QString id = g.getCherryPicking();
		if (GitHash::isValidID(id)) {
			message = g.getMessage(id);
		} else {
			for (GitCommitItem const &item : commitlog().list) {
				if (item.commit_id.isValid()) {
					previousMessage = item.message;
					break;
				}
			}
		}
	}

	while (1) {
		GitUser user = g.getUser(GitSource::Default);
		QString sign_id = g.signingKey(GitSource::Default);
		gpg::Data key;
		for (gpg::Data const &k : gpg_keys) {
			if (k.id == sign_id) {
				key = k;
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

			PtyProcess *pty = getPtyProcess();
			if (amend || dlg.isAmend()) {
				ok = g.commit_amend_m(text, sign, pty);
			} else {
				ok = g.commit(text, sign, pty);
			}
			while (!pty->wait(1)); // wait for the process to finish

			if (ok) {
				updateStatusBarText();
				reopenRepository(true);
			} else {
				QString err = QString::fromStdString(pty->getMessage()).trimmed();
				err += "\n*** ";
				err += tr("Failed to commit");
				err += " ***\n";
				global->writeLog(err);
			}
		}
		break;
	}
}

/**
 * @brief MainWindow::commitAmend
 *
 * コミットを修正する
 */
void MainWindow::commitAmend()
{
	commit(true);
}

/**
 * @brief MainWindow::push
 * @param -uオプションを有効にする
 * @param remote リモート
 * @param branch ブランチ
 * @param force 強制
 *
 * pushする
 */
void MainWindow::push(bool set_upstream, const QString &remote, const QString &branch, bool force)
{
	if (set_upstream) {
		if (remote.isEmpty()) return;
		if (branch.isEmpty()) return;
	}

	runPtyGit(tr("Pushing..."), git(), Git_push{set_upstream, remote, branch, force}, RUN_PTY_CALLBACK{
		ASSERT_MAIN_THREAD();
		if (status->exit_code == 128) {
			if (status->error_message.find("Connection refused") != std::string::npos) {
				QMessageBox::critical(this, qApp->applicationName(), tr("Connection refused."));
				return;
			}
		}
		updateRemoteInfo();
		reopenRepository(true);
	}, {});
}

void MainWindow::fetch(GitRunner g, bool prune)
{
	runPtyGit(tr("Fetching..."), g, Git_fetch{prune}, RUN_PTY_CALLBACK{
		internalAfterFetch();
	}, {});
}

void MainWindow::stage(GitRunner g, QStringList const &paths)
{
	runPtyGit(tr("Stageing..."), g, Git_stage{paths}, RUN_PTY_CALLBACK{
		// updateFileListLater(0);
		updateCurrentFileList();
	}, {});
}

void MainWindow::fetch(GitRunner g)
{
	runPtyGit(tr("Fetching tags..."), g, Git_fetch{false}, nullptr, {});
}

void MainWindow::pull(GitRunner g)
{
	runPtyGit(tr("Pulling..."), g, Git_pull{}, RUN_PTY_CALLBACK{
		RepositoryInfo repodata = userdata.value<RepositoryInfo>();
		doReopenRepository(status, repodata);
	}, QVariant::fromValue(m->current_repository));
}

void MainWindow::push_tags(GitRunner g)
{
	runPtyGit(tr("Pushing tags..."), g, Git_push_tags{}, nullptr, {});
}

void MainWindow::delete_tags(GitRunner g, const QStringList &tagnames)
{
	runPtyGit(QString{}, g, Git_delete_tags{tagnames}, nullptr, {});
}

void MainWindow::add_tag(GitRunner g, const QString &name, GitHash const &commit_id)
{
	runPtyGit(QString{}, g, Git_add_tag{name, commit_id}, nullptr, {});
}

/**
 * @brief MainWindow::push
 * @return
 *
 * pushする
 */
bool MainWindow::push()
{
	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return false;

	QStringList remotes = g.getRemotes();

	QString current_branch = currentBranchName();

	QStringList branches;
	for (GitBranch const &b : g.branches()) {
		branches.push_back(b.name);
	}

	if (remotes.isEmpty() || branches.isEmpty()) {
		return false;
	}

	QString url;
	{
		std::vector<GitRemote> remotes;
		g.remote_v(&remotes);
		for (GitRemote const &r : remotes) {
			if (!r.url_push.isEmpty()) {
				url = r.url_push;
				break;
			}
		}
	}

	PushDialog dlg(this, url, remotes, branches, PushDialog::RemoteBranch(QString(), current_branch));
	if (dlg.exec() == QDialog::Accepted) {
		bool set_upstream = dlg.isSetUpStream();
		QString remote = dlg.remote();
		QString branch = dlg.branch();
		bool force = dlg.isForce();
		push(set_upstream, remote, branch, force);
		return true;
	}

	return false;
}

/**
 * @brief MainWindow::deleteBranch
 * @param commit コミット
 *
 * ブランチを削除する
 */
void MainWindow::deleteBranch(GitCommitItem const &commit)
{
	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return;

	QStringList all_branch_names;
	QStringList current_local_branch_names;
	{
		NamedCommitList named_commits = namedCommitItems(Branches);
		for (NamedCommitItem const &item : named_commits) {
			if (item.name == "HEAD") continue;
			if (item.id == commit.commit_id) {
				current_local_branch_names.push_back(item.name);
			}
			all_branch_names.push_back(item.name);
		}
	}

	DeleteBranchDialog dlg(this, false, all_branch_names, current_local_branch_names);
	if (dlg.exec() == QDialog::Accepted) {
		// setLogEnabled(g, true);
		QStringList names = dlg.selectedBranchNames();
		int count = 0;
		for (QString const &name : names) {
			if (g.deleteBranch(name)) {
				count++;
			} else {
				global->writeLog(tr("Failed to delete the branch '%1'").arg(name) + '\n');
			}
		}
		if (count > 0) {
			OpenRepositoryOption opt;
			opt.validate = true;
			opt.wait_cursor = true;
			opt.keep_selection = true;
			openRepository(opt);
		}
	}
}

/**
 * @brief MainWindow::deleteBranch
 *
 * ブランチを削除する
 */
void MainWindow::deleteSelectedBranch()
{
	deleteBranch(selectedCommitItem());
}

/**
 * @brief MainWindow::resetFile
 * @param paths パス
 *
 * ファイルをリセットする
 */
void MainWindow::resetFile(const QStringList &paths)
{
	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return;

	if (paths.isEmpty()) {
		// nop
	} else {
		QString cmd = "git checkout -- \"%1\"";
		cmd = cmd.arg(paths[0]);
		if (askAreYouSureYouWantToRun(tr("Reset a file"), "> " + cmd)) {
			for (QString const &path : paths) {
				g.resetFile(path);
			}
			reopenRepository(true);
		}
	}
}

/**
 * @brief MainWindow::internalDeleteTags
 * @param tagnames タグ名
 *
 * タグを削除する
 */
void MainWindow::internalDeleteTags(const QStringList &tagnames)
{
	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return;

	if (!tagnames.isEmpty()) {
		delete_tags(g, tagnames);
	}
}

/**
 * @brief MainWindow::internalAddTag
 * @param name 名前
 * @return
 *
 * タグを追加する
 */
void MainWindow::internalAddTag(const QString &name)
{
	if (name.isEmpty()) return;

	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return;

	GitHash commit_id = selectedCommitItem().commit_id;
	if (!GitHash::isValidID(commit_id)) return;

	add_tag(g, name, commit_id);
}

/**
 * @brief MainWindow::createRepository
 * @param dir ディレクトリ
 *
 * リポジトリを作成する
 */
void MainWindow::createRepository(const QString &dir)
{
	CreateRepositoryDialog dlg(this, dir);
	if (dlg.exec() == QDialog::Accepted) {
		QString path = dlg.path();
		if (QFileInfo(path).isDir()) {
			if (git().isValidWorkingCopy(path)) {
				// A valid git repository already exists there.
			} else {
				GitRunner g = new_git_runner(path, {});
				if (g.init()) {
					QString name = dlg.name();
					if (!name.isEmpty()) {
						_addExistingLocalRepository(path, name, {}, true);
					}
					QString remote_name = dlg.remoteName();
					QString remote_url = dlg.remoteURL();
					QString ssh_key = dlg.overridedSshKey();
					if (!remote_name.isEmpty() && !remote_url.isEmpty()) {
						GitRemote r;
						r.name = remote_name;
						r.set_url(remote_url);
						r.ssh_key = ssh_key;
						g.addRemoteURL(r);
						changeSshKey(path, ssh_key, true);
					}
				}
			}
		} else {
			// not dir
		}
	}
}

/**
 * @brief MainWindow::initRepository
 * @param path パス
 * @param reponame リポジトリ名
 * @param remote リモート
 *
 * リポジトリを初期化する
 */
void MainWindow::initRepository(QString const &path, QString const &reponame, GitRemote const &remote)
{
	if (QFileInfo(path).isDir()) {
		if (git().isValidWorkingCopy(path)) {
			// A valid git repository already exists there.
		} else {
			GitRunner g = new_git_runner(path, remote.ssh_key);
			if (g.init()) {
				if (!remote.name.isEmpty() && !remote.url_fetch.isEmpty()) {
					g.addRemoteURL(remote);
					changeSshKey(path, remote.ssh_key, false);
				}
				_addExistingLocalRepository(path, reponame, remote.ssh_key, true);
			}
		}
	}
}

/**
 * @brief MainWindow::addRepository
 * @param local_dir ディレクトリ
 *
 * リポジトリを追加する（クローンしたり初期化することもできる）
 */
void MainWindow::addRepository(const QString &local_dir, const QString &group)
{
	setPreferredRepositoryGroup(group);

	AddRepositoryDialog dlg(this, local_dir); // リポジトリを追加するダイアログ
	if (dlg.exec() == QDialog::Accepted) {
		addRepositoryAccepted(dlg);
	}
}

void MainWindow::addRepositoryAccepted(AddRepositoryDialog const &dlg)
{
	if (dlg.mode() == AddRepositoryDialog::Clone) {
		auto cdata = dlg.makeCloneData();
		auto rdata = dlg.repositoryInfo();
		cloneRepository(cdata, rdata);
	} else if (dlg.mode() == AddRepositoryDialog::AddExisting) {
		QString dir = dlg.localPath(false);
		addExistingLocalRepository(dir, true);
	} else if (dlg.mode() == AddRepositoryDialog::Initialize) {
		RepositoryInfo repodata = dlg.repositoryInfo();
		GitRemote r;
		r.name = dlg.remoteName();
		r.set_url(dlg.remoteURL());
		r.ssh_key = repodata.ssh_key;
		initRepository(repodata.local_dir, repodata.name, r);
	}
}

void MainWindow::scanFolderAndRegister(QString const &group)
{
	QString path = QFileDialog::getExistingDirectory(this, tr("Select a folder"), QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	if (!path.isEmpty()) {
		QStringList dirs;
		std::set<QString> existing;
		for (RepositoryInfo const &item : m->repos) {
			existing.insert(item.local_dir);
		}
		QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot);
		while (it.hasNext()) {
			it.next();
			QFileInfo info = it.fileInfo();
			QString local_dir = info.absoluteFilePath();
			if (existing.find(local_dir) == existing.end()) {
				if (git().isValidWorkingCopy(local_dir)) {
					dirs.push_back(local_dir);
				}
			}
		}
		if (dirs.isEmpty()) {
			QMessageBox::warning(this, tr("No repositories found"), tr("No repositories found in the folder."));
		} else {
			AddRepositoriesCollectivelyDialog dlg(this, dirs);
			if (dlg.exec() == QDialog::Accepted) {
				dirs = dlg.selectedDirs();
				for (QString const &dir : dirs) {
					addExistingLocalRepositoryWithGroup(dir, group);
				}
				saveRepositoryBookmarks();
				updateRepositoryList();
			}
		}
	}
}

/**
 * @brief MainWindow::doGitCommand
 * @param callback コールバック
 *
 * gitコマンドを実行する
 */
void MainWindow::doGitCommand(const std::function<void (GitRunner)> &callback)
{
	GitRunner g = git();
	if (g.isValidWorkingCopy()) {
		callback(g);
		OpenRepositoryOption opt;
		opt.validate = false;
		opt.wait_cursor = false;
		opt.keep_selection = false;
		openRepository(opt);
	}
}

/**
 * @brief MainWindow::_updateCommitLogTableView
 * @param delay_ms
 *
 * コミットログテーブルを更新する
 */
void MainWindow::_updateCommitLogTableView(int delay_ms)
{
	if (delay_ms == 0) {
		ui->tableWidget_log->viewport()->update();
	} else if (!m->update_commit_log_timer.isActive()) {
		m->update_commit_log_timer.setSingleShot(true);
		m->update_commit_log_timer.start(delay_ms);
	}
}

void MainWindow::updateCommitLogTableViewLater()
{
	_updateCommitLogTableView(300);
}

BranchLabelList MainWindow::rowLabels(int row, bool sorted) const
{
	BranchLabelList list;
	{
		auto const &map = currentRepositoryData()->label_map;
		auto it = map.find(row);
		if (it != map.end()) {
			list = it->second;
		}
	}

	if (sorted) {
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

void MainWindow::updateAvatar(const GitUser &user, bool request)
{
	m->current_git_user = user;

	QImage icon;
	if (isAvatarEnabled()) {
		icon = global->avatar_loader.fetch(user.email, request);
	}
	ui->widget_avatar_icon->setImage(icon);
}

void MainWindow::onAvatarReady()
{
	updateAvatar(currentGitUser(), false);
	updateCommitLogTableViewLater();
}

void MainWindow::onCommitDetailGetterReady()
{
	updateCommitLogTableViewLater();
}

void MainWindow::setWindowTitle_(const GitUser &user)
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

	GitRunner g = unassosiated_git_runner();
	// Git g(global->gcx(), {}, {}, {});
	GitUser user = g.getUser(GitSource::Global);
	setWindowTitle_(user);
}

void MainWindow::setCurrentRemoteName(const QString &name)
{
	m->current_remote_name = name;
}

void MainWindow::deleteTags(const GitCommitItem &commit)
{
	auto const &map = tagmap();
	auto it = map.find(commit.commit_id);
	if (it != map.end()) {
		QStringList names;
		TagList const &tags = it->second;
		for (GitTag const &tag : tags) {
			names.push_back(tag.name);
		}
		deleteTags(names);
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

/**
 * @brief MainWindow::findBranch
 * @param id
 *
 * コミットIDからブランチを検索する
 */
BranchList MainWindow::findBranch(GitHash const &id)
{
	auto it = branchmap().find(id);
	if (it != branchmap().end()) {
		return it->second;
	}
	return BranchList();
}

QString MainWindow::tempfileHeader() const
{
	QString name = "jp_soramimi_Guitar_%1_";
	return name.arg(QApplication::applicationPid());
}

/**
 * @brief MainWindow::deleteTempFiles
 *
 * 一時ファイルを削除する
 */
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

int MainWindow::limitLogCount() const
{
	const int n = appsettings()->maximum_number_of_commit_item_acquisitions;
	return (n >= 1 && n <= 100000) ? n : 10000;
}

bool MainWindow::isThereUncommitedChanges() const
{
	return m->uncommited_changes;
}

MainWindow::RepositoryTreeIndex MainWindow::repositoryTreeIndex(QTreeWidgetItem const *item) const
{
	if (item) {
		bool ok = false;
		int i = item->data(0, RepositoryTreeWidgetItem::IndexRole).toInt(&ok);
		if (ok && i >= 0 && i < repositoryList().size()) {
			RepositoryTreeIndex index;
			index.row = i;
			return index;
		}
	}
	return {};
}

std::optional<RepositoryInfo> MainWindow::repositoryItem(RepositoryTreeIndex const &index) const
{
	QList<RepositoryInfo> const &repos = repositoryList();
	if (index.row >= 0 && index.row < repos.size()) {
		return repos[index.row];
	}
	return std::nullopt;
}

std::optional<RepositoryInfo> MainWindow::selectedRepositoryItem() const
{
	QTreeWidgetItem *item = ui->treeWidget_repos->currentItem();
	RepositoryTreeIndex index = repositoryTreeIndex(item);
	return repositoryItem(index);
}

RepositoryTreeWidget::RepositoryListStyle MainWindow::repositoriesListStyle() const
{
	return ui->treeWidget_repos->currentRepositoryListStyle();
}

/**
 * @brief リポジトリリストを更新
 */
void MainWindow::updateRepositoryList(RepositoryTreeWidget::RepositoryListStyle style, int select_row, QString const &search_text)
{
	if (style == RepositoryTreeWidget::RepositoryListStyle::_Keep) {
		style = ui->treeWidget_repos->currentRepositoryListStyle();
	} else {
		{
			bool checked = (style == RepositoryTreeWidget::RepositoryListStyle::SortRecent);
			bool b = ui->action_view_sort_by_time->blockSignals(true);
			ui->action_view_sort_by_time->setChecked(checked);
			ui->action_view_sort_by_time->blockSignals(b);
		}
		ui->treeWidget_repos->setRepositoryListStyle(style); // リポジトリリストスタイルを設定
	}

	QString path = getBookmarksFilePath();
	setRepositoryList(RepositoryBookmark::load(path));
	QList<RepositoryInfo> const &repos = repositoryList();

	RepositoryTreeWidget *tree = ui->treeWidget_repos;
	tree->updateList(style, repos, search_text, select_row);
}

void MainWindow::onRepositoryTreeSortRecent(bool f)
{
	if (f) {
		updateRepositoryList(RepositoryTreeWidget::RepositoryListStyle::SortRecent);
	} else {
		clearAllFilters();
	}
}

/**
 * @brief ファイルリストを消去
 */
void MainWindow::clearFileList()
{
	ASSERT_MAIN_THREAD();
	ui->listWidget_unstaged->clear();
	ui->listWidget_staged->clear();
	ui->listWidget_files->clear();
}

/**
 * @brief 差分ビューを消去
 */
void MainWindow::clearDiffView()
{
	ASSERT_MAIN_THREAD();

	ui->widget_diff_view->clearDiffView();
}

void MainWindow::setRepositoryInfo(QString const &reponame, QString const &brname)
{
	ASSERT_MAIN_THREAD();

	ui->label_repo_name->setText(reponame);
	ui->label_branch_name->setText(brname);
}

/**
 * @brief 指定のコミットにおけるサブモジュールリストを取得
 * @param g GitRunner
 * @param id コミットID
 * @return サブモジュールリスト
 */
QList<GitSubmoduleItem> MainWindow::updateSubmodules(GitRunner g, GitHash const &id)
{
	PROFILE;
	QList<GitSubmoduleItem> submodules;
	if (!id) {
		submodules = g.submodules();
	} else {
		GitTreeItemList list;
		GitObjectCache objcache;
		// サブモジュールリストを取得する
		{
			TraceLogger trace_retrieve_submodules;
			trace_retrieve_submodules.begin("retrieve submodules", {});
			GitCommit tree;
			GitCommit::parseCommit(g, &objcache, id, &tree);
			parseGitTreeObject(g, &objcache, tree.tree_id, {}, &list);
			for (GitTreeItem const &item : list) {
				if (item.type == GitTreeItem::Type::BLOB && item.name == ".gitmodules") {
					GitObject obj = objcache.catFile(g, GitHash(item.id));
					if (!obj.content.isEmpty()) {
						parseGitSubModules(obj.content, &submodules);
					}
				}
			}
			trace_retrieve_submodules.end();
		}
		// サブモジュールに対応するIDを求める
		TraceLogger trace_retrieve_submodules;
		trace_retrieve_submodules.begin("retrieve submodules parse objects", {});
		for (int i = 0; i < submodules.size(); i++) {
			QStringList vars = submodules[i].path.split('/');
			for (int j = 0; j < vars.size(); j++) {
				for (int k = 0; k < list.size(); k++) {
					if (list[k].name == vars[j]) {
						if (list[k].type == GitTreeItem::Type::BLOB) {
							if (j + 1 == vars.size()) {
								submodules[i].id = GitHash(list[k].id);
								goto done;
							}
						} else if (list[k].type == GitTreeItem::Type::TREE) {
							GitObject obj = objcache.catFile(g, GitHash(list[k].id));
							parseGitTreeObject(obj.content, {}, &list);
							break;
						}
					}
				}
			}
done:;
		}
		trace_retrieve_submodules.end();
	}
	return submodules;
}

void MainWindow::changeRepositoryBookmarkName(RepositoryInfo item, QString new_name)
{
	item.name = new_name;
	saveRepositoryBookmark(item);
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
	setInteractionEnabled(false);
}

PtyProcess *MainWindow::getPtyProcess()
{
	return &m->pty_process;
}

PtyProcess const *MainWindow::getPtyProcess() const
{
	return &m->pty_process;
}

bool MainWindow::getPtyProcessOk() const
{
	return m->pty_process_ok;
}

bool MainWindow::isPtyProcessRunning() const
{
	return getPtyProcess()->isRunning();
}

void MainWindow::setCompletedHandler(std::function<void (bool, QVariant const &)> fn, QVariant const &userdata)
{
	m->pty_process.setCompletedHandler(fn, userdata);
}

void MainWindow::setPtyProcessOk(bool pty_process_ok)
{
	m->pty_process_ok = pty_process_ok;
}

bool MainWindow::interactionEnabled() const
{
	return m->interaction_enabled;
}

void MainWindow::setInteractionEnabled(bool enabled)
{
	m->interaction_enabled = enabled;
}

MainWindow::InteractionMode MainWindow::interactionMode() const
{
	return m->interaction_mode;
}

void MainWindow::setInteractionMode(const MainWindow::InteractionMode &im)
{
	m->interaction_mode = im;
}

void MainWindow::setUncommitedChanges(bool uncommited_changes)
{
	m->uncommited_changes = uncommited_changes;
}

QList<GitDiff> const *MainWindow::diffResult() const
{
	return &m->diff_result;
}

void MainWindow::clearLabelMap()
{
	currentRepositoryData()->label_map.clear();
}

GitHash MainWindow::getHeadId() const
{
	return m->head_id;
}

void MainWindow::setHeadId(GitHash const &head_id)
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
	return const_cast<MainWindow *>(this)->isValidWorkingCopy(currentWorkingCopyDir());
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
void MainWindow::addDiffItems(const QList<GitDiff> *diff_list, const std::function<void (ObjectData const &data)> &fn_add_item)
{
	for (int idiff = 0; idiff < diff_list->size(); idiff++) {
		GitDiff const &diff = diff_list->at(idiff);
		QString header;

		switch (diff.type) {
		case GitDiff::Type::Modify:   header = "(chg) "; break;
		case GitDiff::Type::Copy:     header = "(cpy) "; break;
		case GitDiff::Type::Rename:   header = "(ren) "; break;
		case GitDiff::Type::Create:   header = "(add) "; break;
		case GitDiff::Type::Delete:   header = "(del) "; break;
		case GitDiff::Type::ChType:   header = "(chg) "; break;
		case GitDiff::Type::Unmerged: header = "(unmerged) "; break;
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

std::map<GitHash, BranchList> const &MainWindow::branchmap() const
{
	return currentRepositoryData()->branch_map;
}

void MainWindow::updateWindowTitle(GitUser const &user)
{
	if (user) {
		setWindowTitle_(user);
	} else {
		setUnknownRepositoryInfo();
	}
}

void MainWindow::updateWindowTitle(GitRunner g)
{
	GitUser user;
	if (g.isValidWorkingCopy()) {
		user = g.getUser(GitSource::Default);
	}
	updateWindowTitle(user);
}

std::tuple<QString, BranchLabelList> MainWindow::makeCommitLabels(GitCommitItem const &commit, std::map<GitHash, BranchList> const &branch_map, std::map<GitHash, TagList> const &tag_map) const
{
	QString message_ex;
	BranchLabelList label_list;

	{ // branch
		if (commit.commit_id == getHeadId()) {
			BranchLabel label(BranchLabel::Head);
			label.text = "HEAD";
			label_list.push_back(label);
		}
		auto it = branch_map.find(commit.commit_id);
		if (it != branch_map.end() && !it->second.empty()) {
			BranchList const &list = it->second;
			for (GitBranch const &b : list) {
				if (b.flags & GitBranch::HeadDetachedAt) continue;
				if (b.flags & GitBranch::HeadDetachedFrom) continue;
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
				label_list.push_back(label);
			}
		}
	}

	{ // tag
		TagList list = findTag(tag_map, commit.commit_id);
		for (GitTag const &t : list) {
			BranchLabel label(BranchLabel::Tag);
			label.text = t.name;
			message_ex += QString(" {#%1}").arg(label.text);
			label_list.push_back(label);
		}
	}

	return {message_ex, label_list};
}

/**
 * @brief MainWindow::labelsInfoText
 * @param commit
 * @return
 *
 * コミットに関するラベル文字列を取得
 *
 * e.g. " {hoge} {fuga} {r/o/piyo}"
 */
QString MainWindow::labelsInfoText(GitCommitItem const &commit)
{
	auto tuple = makeCommitLabels(commit, m->current_repository_data.branch_map, m->current_repository_data.tag_map);
	return std::get<0>(tuple);
}

/**
 * @brief リポジトリをブックマークから消去
 * @param index 消去するリポジトリのインデックス
 * @param ask trueならユーザーに問い合わせる
 */
void MainWindow::removeRepositoryFromBookmark(RepositoryTreeIndex const &index, bool ask)
{
	if (ask) { // ユーザーに問い合わせ
		int r = QMessageBox::warning(this, tr("Confirm Remove"), tr("Are you sure you want to remove the repository from bookmarks?") + '\n' + tr("(Files will NOT be deleted)"), QMessageBox::Ok, QMessageBox::Cancel);
		if (r != QMessageBox::Ok) return;
	}
	QList<RepositoryInfo> repos = repositoryList();
	if (index.row >= 0 && index.row < repos.size()) {
		repos.erase(repos.begin() + index.row); // 消す
		setRepositoryList(std::move(repos));
		saveRepositoryBookmarks(); // 保存
		updateRepositoryList();
	}
}

/**
 * @brief リポジトリをブックマークから消去
 * @param ask trueならユーザーに問い合わせる
 */
void MainWindow::removeSelectedRepositoryFromBookmark(bool ask)
{
	auto index = repositoryTreeIndex(ui->treeWidget_repos->currentItem());
	removeRepositoryFromBookmark(index, ask);
}

/**
 * @brief コマンドプロンプトを開く
 * @param repo
 */
void MainWindow::openTerminal(const RepositoryInfo *repo)
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
void MainWindow::openExplorer(const RepositoryInfo *repo)
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

void MainWindow::saveApplicationSettings()
{
	appsettings()->saveSettings();
}



void MainWindow::setDiffResult(const QList<GitDiff> &diffs)
{
	m->diff_result = diffs;
}

const QList<GitSubmoduleItem> &MainWindow::submodules() const
{
	return m->submodules;
}

void MainWindow::setSubmodules(const QList<GitSubmoduleItem> &submodules)
{
	m->submodules = submodules;
}

bool MainWindow::runOnRepositoryDir(const std::function<void (QString, QString)> &callback, const RepositoryInfo *repo)
{
	if (!repo) {
		repo = &currentRepository();
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

NamedCommitList MainWindow::namedCommitItems(int flags)
{
	NamedCommitList items;
	if (flags & Branches) {
		for (auto const &pair : branchmap()) {
			BranchList const &list = pair.second;
			for (GitBranch const &b : list) {
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
		for (auto const &pair: tagmap()) {
			TagList const &list = pair.second;
			for (GitTag const &t : list) {
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

GitHash MainWindow::getObjectID(QListWidgetItem *item)
{
	if (!item) return {};
	return GitHash(item->data(ObjectIdRole).toString());
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
void MainWindow::setupAddFileObjectData()
{
	connect(this, &MainWindow::signalAddFileObjectData, this, &MainWindow::onAddFileObjectData);
}

void MainWindow::internalShowPanel(FileListType file_list_type)
{
	switch (file_list_type) {
	case FileListType::MessagePanel:
		ui->stackedWidget_filelist->setCurrentWidget(ui->page_message_panel);
		break;
	case FileListType::SingleList:
		ui->stackedWidget_filelist->setCurrentWidget(ui->page_files); // 1列表示
		break;
	case FileListType::SideBySide:
		ui->stackedWidget_filelist->setCurrentWidget(ui->page_uncommited); // 2列表示
		break;
	}
}

void MainWindow::onShowFileList(FileListType panel_type)
{
	ASSERT_MAIN_THREAD();

	clearDiffView();

	internalShowPanel(panel_type);
}

void MainWindow::showFileList(FileListType files_list_type)
{
	emit sigShowFileList(files_list_type);
}

void MainWindow::onAddFileObjectData(MainWindowExchangeData const &data)
{
	clearFileList();

	for (ObjectData const &obj : data.object_data) {
		QListWidgetItem *item = newListWidgetFileItem(obj);
		showFileList(data.files_list_type);
		switch (data.files_list_type) {
		case FileListType::SingleList:
			ui->listWidget_files->addItem(item);
			break;
		case FileListType::SideBySide:
			if (obj.staged) {
				ui->listWidget_staged->addItem(item);
			} else {
				ui->listWidget_unstaged->addItem(item);
			}
			break;
		default:
			break;
		}
	}
}

void MainWindow::addFileObjectData(MainWindowExchangeData const &data)
{
	emit signalAddFileObjectData(data);
}

void MainWindow::setupShowFileListHandler()
{
	connect(this, &MainWindow::sigShowFileList, this, &MainWindow::onShowFileList);
}

GitCommitItem const *MainWindow::currentCommitItem()
{
	int row = ui->tableWidget_log->actualLogIndex();
	auto const &logs = commitlog();
	if (row >= 0 && row < (int)logs.size()) {
		return &logs[row];
	}
	return nullptr;
}

void MainWindow::updateFileList(GitHash const &id)
{
	PROFILE;

	clearGitCommandCache();

	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return;

	clearFileList();

	std::future<QList<GitSubmoduleItem>> async_modules = std::async(std::launch::async, [&](){
		return updateSubmodules(g, id); // TODO: slow
	});

	FileListType file_list_type;
	if (m->background_process_work_in_progress) {
		file_list_type = FileListType::MessagePanel;
	} else {
		file_list_type = FileListType::SingleList;
		if (!id) {
			updateUncommitedChanges(g);
			if (isThereUncommitedChanges()) {
				file_list_type = FileListType::SideBySide;
			}
		}
	}

	{
		MainWindowExchangeData xdata;
		xdata.files_list_type = FileListType::SingleList;

		if (id) {
			// nop
		} else if (isThereUncommitedChanges()) {
			xdata.files_list_type = FileListType::SideBySide;
		}

		auto diffs = makeDiffs(g, id, std::move(async_modules));
		if (diffs) {
			setDiffResult(*diffs);
		} else {
			setDiffResult({});
			return;
		}

		xdata.files_list_type = file_list_type;

		if (id) {

			auto AddItem = [&](ObjectData const &obj){
				xdata.object_data.push_back(obj);
			};
			addDiffItems(diffResult(), AddItem);

		} else {

			std::map<QString, int> diffmap;

			for (int idiff = 0; idiff < diffResult()->size(); idiff++) {
				GitDiff const &diff = diffResult()->at(idiff);
				QString filename = diff.path;
				if (!filename.isEmpty()) {
					diffmap[filename] = idiff;
				}
			}

			std::atomic_size_t index {0};
			std::vector<std::thread> threads(8);
			const size_t ncount = m->uncommited_changes_file_list.size();
			std::vector<MainWindow::ObjectData> object_data(ncount);
			for (size_t j = 0; j < threads.size(); j++) {
				threads[j] = std::thread([&](GitRunner g){
					while (1) {
						size_t i = index++;
						if (i >= ncount) break;
						GitFileStatus const &s = m->uncommited_changes_file_list[i];
						{
							bool staged = (s.isStaged() && s.code_y() == ' ');
							int idiff = -1;
							QString header;
							auto it = diffmap.find(s.path1());
							GitDiff const *diff = nullptr;
							if (it != diffmap.end()) {
								idiff = it->second;
								diff = &diffResult()->at(idiff);
							}
							QString path = s.path1();
							if (s.code() == GitFileStatus::Code::Unknown) {
								qDebug() << "something wrong...";
							} else if (s.code() == GitFileStatus::Code::Untracked) {
								// nop
							} else if (s.isUnmerged()) {
								header += "(unmerged) ";
							} else if (s.code() == GitFileStatus::Code::AddedToIndex) {
								header = "(add) ";
							} else if (s.code_x() == 'D' || s.code_y() == 'D' || s.code() == GitFileStatus::Code::DeletedFromIndex) {
								header = "(del) ";
							} else if (s.code_x() == 'R' || s.code() == GitFileStatus::Code::RenamedInIndex) {
								header = "(ren) ";
								path = s.path2(); // renamed newer path
							} else if (s.code_x() == 'M' || s.code_y() == 'M') {
								header = "(chg) ";
							}
							ObjectData obj;
							obj.path = path;
							obj.header = header;
							obj.idiff = idiff;
							obj.staged = staged;
							if (diff) {
								obj.submod = diff->b_submodule.item; // TODO:
								if (obj.submod) {
									GitRunner g2 = git_for_submodule(g, obj.submod);
									auto sc = g2.queryCommitItem(obj.submod.id);
									if (sc) {
										obj.submod_commit = *sc;
									}
								}
							}
							object_data[i] = obj;
						}
					}
				}, g.dup());
			}
			for (size_t j = 0; j < threads.size(); j++) {
				threads[j].join();
			}

			xdata.object_data = std::move(object_data);
		}

		for (GitDiff const &diff : *diffResult()) {
			QString key = GitDiffManager::makeKey(diff);
			currentRepositoryData()->diff_cache[key] = diff;
		}

		addFileObjectData(xdata);
	}
}

void MainWindow::updateFileList(GitCommitItem const *commit)
{
	GitHash id;
	if (!commit) {
		// nullptr for uncommited changes
	} else if (Git::isUncommited(*commit)) {
		// empty id for uncommited changes
	} else {
		id = commit->commit_id;
	}
	updateFileList(id);
}

void MainWindow::updateCurrentFileList()
{
	ASSERT_MAIN_THREAD();

	updateFileList(currentCommitItem());
}

void MainWindow::updateFileListLater(int delay_ms)
{
	if (delay_ms == 0) {
		updateCurrentFileList();
	} else {
		m->update_file_list_timer.start(delay_ms);
	}
}

void MainWindow::cancelUpdateFileList()
{
	m->update_file_list_timer.stop();
}

void MainWindow::initUpdateFileListTimer()
{
	m->update_file_list_timer.setSingleShot(true);
	connect(&m->update_file_list_timer, &QTimer::timeout, this, &MainWindow::updateCurrentFileList);
}

void MainWindow::makeDiffList(GitHash const &id, QList<GitDiff> *diff_list, QListWidget *listwidget)
{
	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return;

	listwidget->clear();

	auto AddItem = [&](ObjectData const &data){
		QListWidgetItem *item = newListWidgetFileItem(data);
		listwidget->addItem(item);
	};

	GitDiffManager dm(getObjCache());
	*diff_list = dm.diff(g, id, submodules());
	addDiffItems(diff_list, AddItem);
}

void MainWindow::execCommitViewWindow(const GitCommitItem *commit)
{
	CommitViewWindow win(this, commit);
	win.exec();
}

/**
 * @brief MainWindow::updateCommitGraph
 *
 * 樹形図情報を構築する
 */
void MainWindow::updateCommitGraph(GitCommitItemList *logs)
{
	const int LogCount = (int)logs->size();
	if (LogCount > 0) {
		auto LogItem = [&](int i)->GitCommitItem &{ return logs->at((size_t)i); };
		enum { // 有向グラフを構築するあいだ CommitItem::marker_depth をフラグとして使用する
			UNKNOWN = 0,
			KNOWN = 1,
		};
		for (GitCommitItem &item : logs->list) {
			item.marker_depth = UNKNOWN;
		}
		// コミットハッシュを検索して、親コミットのインデックスを求める
		for (int i = 0; i < LogCount; i++) {
			GitCommitItem *item = &LogItem(i);
			item->parent_lines.clear();
			if (item->parent_ids.empty()) {
				item->resolved = true;
			} else {
				for (int j = 0; j < item->parent_ids.size(); j++) { // 親の数だけループ
					GitHash const &parent_id = item->parent_ids[j]; // 親のハッシュ値
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
					GitCommitItem *item = &LogItem((int)i);
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
					GitCommitItem *item = &LogItem(index);
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
		for (GitCommitItem &item : logs->list) {
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
						GitCommitItem *item = &LogItem(index);
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
					GitTreeLine line(next, e.depth);
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
								GitCommitItem *item = &LogItem(j);
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

void MainWindow::updateHEAD(GitRunner g)
{
	auto head = GitHash(getObjCache()->revParse(g, "HEAD"));
	setHeadId(head);
}

/**
 * @brief ネットワークを使用するコマンドの可否をUIに反映する
 * @param enabled
 */
void MainWindow::setNetworkingCommandsEnabled(bool enabled)
{
	ui->action_clone->setEnabled(enabled); // クローンコマンドの有効性を設定

	if (!git().isValidWorkingCopy(currentWorkingCopyDir())) { // 現在のディレクトリが有効なgitリポジトリなら
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
	GitRunner g = git();
	bool isopened = g.isValidWorkingCopy();

	setNetworkingCommandsEnabled(isOnlineMode());

	GitBranch b = currentBranch();
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

void MainWindow::updateStatusBarText()
{
	ASSERT_MAIN_THREAD();

	bool running = getPtyProcess()->isRunning();
	if (running) {
		setProgress(-1);
		return;
	}

	hideProgress();

	StatusInfo::Message msg;

	auto search_text = getIncrementalSearchText();
	if (!search_text.isEmpty()) {
		msg.text = tr("<div style='background: #80ffff;'>Search: <b>%1</b>&nbsp;</div>").arg(search_text.toHtmlEscaped());
		msg.format = Qt::TextFormat::RichText;
	} else {
		QWidget *w = qApp->focusWidget();
		if (w == ui->treeWidget_repos) {
			std::optional<RepositoryInfo> repo = selectedRepositoryItem();
			if (repo) {
				msg.text = QString("%1 : %2")
				.arg(repo->name)
					.arg(misc::normalizePathSeparator(repo->local_dir))
					;
			}
		} else if (w == ui->tableWidget_log) {
			GitCommitItem const *commit = currentCommitItem();
			if (commit) {
				if (Git::isUncommited(*commit)) {
					msg.text = tr("Uncommited changes");
				} else {
					QString id = commit->commit_id.toQString();
					QString labels = labelsInfoText(*commit);
					msg.text = QString("%1 : %2%3")
							   .arg(id.mid(0, 7))
							   .arg(commit->message)
							   .arg(labels)
						;
				}
			}
		}
	}

	setStatusInfo(StatusInfo::message(msg));
}

void MainWindow::mergeBranch(QString const &commit, GitMergeFastForward ff, bool squash)
{
	if (commit.isEmpty()) return;

	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return;

	g.mergeBranch(commit, ff, squash);
	reopenRepository(true);
}

void MainWindow::mergeBranch(GitCommitItem const *commit, GitMergeFastForward ff, bool squash)
{
	if (!commit) return;
	mergeBranch(commit->commit_id.toQString(), ff, squash);
}

void MainWindow::rebaseBranch(GitCommitItem const *commit)
{
	if (!commit) return;

	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return;

	QString text = tr("Are you sure you want to rebase the commit?");
	text += "\n\n";
	text += "> git rebase " + commit->commit_id.toQString();
	int r = QMessageBox::information(this, tr("Rebase"), text, QMessageBox::Ok, QMessageBox::Cancel);
	if (r == QMessageBox::Ok) {
		g.rebaseBranch(commit->commit_id.toQString());
		reopenRepository(true);
	}
}

void MainWindow::on_action_rebase_abort_triggered()
{
	doGitCommand([&](GitRunner g){
		g.rebase_abort();
	});
}

void MainWindow::cherrypick(GitCommitItem const *commit)
{
	if (!commit) return;

	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return;

	int n = commit->parent_ids.size();
	if (n == 1) {
		g.cherrypick(commit->commit_id.toQString());
	} else if (n > 1) {
		auto head = g.queryCommitItem(g.revParse("HEAD"));
		auto pick = g.queryCommitItem(commit->commit_id);
		QList<GitCommitItem> parents;
		for (int i = 0; i < n; i++) {
			QString id = commit->commit_id.toQString() + QString("^%1").arg(i + 1);
			GitHash id2 = g.revParse(id);
			auto item = g.queryCommitItem(id2);
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
			g.cherrypick(cmd);
		} else {
			return;
		}
	}

	reopenRepository(true);
}

void MainWindow::merge(GitCommitItem commit)
{
	if (isThereUncommitedChanges()) return;

	if (!commit) {
		int row = selectedLogIndex();
		commit = commitItem(row);
		if (!commit) return;
	}

	if (!GitHash::isValidID(commit.commit_id.toQString())) return;

	static const char MergeFastForward[] = "MergeFastForward";

	QString fastforward;
	{
		MySettings s;
		s.beginGroup("Behavior");
		fastforward = s.value(MergeFastForward).toString();
		s.endGroup();
	}

	std::vector<QString> newlabels;
	{
		int row = selectedLogIndex();
		BranchLabelList tmplabels = rowLabels(row);
		for (BranchLabel const &label : tmplabels) {
			if (label.kind == BranchLabel::LocalBranch || label.kind == BranchLabel::Tag) {
				newlabels.push_back(label.text);
			}
		}
		std::sort(newlabels.begin(), newlabels.end());
		newlabels.erase(std::unique(newlabels.begin(), newlabels.end()), newlabels.end());
	}

	newlabels.push_back(commit.commit_id.toQString());

	QString branch_name = currentBranchName();

	MergeDialog dlg(fastforward, newlabels, branch_name, this);
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
	GitRunner g = git();
	if (!isValidWorkingCopy(g)) {
		msgNoRepositorySelected();
		return;
	}
	QString s = g.status();
	TextEditDialog dlg(this);
	dlg.setWindowTitle(tr("Status"));
	dlg.setText(s, true);
	dlg.exec();
}

void MainWindow::on_action_commit_triggered()
{
	commit();
}

void MainWindow::on_action_fetch_triggered()
{
	reopenRepository(false);
}

void MainWindow::on_action_fetch_prune_triggered()
{
	if (!isOnlineMode()) return;

	fetch(git(), true);
}

void MainWindow::on_action_push_triggered()
{
	push();
}

void MainWindow::on_toolButton_push_clicked()
{
	ui->action_push->trigger();
}

void MainWindow::on_action_pull_triggered()
{
	if (!isOnlineMode()) return;

	pull(git());
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
	QWidget *w = qApp->focusWidget();
	if (w == ui->treeWidget_repos) {
		updateStatusBarText();
	}
}

void MainWindow::_chooseRepository(QTreeWidgetItem *item)
{
	int index = RepositoryTreeWidget::repoIndex(item);
	clearAllFilters(index);
	ui->treeWidget_repos->setFocus();
}

void MainWindow::chooseRepository()
{
	QTreeWidgetItem *item = ui->treeWidget_repos->currentItem();
	_chooseRepository(item);
}

void MainWindow::on_treeWidget_repos_itemDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
	if (ui->treeWidget_repos->isFiltered()) {
		_chooseRepository(item);
	} else {
		openSelectedRepository();
	}
}

void MainWindow::execCommitPropertyDialog(QWidget *parent, GitCommitItem const &commit)
{
	if (!commit) return;

	CommitPropertyDialog dlg(parent, commit);
	dlg.exec();
}

void MainWindow::execCommitExploreWindow(QWidget *parent, const GitCommitItem *commit)
{
	if (!commit) return;

	CommitExploreWindow win(parent, getObjCache(), commit);
	win.exec();
}

void MainWindow::execFileHistory(const QString &path)
{
	if (path.isEmpty()) return;

	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return;

	FileHistoryWindow dlg(this);
	dlg.prepare(g, path);
	dlg.exec();
}

void MainWindow::on_treeWidget_repos_customContextMenuRequested(const QPoint &pos)
{
	QTreeWidgetItem *treeitem = ui->treeWidget_repos->currentItem();
	if (!treeitem) return;

	RepositoryTreeIndex repoindex = repositoryTreeIndex(treeitem);
	std::optional<RepositoryInfo> repo = repositoryItem(repoindex);

	if (RepositoryTreeWidget::isGroupItem(treeitem)) { // group item
		QMenu menu;
		QAction *a_add_new_group = menu.addAction(tr("&Add new group"));
		QAction *a_delete_group = menu.addAction(tr("&Delete group"));
		QAction *a_rename_group = menu.addAction(tr("&Rename group"));
		menu.addSeparator();
		QAction *a_add_repository = menu.addAction(tr("&Add repository"));
		QAction *a_scan_folder_and_add = menu.addAction(tr("&Scan folder and add"));
		QPoint pt = ui->treeWidget_repos->mapToGlobal(pos);
		QAction *a = menu.exec(pt + QPoint(8, -8));
		if (a) {
			if (a == a_add_new_group) {
				QTreeWidgetItem *child = RepositoryTreeWidget::newQTreeWidgetGroupItem(tr("New group"));
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
			if (a == a_add_repository) {
				QString group = RepositoryTreeWidget::treeItemPath(treeitem);
				addRepository({}, group);
				return;
			}
			if (a == a_scan_folder_and_add) {
				QString group = RepositoryTreeWidget::treeItemPath(treeitem);
				scanFolderAndRegister(group);
				return;
			}
		}
	} else if (repo) { // repository item
		QStringList strings;
		strings.push_back(repo->name);
		strings.push_back(repo->local_dir);
		{
			std::vector<QString> urls;
			{
				std::vector<GitRemote> remotes;
				new_git_runner(repo->local_dir, {}).remote_v(&remotes);
				for (GitRemote const &r : remotes) {
					urls.push_back(r.url_fetch);
					urls.push_back(r.url_push);
				}
			}
			std::sort(urls.begin(), urls.end());
			auto it = std::unique(urls.begin(), urls.end());
			urls.resize(it - urls.begin());
			for (QString const &s : urls) {
				if (!s.isEmpty()) {
					strings.push_back(s);
				}
			}
		}

		QString open_terminal = tr("Open &terminal");
		QString open_commandprompt = tr("Open command promp&t");
		QMenu menu;
		QAction *a_open = menu.addAction(tr("&Open"));
		menu.addSeparator();

		std::map<QAction *, QString> copymap;
		QMenu *a_copy = menu.addMenu(tr("&Copy"));
		{
			for (QString const &s : strings) {
				QAction *a = a_copy->addAction(s);
				copymap[a] = s;
			}
		}
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
				openExplorer(&*repo);
				return;
			}
			if (a == a_open_terminal) {
				openTerminal(&*repo);
				return;
			}
			if (a == a_remove) {
				removeRepositoryFromBookmark(repoindex, true);
				return;
			}
			if (a == a_properties) {
				execRepositoryPropertyDialog(*repo);
				return;
			}
			auto it = copymap.find(a);
			if (it != copymap.end()) {
				QApplication::clipboard()->setText(it->second);
				return;
			}
		}
	}
}

void MainWindow::on_tableWidget_log_customContextMenuRequested(const QPoint &pos)
{
	int row = selectedLogIndex();
	const BranchLabelList labels = rowLabels(row);

	BranchLabelList local_branches;
	for (const BranchLabel &label : labels) {
		if (label.kind == BranchLabel::LocalBranch) {
			local_branches.push_back(label);
		}
	}

	GitCommitItem const &commit = commitItem(row);
	bool is_valid_commit_id = GitHash::isValidID(commit.commit_id.toQString());
	// if is_valid_commit_id == false, commit is uncommited changes.

	QMenu menu;

	QAction *a_copy_id_7letters = is_valid_commit_id ? menu.addAction(tr("Copy commit id (7 letters)")) : nullptr;
	QAction *a_copy_id_complete = is_valid_commit_id ? menu.addAction(tr("Copy commit id (completely)")) : nullptr;

	std::set<QAction *> copy_label_actions;
	{
		if (!labels.isEmpty()) {
			auto *copy_lebel_menu = menu.addMenu("Copy label");
			for (BranchLabel const &label : labels) {
				QAction *a = copy_lebel_menu->addAction(label.text);
				copy_label_actions.insert(copy_label_actions.end(), a);
			}
		}
	}

	menu.addSeparator();

	QAction *a_checkout = menu.addAction(tr("Checkout/Branch..."));
	QAction *a_checkout_this = nullptr;

	if (local_branches.size() == 1) { // このコミットのブランチが一つだけの場合
		a_checkout_this = menu.addAction(tr("Checkout branch {%1}").arg(local_branches[0].text));
	}


	menu.addSeparator();

	QAction *a_edit_message = nullptr;

	auto canEditMessage = [&](){
		if (commit.has_child) return false; // 子がないこと
		if (Git::isUncommited(commit)) return false; // 未コミットがないこと
		bool is_head = false;
		bool has_remote_branch = false;
		for (const BranchLabel &label : labels) {
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
	QAction *a_delrembranch = remoteBranches(commit.commit_id, nullptr).isEmpty() ? nullptr : menu.addAction(tr("Delete remote branch..."));

	menu.addSeparator();

	QAction *a_explore = is_valid_commit_id ? menu.addAction(tr("Explore")) : nullptr;
	QAction *a_properties = addMenuActionProperty(&menu);

	QAction *a = menu.exec(ui->tableWidget_log->viewport()->mapToGlobal(pos) + QPoint(8, -8));
	if (a) {
		if (a == a_copy_id_7letters) {
			qApp->clipboard()->setText(commit.commit_id.toQString(7));
			return;
		}
		if (a == a_copy_id_complete) {
			qApp->clipboard()->setText(commit.commit_id.toQString());
			return;
		}
		if (a == a_properties) {
			execCommitPropertyDialog(this, commit);
			return;
		}
		if (a == a_edit_message) {
			commitAmend();
			return;
		}
		if (a == a_checkout) {
			checkout(this, commit);
			return;
		}
		if (a == a_checkout_this) {
			checkoutLocalBranch(local_branches[0].text);
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
			merge(commit);
			return;
		}
		if (a == a_rebase) {
			rebaseBranch(&commit);
			return;
		}
		if (a == a_cherrypick) {
			cherrypick(&commit);
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
			execCommitExploreWindow(this, &commit);
			return;
		}
		if (copy_label_actions.find(a) != copy_label_actions.end()) {
			QString text = a->text();
			QApplication::clipboard()->setText(text);
			return;
		}
	}
}

void MainWindow::on_listWidget_files_customContextMenuRequested(const QPoint &pos)
{
	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return;

	QListWidgetItem *item = ui->listWidget_files->currentItem();

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

	QPoint pt = ui->listWidget_files->mapToGlobal(pos) + QPoint(8, -8);
	QAction *a = menu.exec(pt);
	if (a) {
		if (a == a_delete) {
			if (askAreYouSureYouWantToRun("Delete", tr("Delete selected files."))) {
				for_each_selected_files([&](QString const &path){
					g.removeFile(path, true);
				});
				reopenRepository(false);
			}
		} else if (a == a_untrack) {
			if (askAreYouSureYouWantToRun("Untrack", tr("rm --cached files"))) {
				for_each_selected_files([&](QString const &path){
					g.rm_cached(path);
				});
				reopenRepository(false);
			}
		} else if (a == a_history) {
			execFileHistory(item);
		} else if (a == a_blame) {
			blame(item);
		} else if (a == a_clean) {
			cleanSubModule(g, item);
		} else if (a == a_properties) {
			showObjectProperty(item);
		}
	}
}

void MainWindow::on_listWidget_unstaged_customContextMenuRequested(const QPoint &pos)
{
	GitRunner g = git();
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
					g.stage(path);
				});
				updateCurrentFileList();
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
							updateCurrentFileList();
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
					updateCurrentFileList();
				}
				return;
			}
			if (a == a_delete) {
				if (askAreYouSureYouWantToRun("Delete", "Delete selected files.")) {
					for_each_selected_files([&](QString const &path){
						g.removeFile(path, true);
					});
					reopenRepository(false);
				}
				return;
			}
			if (a == a_untrack) {
				if (askAreYouSureYouWantToRun("Untrack", "rm --cached")) {
					for_each_selected_files([&](QString const &path){
						g.rm_cached(path);
					});
					reopenRepository(false);
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
	GitRunner g = git();
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
					g.unstage(path);
					reopenRepository(false);
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
		qDebug() << path;
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
			// サブモジュールのときは新しいプロセスを起動する
			QString commit_id = getSubmoduleCommitId(item);
			QString path = currentWorkingCopyDir() / submodpath;
			QProcess::execute(global->this_executive_program, {path, "--commit-id", commit_id});
		} else {
			// ファイルプロパティダイアログを表示する
			QString path = currentWorkingCopyDir() / getFilePath(item);
			GitHash id = getObjectID(item);
			FilePropertyDialog dlg(this);
			dlg.exec(path, id);
		}
	}
}

void MainWindow::cleanSubModule(GitRunner g, QListWidgetItem *item)
{
	QString submodpath = getSubmodulePath(item);
	if (submodpath.isEmpty()) return;

	GitSubmoduleItem submod;
	submod.path = submodpath;
	submod.id = getObjectID(item);

	CleanSubModuleDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		auto opt = dlg.options();
		GitRunner g2 = git_for_submodule(g, submod);
		if (opt.reset_hard) {
			g2.reset_hard();
		}
		if (opt.clean_df) {
			g2.clean_df();
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

	QString path = global->appsettings.git_command;

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
	QString path = global->appsettings.ssh_command;

	auto fn = [&](QString const &path){
		setSshCommand(path, save);
	};

	QStringList list = whichCommand_(SSH_COMMAND);

	QStringList cmdlist;
	cmdlist.push_back(SSH_COMMAND);
	return selectCommand_("ssh", cmdlist, list, path, fn);
}

void MainWindow::setCurrentGitRunner(GitRunner g)
{
	m->current_repository_data.git_runner = g;
}

GitObjectCache *MainWindow::getObjCache() // TODO:
{
	return currentRepositoryData()->git_runner.getObjCache();
}

void MainWindow::clearGitCommandCache()
{
	m->unassosiated_git_runner = {};
	m->current_repository_data.git_runner.clearCommandCache();
}

void MainWindow::clearGitObjectCache()
{
	m->current_repository_data.git_runner.clearCommandCache();
}

GitRunner MainWindow::_git(const QString &dir, const QString &submodpath, const QString &sshkey) const
{
	std::shared_ptr<Git> g = std::make_shared<Git>(global->gcx(), dir, submodpath, sshkey);
	if (g->isValidGitCommand()) {
		return g;
	}

	QString text = tr("git command not specified") + '\n';
	global->writeLog(text);
	return {};
}

GitRunner MainWindow::unassosiated_git_runner() const
{
	if (!m->unassosiated_git_runner) {
		m->unassosiated_git_runner = _git({}, {}, {});
	}
	return m->unassosiated_git_runner;
}

GitRunner MainWindow::new_git_runner(const QString &dir, const QString &sshkey)
{
	return _git(dir, {}, sshkey);
}

GitRunner MainWindow::new_git_runner()
{
	RepositoryInfo const &item = currentRepository();
	return new_git_runner(item.local_dir, item.ssh_key);
}

GitRunner MainWindow::git()
{
	if (m->current_repository_data.git_runner) {
		return m->current_repository_data.git_runner;
	}
	return unassosiated_git_runner();
}

GitRunner MainWindow::git_for_submodule(GitRunner g, const GitSubmoduleItem &submod)
{
	GitRunner g2 = g.dup();
	g2.setSubmodulePath(submod.path);
	return g2;
}

bool MainWindow::isValidWorkingCopy(QString const &dir) const
{
	auto g = unassosiated_git_runner();
	return g.isValidWorkingCopy(dir);
}

GitUser MainWindow::currentGitUser() const
{
	return m->current_git_user;
}

void MainWindow::autoOpenRepository(QString dir, QString const &commit_id)
{
	auto Open = [&](RepositoryInfo const &item, QString const &commit_id){
		setCurrentRepository(item, true);
		reopenRepository(false);
		if (!commit_id.isEmpty()) {
			if (!locateCommitID(commit_id)) {
				QMessageBox::information(this, tr("Open Repository"), tr("The specified commit ID was not found."));
			}
		}
	};

	RepositoryInfo const *repo = findRegisteredRepository(&dir);
	if (repo) {
		Open(*repo, commit_id);
		return;
	}

	RepositoryInfo newitem;
	if (isValidWorkingCopy(dir)) {
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

std::optional<GitCommitItem> MainWindow::queryCommit(GitHash const &id)
{
	return git().queryCommitItem(id);
}

bool MainWindow::checkoutLocalBranch(QString const &name)
{
	if (!name.isEmpty()) {
		GitRunner g = git();
		bool ok = g.checkout(name);
		if (ok) {
			reopenRepository(true);
			return true;
		}
	}
	return false;
}

void MainWindow::checkout(QWidget *parent, GitCommitItem const &commit, std::function<void ()> accepted_callback)
{
	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return;

	QStringList tags;
	QStringList all_local_branches;
	QStringList local_branches;
	QStringList remote_branches;
	{
		NamedCommitList named_commits = namedCommitItems(Branches | Tags | Remotes);
		for (NamedCommitItem const &item : named_commits) {
			QString name = item.name;
			if (item.id == commit.commit_id) {
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
		GitHash id = commit.commit_id;
		if (!id.isValid() && !commit.parent_ids.isEmpty()) {
			id = commit.parent_ids.front();
		}
		if (op == CheckoutDialog::Operation::HeadDetached) {
			if (id.isValid()) {
				bool ok = g.checkout_detach(id.toQString());
				if (ok) {
					reopenRepository(true);
				}
			}
		} else if (op == CheckoutDialog::Operation::CreateLocalBranch) {
			if (!name.isEmpty() && id.isValid()) {
				bool ok = g.checkout(name, id.toQString());
				if (ok) {
					reopenRepository(true);
				} else {
					GitHash id = g.revParse(name);
					if (id.isValid()) {
						if (QMessageBox::question(parent, tr("Create Local Branch"),
												  tr("Failed to create a local branch.") + "\n" + tr("Do you want to jump to the existing commit?"),
												  QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
							jump(g, id);
						}
					}
				}
			}
		} else if (op == CheckoutDialog::Operation::ExistingLocalBranch) {
			checkoutLocalBranch(name);
		}
	}
}

void MainWindow::checkout()
{
	checkout(this, selectedCommitItem());
}

TextEditorThemePtr MainWindow::themeForTextEditor()
{
	return global->theme->text_editor_theme;
}

bool MainWindow::isValidWorkingCopy(GitRunner g)
{
	return g && g.isValidWorkingCopy();
}

bool MainWindow::isValidWorkingCopy(QString const &local_dir)
{
	GitRunner g = new_git_runner(local_dir, {});
	return isValidWorkingCopy(g);
}

void MainWindow::emitWriteLog(const LogData &logdata)
{
	emit sigWriteLog(logdata);
}

QString MainWindow::findFileID(GitHash const &commit_id, const QString &file)
{
	return lookupFileID(git(), getObjCache(), commit_id, file);
}

GitCommitItem const &MainWindow::commitItem(int row) const
{
	ASSERT_MAIN_THREAD();

	if (row >= 0 && row < (int)commitlog().size()) {
		return commitlog()[row];
	}
	return m->null_commit_item;
}

GitCommitItem const &MainWindow::commitItem(GitHash const &id) const
{
	ASSERT_MAIN_THREAD();
	auto *p = commitlog().find(id);
	return p ? *p : m->null_commit_item;
}

QImage MainWindow::committerIcon(int row, QSize size) const
{
	ASSERT_MAIN_THREAD();

	QImage icon;
	if (isAvatarEnabled() && isOnlineMode()) {
		GitCommitItem commit;
		auto const &logs = commitlog();
		if (row >= 0 && row < (int)logs.size()) {
			commit = logs[row];
		}
		if (commit) {
			icon = global->avatar_loader.fetch(commit.email, true); // from gavatar
			if (!size.isValid()) {
				icon = icon.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
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

	auto repos = repositoryList();
	for (int i = 0; i < repos.size(); i++) {
		RepositoryInfo *item = &(repos)[i];
		QString repodir = item->local_dir;
#ifdef Q_OS_WIN
		repodir = repodir.toLower().replace('\\', '/');
#endif
		if (locdir == repodir) {
			item->ssh_key = ssh_key;
			changed = true;
		}
	}
	setRepositoryList(std::move(repos));

	if (save && changed) {
		saveRepositoryBookmarks();
	}

	if (m->current_repository.local_dir == local_dir) {
		m->current_repository.ssh_key = ssh_key;
	}
}

QString MainWindow::abbrevCommitID(const GitCommitItem &commit)
{
	return commit.commit_id.toQString(7);
}

/**
 * @brief コミットログの選択が変化した
 */
void MainWindow::onLogCurrentItemChanged(bool update_file_list)
{
	// PROFILE;
	clearFileList();

	if (update_file_list) {
		updateFileListLater(300); // 少し待ってファイルリストを更新する
	}

	updateAncestorCommitMap();
	ui->tableWidget_log->viewport()->update();
}

void MainWindow::findNext()
{
	ASSERT_MAIN_THREAD();

	if (m->search_text.isEmpty()) {
		return;
	}

	auto const &logs = commitlog();
	for (int pass = 0; pass < 2; pass++) {
		int row = 0;
		if (pass == 0) {
			row = selectedLogIndex();
			if (row < 0) {
				row = 0;
			} else if (m->searching) {
				row++;
			}
		}
		while (row < (int)logs.size()) {
			GitCommitItem const &commit = logs[row];
			if (!Git::isUncommited(commit)) {
				if (commit.message.indexOf(m->search_text, 0, Qt::CaseInsensitive) >= 0) {
					bool b = ui->tableWidget_log->blockSignals(true);
					setCurrentLogRow(row);
					ui->tableWidget_log->blockSignals(b);
					m->searching = true;
					return;
				}
			}
			row++;
		}
	}
}

bool MainWindow::locateCommitID(QString const &commit_id)
{
	ASSERT_MAIN_THREAD();

	GitCommitItemList const &logs = commitlog();
	int row = 0;
	while (row < (int)logs.size()) {
		GitCommitItem const &commit = logs[row];
		if (!Git::isUncommited(commit)) {
			if (commit.commit_id.toQString().startsWith(commit_id)) {
				bool b = ui->tableWidget_log->blockSignals(true);
				setCurrentLogRow(row);
				ui->tableWidget_log->blockSignals(b);
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

bool MainWindow::isAncestorCommit(GitHash const &id) const
{
	auto it = m->ancestors.find(id);
	return it != m->ancestors.end();
}

/**
 * @brief 選択されたコミットを起点として、視認可能な範囲にあるすべての先祖コミットを探索し、
 *        太線描画用の先祖コミット集合を更新する。
 *
 * この関数は樹形図の描画において、現在選択されているコミットとその先祖に該当するコミットを
 * 太線で表示するための補助情報を更新する処理を行う。
 *
 * 前提として、UIスレッド上で実行される必要がある。
 */
void MainWindow::updateAncestorCommitMap()
{
	ASSERT_MAIN_THREAD(); // UIスレッド上での実行を保証

	m->ancestors.clear(); // 先祖集合を初期化

	GitCommitItemList const &logs = commitlog(); // コミットログ一覧の取得

	const int index = selectedLogIndex(); // 現在選択されている行のインデックス
	if (index < 0 || index >= logs.size()) return; // 無効なインデックスなら早期リターン

	size_t end = logs.size(); // 探索の終端をログの末尾と仮定

	// 選択行以降で、可視範囲に含まれる行のみを対象とする
	if (index < end) {
		for (size_t i = index; i < end; i++) {
			// 対象行の矩形領域を取得（列0で十分）
			QRect r = ui->tableWidget_log->visualItemRect((int)i, 0);

			// 表示領域を超えていたらそれ以降は無視
			if (r.y() >= ui->tableWidget_log->height()) {
				end = i + 1; // 表示されている範囲の最終行までに制限
				break;
			}
		}
	}

	// 現在選択されているコミットを先祖集合に追加
	m->ancestors.insert(m->ancestors.end(), logs.at(index).commit_id);

	// 選択行から可視範囲の末尾まで探索
	for (size_t i = index; i < end; i++) {
		GitCommitItem const *item = &logs.at(i);

		// このコミットがすでに先祖集合に含まれていれば、その親もすべて先祖と見なす
		if (isAncestorCommit(item->commit_id)) {
			for (GitHash const &parent : item->parent_ids) {
				// 親コミットを先祖集合に追加
				m->ancestors.insert(m->ancestors.end(), parent);
			}
		}
	}
}

void MainWindow::refresh()
{
	if (!isValidWorkingCopy(git())) return;
	reopenRepository(true);
}

void MainWindow::on_action_view_refresh_triggered()
{
	refresh();
}

void MainWindow::on_toolButton_stage_clicked()
{
	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return;

	if (m->last_focused_file_list == ui->listWidget_unstaged) {
		QList<QListWidgetItem *> items = ui->listWidget_unstaged->selectedItems();
		if (items.size() == ui->listWidget_unstaged->count()) {
			g.add_A();
		} else {
			QStringList list;
			for (QListWidgetItem *item : items) {
				QString path = getFilePath(item);
				list.push_back(path);
			}
			// stage(g, list);
			g.stage(list, nullptr);
		}
		updateCurrentFileList();
	}
}

void MainWindow::on_toolButton_unstage_clicked()
{
	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return;

	if (m->last_focused_file_list == ui->listWidget_staged) {
		QList<QListWidgetItem *> items = ui->listWidget_staged->selectedItems();
		if (items.size() == ui->listWidget_staged->count()) {
			g.unstage_all();
		} else {
			g.unstage(selectedFiles());
		}
		updateCurrentFileList();
	}
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
		updateCurrentFileList();
	}
}

int MainWindow::selectedLogIndex() const
{
	ASSERT_MAIN_THREAD();

	auto const &logs = commitlog();
	int i = ui->tableWidget_log->actualLogIndex();
	if (i >= 0 && i < (int)logs.size()) {
		return i;
	}
	return -1;
}

/**
 * @brief ファイル差分表示を更新する
 * @param item
 */
void MainWindow::updateDiffView(QListWidgetItem *item)
{
	ASSERT_MAIN_THREAD();

	clearDiffView();

	m->last_selected_file_item = item;

	if (!item) return;

	int idiff = indexOfDiff(item);
	QList<GitDiff> const *diffs = diffResult();
	if (idiff >= 0 && idiff < diffs->size()) {
		GitDiff const &diff = diffs->at(idiff);
		bool updatediffview = false;
		bool uncommited = false;
		{
			auto const &logs = commitlog();
			int row = ui->tableWidget_log->actualLogIndex();
			uncommited = (row >= 0 && row < (int)logs.size() && Git::isUncommited(logs[row]));
			updatediffview = true;
		}
		if (updatediffview) {
			ui->widget_diff_view->updateDiffView(diff, uncommited);
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
	if (c == Qt::Key_F11) {
		toggleMaximized();
		return;
	}
	if (QApplication::focusWidget() == ui->tableWidget_log && (c == Qt::Key_Return || c == Qt::Key_Enter)) {
		GitCommitItem const &commit = selectedCommitItem();
		if (commit) {
			execCommitPropertyDialog(this, commit);
		}
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

void MainWindow::on_action_add_repository_triggered()
{
	addRepository(QString());
}

void MainWindow::on_toolButton_addrepo_clicked()
{
	ui->action_add_repository->trigger();
}

void MainWindow::on_action_about_triggered()
{
	AboutDialog dlg(this);
	dlg.exec();
}

void MainWindow::on_toolButton_fetch_clicked()
{
	ui->action_fetch->trigger();
}

/**
 * @brief フィルタ文字列を取得する
 * @return
 */
QString MainWindow::getIncrementalSearchText() const
{
	return m->incremental_search_text;
}

/**
 * @brief フィルタ文字列を設定する
 * @param text
 */
void MainWindow::setFilterText(QString const &text, int repo_list_select_row)
{
	FilterTarget ft = filtertarget();

	if (m->incremental_search_text.isEmpty() && !text.isEmpty()) {
		if (ft == FilterTarget::RepositorySearch) {
		} else if (ft == FilterTarget::CommitLogSearch) {
			m->before_search_row = ui->tableWidget_log->currentRow();
		}
	}

	m->incremental_search_text = text;

	if (ft == FilterTarget::RepositorySearch) {
		updateRepositoryList(RepositoryTreeWidget::RepositoryListStyle::Standard, repo_list_select_row, m->incremental_search_text);
	} else if (ft == FilterTarget::CommitLogSearch) {
		ui->tableWidget_log->setFilter(m->incremental_search_text);
	}
}

MainWindow::FilterTarget MainWindow::filtertarget() const
{
	return m->filter_target;
}

/**
 * @brief フィルタの文字列をクリアする
 */
void MainWindow::clearFilterText(int repo_list_select_row)
{
	int commit_log_select_row = m->before_search_row;

	m->incremental_search_text = QString();
	updateRepositoryList(RepositoryTreeWidget::RepositoryListStyle::Standard, repo_list_select_row, {});
	ui->tableWidget_log->setFilter({});

	ui->tableWidget_log->setCurrentRow(commit_log_select_row);
}

/**
 * @brief すべてのフィルタの文字列をクリアし、ステータスバーを更新する
 */
void MainWindow::clearAllFilters(int select_row)
{
	if (m->incremental_search_text.isEmpty()) return;

	clearFilterText(select_row);
	updateStatusBarText();
}

bool MainWindow::applyFilter()
{
	if (getIncrementalSearchText().isEmpty()) return false;

	auto ft = filtertarget();
	if (ft == FilterTarget::RepositorySearch) {

	} else if (ft == FilterTarget::CommitLogSearch) {
		int row = ui->tableWidget_log->currentRow();
		int index = ui->tableWidget_log->unfilteredIndex(row);

		clearAllFilters();

		ui->tableWidget_log->setCurrentRow(index);
		ui->tableWidget_log->setFocus();
	}

	return true;
}

/**
 * @brief フィルタに文字を追加する
 * @return
 */
void MainWindow::_appendCharToFilterText(ushort c)
{
	QString text = getIncrementalSearchText();
	if (c == ASCII_BACKSPACE) {
		int i = text.size();
		if (i > 0) {
			text.remove(i - 1, 1);
		}
	} else if (QChar(c).isLetterOrNumber()) {
		text.append(QChar(c).toLower());
	}
	setFilterText(text);
}

bool MainWindow::appendCharToFilterText(int k, MainWindow::FilterTarget ft)
{
	m->filter_target = ft;

	if (k >= 0 && k < 128 && isalnum(k)) { // 英数字
		// thru
	} else if (k == Qt::Key_Backspace) {
		k = ASCII_BACKSPACE;
	} else {
		return false;
	}

	if (isPtyProcessRunning()) {
		// ignore but return true
	} else {
		_appendCharToFilterText(k);
		updateStatusBarText();
	}

	return true;
}

void MainWindow::deleteTags(QStringList const &tagnames)
{
	int row = ui->tableWidget_log->currentRow();

	internalDeleteTags(tagnames);

	ui->tableWidget_log->selectRow(row);
}

void MainWindow::revertCommit()
{
	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return;

	GitCommitItem const &commit = selectedCommitItem();
	if (commit) {
		g.revert(commit.commit_id);
		reopenRepository(false);
	}
}

void MainWindow::addTag(QString const &name)
{
	int row = ui->tableWidget_log->currentRow();

	internalAddTag(name);

	selectLogTableRow(row);
}

void MainWindow::on_action_push_all_tags_triggered()
{
	push_tags(git());
}

void MainWindow::on_tableWidget_log_doubleClicked(const QModelIndex &index)
{
	int i = selectedLogIndex();

	clearAllFilters();

	ui->tableWidget_log->setCurrentRow(i);

	GitCommitItem const &commit = commitItem(i);
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
	GitUser global_user;
	GitUser local_user;
	bool enable_local_user = false;

	GitRunner g = git();

	// グローバルユーザーを取得
	global_user = g.getUser(GitSource::Global);

	// ローカルユーザーを取得
	if (isValidWorkingCopy(g)) {
		local_user = g.getUser(GitSource::Local);
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
	GitRunner g = new_git_runner({}, sshkey);
	QString cmd = "ls-remote \"%1\" HEAD";
	cmd = cmd.arg(url);
	AbstractGitSession::Option opt;
	opt.chdir = false;
	opt.pty = getPtyProcess();
	bool f = (bool)g.git->exec_git(cmd, opt);
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
			if (name == "HEAD" && GitHash::isValidID(id)) {
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

bool MainWindow::jump(GitRunner g, GitHash const &id)
{
	return jumpToCommit(id);
}

void MainWindow::jump(GitRunner g, QString const &text)
{
	if (text.isEmpty()) return;

	GitHash id = g.revParse(text);
	if (!jumpToCommit(id)) {
		QMessageBox::warning(this, tr("Jump"), QString("%1\n(%2)\n\n").arg(text).arg(text) + tr("No such commit"));
	}
}

void MainWindow::on_action_repo_jump_triggered()
{
	GitRunner g = git();
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
		jump(g, text);
	}
}

void MainWindow::on_action_repo_checkout_triggered()
{
	checkout();
}

void MainWindow::on_action_delete_branch_triggered()
{
	deleteSelectedBranch();
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
	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return;

	g.reset_head1();
	reopenRepository(false);
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
	GitRunner g = git();
	Git::ReflogItemList reflog;
	g.reflog(&reflog);

	ReflogWindow dlg(this, this, reflog);
	dlg.exec();
}

void MainWindow::blame(QListWidgetItem *item)
{
	QList<BlameItem> list;
	QString path = getFilePath(item);
	{
		GitRunner g = git();
		QByteArray ba = g.blame(path);
		if (!ba.isEmpty()) {
			list = BlameWindow::parseBlame(std::string_view{ba.data(), (size_t)ba.size()});
		}
	}
	if (!list.isEmpty()) {
		GlobalSetOverrideWaitCursor();
		BlameWindow win(this, path, list);
		GlobalRestoreOverrideCursor();
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
	GitRunner g = git();
	QString global_key_id = g.signingKey(GitSource::Global);
	QString repository_key_id;
	if (g.isValidWorkingCopy()) {
		repository_key_id = g.signingKey(GitSource::Local);
	}
	SetGpgSigningDialog dlg(this, currentRepositoryName(), global_key_id, repository_key_id);
	if (dlg.exec() == QDialog::Accepted) {
		g.setSigningKey(dlg.id(), dlg.isGlobalChecked());
	}
}

void MainWindow::execAreYouSureYouWantToContinueConnectingDialog(QString const &windowtitle)
{
	using TheDlg = AreYouSureYouWantToContinueConnectingDialog;

	setInteractionMode(InteractionMode::Busy);

	GlobalRestoreOverrideCursor();

	TheDlg dlg(this);
	dlg.setLabel(windowtitle);
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
		setInteractionEnabled(false);
	}
	setInteractionMode(InteractionMode::Busy);
}

void MainWindow::deleteRemoteBranch(GitCommitItem const &commit)
{
	if (!commit) return;

	GitRunner g = git();
	if (!isValidWorkingCopy(g)) return;

	QStringList all_branches;
	QStringList remote_branches = remoteBranches(commit.commit_id, &all_branches);
	if (remote_branches.isEmpty()) return;

	DeleteBranchDialog dlg(this, true, all_branches, remote_branches);
	if (dlg.exec() == QDialog::Accepted) {
		// setLogEnabled(g, true);
		QStringList names = dlg.selectedBranchNames();
		for (QString const &name : names) {
			int i = name.indexOf('/');
			if (i > 0) {
				QString remote = name.mid(0, i);
				QString branch = ':' + name.mid(i + 1);
				push(true, remote, branch, false);
			}
		}
	}
}

QStringList MainWindow::remoteBranches(GitHash const &id, QStringList *all)
{
	if (all) all->clear();

	QStringList list;

	GitRunner g = git();
	if (isValidWorkingCopy(g)) {
		NamedCommitList named_commits = namedCommitItems(Branches | Remotes);
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

void runCommand(QString const &cmd)
{
	Process proc;
	proc.start(cmd.toStdString(), false);
	proc.wait();
}

void MainWindow::onLogIdle()
{
	if (!interactionEnabled()) return;
	if (interactionMode() != InteractionMode::None) return;

	enum PatternIndex {
		ARE_YOU_SURE_YOU_WANT_TO_CONTINUE_CONNECTING,
		ENTER_PASSPHRASE,
		ENTER_PASSPHRASE_FOR_KEY,
		FATAL_AUTHENTICATION_FAILED_FOR,
		REMOTE_HOST_IDENTIFICATION_HAS_CHANGED,
	};

	static std::map<int, QRegularExpression> patterns;
	if (patterns.empty()) {
		patterns[ARE_YOU_SURE_YOU_WANT_TO_CONTINUE_CONNECTING] = QRegularExpression(
				"Are you sure you want to continue connecting.*\\?");
		patterns[ENTER_PASSPHRASE] = QRegularExpression(
				"Enter passphrase: ");
		patterns[ENTER_PASSPHRASE_FOR_KEY] = QRegularExpression(
				"Enter passphrase for key '");
		patterns[FATAL_AUTHENTICATION_FAILED_FOR] = QRegularExpression(
				"fatal: Authentication failed for '");
		patterns[REMOTE_HOST_IDENTIFICATION_HAS_CHANGED] = QRegularExpression(
				"WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!");
	}

	std::vector<std::string> lines = getLogHistoryLines();
	clearLogHistory();

	if (lines.empty()) return;


	auto RegExp = [&](PatternIndex i){
		auto it = patterns.find(i);
		return it == patterns.end() ? QRegularExpression() : it->second;
	};

	auto Equals = [&](std::string const &line, PatternIndex i){
		std::string const &str = RegExp(i).pattern().toStdString();
		return line == str;
	};

	auto Contains = [&](PatternIndex i){
		std::string const &str = RegExp(i).pattern().toStdString();
		for (std::string const &line : lines) {
			if (strstr(line.c_str(), str.c_str())) {
				return true;
			}
		}
		return false;
	};

	auto Match = [&](std::string const &line, PatternIndex i){
		return RegExp(i).match(QString::fromStdString(line)).hasMatch();
	};

	auto StartsWith = [&](std::string const &line, PatternIndex i){
		std::string const &str = RegExp(i).pattern().toStdString();
		char const *p = str.c_str();
		char const *s = line.c_str();
		while (*p) {
			if (*s != *p) return false;
			p++;
			s++;
		}
		return true;
	};

	if (Contains(REMOTE_HOST_IDENTIFICATION_HAS_CHANGED)) {
		QString text;
		{
			for (std::string const &line : lines) {
				QString qline = QString::fromStdString(line);
				text += qline + '\n';
			}
		}
		TextEditDialog dlg(this);
		dlg.setWindowTitle(RegExp(REMOTE_HOST_IDENTIFICATION_HAS_CHANGED).pattern());
		dlg.setText(text, true);
		dlg.exec();
		return;
	}

	{
		QString dir = QString::fromStdString(parseDetectedDubiousOwnershipInRepositoryAt(lines));
		if (!dir.isEmpty()) {

			dir = misc::normalizePathSeparator(dir);

			// remove empty lines at the end and at the beginning
			while (!lines.empty() && lines.back().empty()) {
				lines.pop_back();
			}
			while (!lines.empty() && lines.front().empty()) {
				lines.erase(lines.begin());
			}

			// join lines
			QString msg;
			for (std::string const &line : lines) {
				msg += QString::fromStdString(line) + '\n';
			}

			// make command
			QString cmd = "git config --global --add safe.directory \"" + dir + "\"";

			// show dialog
			GitConfigGlobalAddSafeDirectoryDialog dlg(this);
			dlg.setMessage(msg, cmd);
			if (dlg.exec() == QDialog::Accepted) {
				// run command
				runCommand(cmd);

				if (isRetryQueued()) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1000));
					retry();
				}
			} else {
				clearRetry();
			}
			return;
		}
	}

	{
		std::string line = lines.back();
		line = misc::trimmed(line);
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

			if (Match(line, ARE_YOU_SURE_YOU_WANT_TO_CONTINUE_CONNECTING)) {
				execAreYouSureYouWantToContinueConnectingDialog(QString::fromStdString(line).trimmed());
				return;
			}

			if (Equals(line, ENTER_PASSPHRASE)) {
				ExecLineEditDialog(this, "Passphrase", QString::fromStdString(line), QString(), true);
				return;
			}

			if (StartsWith(line, ENTER_PASSPHRASE_FOR_KEY)) {
				std::string keyfile;
				{
					std::string pattern = RegExp(ENTER_PASSPHRASE_FOR_KEY).pattern().toStdString();
					char const *p = line.c_str() + pattern.size();
					char const *q = strrchr(p, ':');
					if (q && p + 2 < q && q[-1] == '\'') {
						keyfile.assign(p, q - 1);
					}
				}
				if (!keyfile.empty()) {
					if (keyfile == sshPassphraseUser() && !sshPassphrasePass().empty()) {
						std::string text = sshPassphrasePass() + '\n';
						getPtyProcess()->writeInput(text.c_str(), (int)text.size());
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

			if (StartsWith(line, FATAL_AUTHENTICATION_FAILED_FOR)) {
				QMessageBox::critical(this, tr("Authentication Failed"), QString::fromStdString(line));
				abortPtyProcess();
				return;
			}
		}
	}
}

void MainWindow::on_action_edit_tags_triggered()
{
	GitCommitItem const &commit = selectedCommitItem();
	if (commit && GitHash::isValidID(commit.commit_id.toQString())) {
		EditTagsDialog dlg(this, &commit);
		dlg.exec();
	}
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
	doGitCommand([&](GitRunner g){
		g.reset_hard();
	});
}

void MainWindow::on_action_clean_df_triggered()
{
	doGitCommand([&](GitRunner g){
		g.clean_df();
	});
}

void MainWindow::on_action_stash_triggered()
{
	doGitCommand([&](GitRunner g){
		g.stash();
	});
}

void MainWindow::on_action_stash_apply_triggered()
{
	doGitCommand([&](GitRunner g){
		g.stash_apply();
	});
}

void MainWindow::on_action_stash_drop_triggered()
{
	doGitCommand([&](GitRunner g){
		g.stash_drop();
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
	ASSERT_MAIN_THREAD();

	m->searching = false;

	if (commitlog().empty()) return;

	FindCommitDialog dlg(this, m->search_text);
	if (dlg.exec() == QDialog::Accepted) {
		m->search_text = dlg.text();
		setFocusToLogTable();
		findNext();
	}
}

void MainWindow::on_action_find_next_triggered()
{
	if (m->search_text.isEmpty()) {
		on_action_find_triggered();
	} else {
		findNext();
	}
}

void MainWindow::on_action_repo_jump_to_head_triggered()
{
	jumpToCommit("HEAD");
}

void MainWindow::on_action_repo_merge_triggered()
{
	merge();
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

void MainWindow::setShowAvatars(bool show, bool save)
{
	ApplicationSettings *as = appsettings();
	as->show_avatars = show;

	bool b = ui->action_show_avatars->blockSignals(true);
	ui->action_show_avatars->setChecked(show);
	ui->action_show_avatars->blockSignals(b);

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

bool MainWindow::isAvatarsVisible() const
{
	return appsettings()->show_avatars;
}

void MainWindow::on_action_show_labels_triggered()
{
	bool f = ui->action_show_labels->isChecked();
	setShowLabels(f, true);
	updateLogTableView();
}

void MainWindow::on_action_show_graph_triggered()
{
	bool f = ui->action_show_graph->isChecked();
	setShowGraph(f, true);
	updateLogTableView();
}

void MainWindow::on_action_show_avatars_triggered()
{
	bool f = ui->action_show_avatars->isChecked();
	setShowAvatars(f, true);
	updateLogTableView();
}

void MainWindow::on_action_submodules_triggered()
{
	GitRunner g = git();
	QList<GitSubmoduleItem> mods = g.submodules();

	std::vector<SubmodulesDialog::Submodule> mods2;
	mods2.resize((size_t)mods.size());

	for (size_t i = 0; i < (size_t)mods.size(); i++) {
		const GitSubmoduleItem mod = mods[(int)i];
		mods2[i].submodule = mod;

		GitRunner g2 = git_for_submodule(g, mod);
		auto commit = g2.queryCommitItem(mod.id);
		if (commit) {
			mods2[i].head = *commit;
		}
	}

	SubmodulesDialog dlg(this, mods2);
	dlg.exec();
}

void MainWindow::on_action_submodule_add_triggered()
{
	submodule_add({}, currentWorkingCopyDir());
}

void MainWindow::on_action_submodule_update_triggered()
{
	SubmoduleUpdateDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		GitRunner g = git();
		GitSubmoduleUpdateData data;
		data.init = dlg.isInit();
		data.recursive = dlg.isRecursive();
		g.submodule_update(data, getPtyProcess());
		refresh();
	}
}

void MainWindow::on_action_create_desktop_launcher_file_triggered()
{
	platform::createApplicationShortcut(this);
}

GitCommitItemList const &MainWindow::commitlog() const
{
	return currentRepositoryData()->commit_log;
}

void MainWindow::clearLogContents()
{
	ui->tableWidget_log->scrollToTop();
}

void MainWindow::updateLogTableView()
{
	ui->tableWidget_log->viewport()->update();
}

void MainWindow::setFocusToLogTable()
{
	ui->tableWidget_log->setFocus();
}

void MainWindow::selectLogTableRow(int row)
{
	ui->tableWidget_log->selectRow(row);
}

void MainWindow::onCommitLogCurrentRowChanged(int row)
{
	// qDebug() << __FILE__ << __LINE__;
	onLogCurrentItemChanged(true);
	updateStatusBarText(); // ステータスバー更新
	m->searching = false;
}

void MainWindow::on_action_view_sort_by_time_changed()
{
	bool f = ui->action_view_sort_by_time->isChecked();
	onRepositoryTreeSortRecent(f);
}

int genmsg()
{
	global->appsettings = ApplicationSettings::loadSettings();
	GitContext gcx;
	gcx.git_command = global->appsettings.git_command;

	QString dir = QDir::currentPath();
	std::shared_ptr<Git> g = std::make_shared<Git>(gcx, dir, QString{}, QString{});
	// std::string diff = CommitMessageGenerator::diff_head(g);
	std::string cmd = "diff --name-only HEAD";
	auto result = g->git(QString::fromStdString(cmd));
	std::vector<std::string> files = misc::splitLines(g->resultStdString(result), false);

	std::string diff;
	for (std::string const &file : files) {
		if (file.empty()) continue;
		std::string mimetype = global->determineFileType(QString::fromStdString(file));
		if (misc::starts_with(mimetype, "image/")) continue; // 画像ファイルはdiffしない
		if (mimetype == "application/octetstream") continue; // バイナリファイルはdiffしない
		if (mimetype == "application/pdf") continue; // PDFはdiffしない
		if (mimetype == "text/xml" && misc::ends_with(file, ".ts")) return false; // Do not diff Qt translation TS files (line numbers and other changes are too numerous)
		cmd = "diff --full-index HEAD -- " + file;
		auto result = g->git(QString::fromStdString(cmd));
		diff.append(g->resultStdString(result));
	}

	CommitMessageGenerator gen;
	CommitMessageGenerator::Result msg = gen.generate(diff);

	for (std::string const &line : msg.messages) {
		printf("--- %s\n", line.c_str());
	}

	return 0;
}



void MainWindow::on_action_ssh_triggered()
{
#ifdef UNSAFE_ENABLED
	if (global->isUnsafeEnabled()) {
#if 1
		RepositoryInfo item;
		item.local_dir = "/home/soramimi/develop/Guitar";
		item.name = "Guitar";
		setCurrentRepository(item, true);
		OpenRepositoryOption opt;
		opt.clear_log = true;
		opt.do_fetch = false;
		opt.keep_selection = false;
		openRepositoryMain(opt);
		return;
#else
		GitRemoteSshSession session;
		SshDialog dlg(this, session.ssh_.get());
		QString host = "192.168.0.80";
		dlg.setHostName(host);
		if (dlg.exec() == QDialog::Accepted) {
			QString path = dlg.selectedPath();
			bool yes = false;
			std::string gitcmd;
			{
				session.ssh_->add_allowed_command("which");
				auto ret = session.ssh_->exec("which git");
				if (ret) {
					gitcmd = misc::trimmed(*ret);
					// auto r = QMessageBox::question(this, tr("Confirmation"), tr("Do you agree to run %1 on %2 ?").arg(QString::fromStdString(gitcmd)).arg(host), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
					ConfirmRemoteSessionDialog dlg(this);
					QString remote_path = host + ':' + path;
					QString remote_command = host + ':' + QString::fromStdString(gitcmd);
					if (dlg.exec(remote_path, remote_command) == QMessageBox::Accepted) {
						yes = true;
					}
				}
			}
			if (yes) {
				session.ssh_->add_allowed_command(gitcmd);
				{
					std::string cmd = gitcmd + ' ' + "--version";
					auto ret = session.ssh_->exec(cmd.c_str());
					if (ret) {
						qDebug() << QString::fromStdString(*ret);
					}
				}
			}
		}
		return;
#endif
	}
#endif
	QMessageBox::warning(this, tr("SSH Connection"), tr("SSH connection is disabled"), QMessageBox::Warning, QMessageBox::Ok);
}


std::string normalize_path(char const *path);

#include <QBuffer>

void MainWindow::test()
{
}

void MainWindow::on_action_restart_trace_logger_triggered()
{
	global->close_trace_logger();
}

