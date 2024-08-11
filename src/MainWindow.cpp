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
#include "GitDiff.h"
#include "GitHubAPI.h"
#include "GitObjectManager.h"
#include "JumpDialog.h"
#include "LineEditDialog.h"
#include "MemoryReader.h"
#include "MergeDialog.h"
#include "MySettings.h"
#include "ObjectBrowserDialog.h"
#include "OverrideWaitCursor.h"
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
#include "common/misc.h"
#include "GitProcessThread.h"
#include "gunzip.h"
#include "platform.h"
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
#include <QProcess>
#include <QShortcut>
#include <QStandardPaths>
#include <QTimer>
#include <fcntl.h>
#include <sys/stat.h>

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
	// MainWindow::PtyCondition pty_condition = MainWindow::PtyCondition::None;

	bool interaction_canceled = false;
	MainWindow::InteractionMode interaction_mode = MainWindow::InteractionMode::None;

	QString repository_filter_text;
	bool uncommited_changes = false;
	Git::FileStatusList uncommited_changes_file_list;

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

	CommitDetailGetter commit_detail_getter;

	QTimer update_commit_log_timer;

	QString add_repository_into_group;

	std::thread update_files_list_thread;

	GitProcessThread git_process_thread;
};

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, m(new Private)
{
	ui->setupUi(this);
	ui->frame_progress->setVisible(false);

	setupShowFileListHandler();
	setupProgressHandler();
	setupAddFileObjectData();
	setupUpdateCommitLog();

	ui->frame_repository_wrapper->bind(this
									   , ui->tableWidget_log
									   , ui->listWidget_files
									   , ui->listWidget_unstaged
									   , ui->listWidget_staged
									   , ui->widget_diff_view
									   );

	loadApplicationSettings();
	setupExternalPrograms();

	setUnknownRepositoryInfo();

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

	frame()->filediffwidget()->init();

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

	connectPtyProcessCompleted();

	// 右上のアイコンがクリックされたとき、ConfigUserダイアログを表示
	connect(ui->widget_avatar_icon, &SimpleImageWidget::clicked, this, &MainWindow::on_action_configure_user_triggered);

	// connect(new QShortcut(QKeySequence("Ctrl+A"), this), &QShortcut::activated, this, &MainWindow::onCtrlA);
	connect(new QShortcut(QKeySequence("Ctrl+T"), this), &QShortcut::activated, this, &MainWindow::test);

	connect(&m->commit_detail_getter, &CommitDetailGetter::ready, this, &MainWindow::onCommitDetailGetterReady);

	connect(&m->update_commit_log_timer, &QTimer::timeout, [&](){
		updateCommitLogTable(0);
	});

	//

	QString path = getBookmarksFilePath();
	setRepositoryList(RepositoryBookmark::load(path));
	updateRepositoriesList();

	// アイコン取得機能
	global->avatar_loader.connectAvatarReady(this, &MainWindow::onAvatarReady);

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

	// connect(&m->git_process_thread, &GitProcessThread::done, this, &MainWindow::onGitProcessThreadDone);
	m->git_process_thread.start();

	ui->action_sidebar->setChecked(true);

	startTimers();
}

MainWindow::~MainWindow()
{
	m->git_process_thread.stop();

	if (m->update_files_list_thread.joinable()) {
		m->update_files_list_thread.join();
	}

	global->avatar_loader.disconnectAvatarReady(this, &MainWindow::onAvatarReady);

	cancelPendingUserEvents();

	stopPtyProcess();

	deleteTempFiles();

	delete m;
	delete ui;
}

RepositoryWrapperFrame *MainWindow::frame()
{
	if (!ui->frame_repository_wrapper) {
		qDebug();
	}
	return ui->frame_repository_wrapper;
}

RepositoryWrapperFrame const *MainWindow::frame() const
{
	if (!ui->frame_repository_wrapper) {
		qDebug();
	}
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
void MainWindow::postUserFunctionEvent(const std::function<void (const QVariant &, void *ptr)> fn, const QVariant &v, void *p, int ms_later)
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
				if (e->modifiers() & Qt::ControlModifier) {
					if (k == Qt::Key_A) {
						on_action_add_repository_triggered();
						return true;
					}
				} else {
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
			} else if (watched == frame()->filelistwidget() || watched == frame()->unstagedFileslistwidget() || watched == frame()->stagedFileslistwidget()) {
				if (k == Qt::Key_Escape) {
					frame()->logtablewidget()->setFocus();
					return true;
				}
			}
		}
	} else if (et == QEvent::FocusIn) {
		// ファイルリストがフォーカスを得たとき、diffビューを更新する。（コンテキストメニュー対応）
		if (watched == frame()->unstagedFileslistwidget()) {
			m->last_focused_file_list = watched;
			updateStatusBarText(frame());
			return true;
		}
		if (watched == frame()->stagedFileslistwidget()) {
			m->last_focused_file_list = watched;
			updateStatusBarText(frame());
			return true;
		}
		if (watched == frame()->filelistwidget()) {
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
	} else if (et == UserEvent(UserFunction)) {
		if (auto *e = (UserFunctionEvent *)event) {
			e->func(e->var, e->ptr);
			return true;
		}
	}
	return QMainWindow::event(event);
}

void MainWindow::customEvent(QEvent *e)
{
	if (e->type() == UserEvent(Start)) {
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

void MainWindow::toggleMaximized()
{
	auto state = windowState();
	state ^= Qt::WindowMaximized;
	setWindowState(state);
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
	m->log_history_bytes.clear();
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

void MainWindow::setupProgressHandler() const
{
	connect(this, &MainWindow::signalSetProgress, this, &MainWindow::onSetProgress);
	connect(this, &MainWindow::signalHideProgress, this, &MainWindow::onHideProgress);
	connect(this, &MainWindow::signalShowProgress, this, &MainWindow::onShowProgress);
}

void MainWindow::onSetProgress(float progress)
{
	ASSERT_MAIN_THREAD();
	ui->label_progress->setProgress(progress);
}

void MainWindow::setProgress(float progress)
{
	emit signalSetProgress(progress);
}

void MainWindow::onShowProgress(const QString &text, bool cancel_button)
{
	ASSERT_MAIN_THREAD();
	ui->toolButton_cancel->setVisible(cancel_button);
	ui->label_progress->setText(text);
	ui->label_progress->setProgress(-1.0f);
	ui->frame_progress->setVisible(true);
}

void MainWindow::showProgress(QString const &text, bool cancel_button)
{
	emit signalShowProgress(text, cancel_button);
}

void MainWindow::onHideProgress()
{
	ASSERT_MAIN_THREAD();
	ui->frame_progress->setVisible(false);
}

void MainWindow::hideProgress()
{
	emit signalHideProgress();
}

QString MainWindow::treeItemName(QTreeWidgetItem *item)
{
	return item->text(0);
}

QString MainWindow::treeItemGroup(QTreeWidgetItem *item)
{
	QString group;
	QTreeWidgetItem *p = item;
	while (1) {
		p = p->parent();
		if (!p) break;
		group = treeItemName(p) / group;
	}
	if (group.endsWith('/')) {
		group.chop(1);
	}
	group = '/' + group;
	return group;
}

void MainWindow::buildRepoTree(QString const &group, QTreeWidgetItem *item, QList<RepositoryData> *repos)
{
	QString name = treeItemName(item);
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

const QList<RepositoryData> &MainWindow::cRepositories() const
{
	return m->repos;
}

QList<RepositoryData> *MainWindow::pRepositories()
{
	return &m->repos;
}

void MainWindow::setRepositoryList(QList<RepositoryData> &&list)
{
	m->repos = std::move(list);
}

/**
 * @brief MainWindow::saveRepositoryBookmarks
 * @param update_list リポジトリリストを更新するかどうか
 *
 * リポジトリブックマークをファイルに保存する
 */
bool MainWindow::saveRepositoryBookmarks(bool update_list)
{
	QString path = getBookmarksFilePath();

	if (!RepositoryBookmark::save(path, &cRepositories())) return false;

	if (update_list) {
		updateRepositoriesList();
	}

	return true;
}

/**
 * @brief MainWindow::refrectRepositories()
 * @param repos
 *
 * リポジトリツリーの状態をリポジトリリストに反映する
 */
void MainWindow::refrectRepositories()
{
	QList<RepositoryData> newrepos;
	int n = ui->treeWidget_repos->topLevelItemCount();
	for (int i = 0; i < n; i++) {
		QTreeWidgetItem *item = ui->treeWidget_repos->topLevelItem(i);
		buildRepoTree(QString(), item, &newrepos);
	}
	setRepositoryList(std::move(newrepos));
	saveRepositoryBookmarks(false);
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
	pr->drawPixmap(x, y, w, h, m->digits, n * w, 0, w, h);
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
 * @brief MainWindow::currentWorkingCopyDir
 * @return 現在の作業ディレクトリ
 *
 * 現在の作業ディレクトリを返す
 */
QString MainWindow::currentWorkingCopyDir() const
{
	return m->current_repo.local_dir;
}

/**
 * @brief MainWindow::signatureVerificationIcon
 * @param id コミットID
 *
 * コミットの署名検証結果に応じたアイコンを返す
 */
QIcon MainWindow::signatureVerificationIcon(Git::CommitID const &id) const
{
	char c = 0;

	Git::CommitItem const commit = commitItem(frame(), id);
	if (commit) {
		if (commit.sign.verify) {
			c = commit.sign.verify;
		} else {
			auto a = m->commit_detail_getter.query(commit.commit_id, true, true);
			c = a.sign_verify;
		}

		Git::SignatureGrade sg = Git::evaluateSignature(c);
		switch (sg) {
		case Git::SignatureGrade::Good: // 署名あり、検証OK
			return m->signature_good_icon;
		case Git::SignatureGrade::Bad: // 署名あり、検証NG
			return m->signature_bad_icon;
		case Git::SignatureGrade::Unknown:
		case Git::SignatureGrade::Dubious:
		case Git::SignatureGrade::Missing:
			return m->signature_dubious_icon; // 署名あり、検証不明
		}
	} else {
		qDebug();
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
				auto c = g->queryCommitItem(submod.id);
				if (c) *commit = *c;
			}
			return &submod;
		}
	}
	return nullptr;
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

/**
 * @brief MainWindow::git_log_callback
 * @param cookie
 * @param ptr
 * @param len
 *
 * git logのコールバック関数
 */
bool MainWindow::git_log_callback(void *cookie, const char *ptr, int len)
{
	auto *mw = (MainWindow *)cookie;
	mw->emitWriteLog(QByteArray(ptr, len), false);
	return true;
}

/**
 * @brief MainWindow::revertAllFiles
 *
 * すべてのファイルをHEADに戻す
 */
void MainWindow::revertAllFiles()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QString cmd = "git reset --hard HEAD";
	if (askAreYouSureYouWantToRun(tr("Revert all files"), "> " + cmd)) {
		g->resetAllFiles();
		reopenRepository();
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
		GitPtr g = git();
		Git::User user = dlg.user();
		g->setUser(user, true);
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
void MainWindow::saveRepositoryBookmark(RepositoryData item)
{
	if (item.local_dir.isEmpty()) return;

	if (item.name.isEmpty()) {
		item.name = tr("Unnamed");
	}

	item.group = preferredRepositoryGroup();

	QList<RepositoryData> repos = cRepositories();

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
	setRepositoryList(std::move(repos));
	saveRepositoryBookmarks(true);
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
bool MainWindow::addExistingLocalRepository(QString dir, QString name, QString sshkey, bool open, bool save, bool msgbox_if_err)
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

	RepositoryData item;
	item.local_dir = dir;
	item.name = name;
	item.group = preferredRepositoryGroup();
	item.ssh_key = sshkey;
	if (save) {
		saveRepositoryBookmark(item);
	}

	if (open) {
		setCurrentRepository(item, true);
		GitPtr g = git(item.local_dir, {}, sshkey);
		internalOpenRepository(g, false, false);
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
			RepositoryData item;
			item.local_dir = info1.absoluteFilePath();
			item.name = makeRepositoryName(item.local_dir);
			item.group = group;
			m->repos.append(item);
		}
	}
}

bool MainWindow::addExistingLocalRepository(const QString &dir, bool open)
{
	return addExistingLocalRepository(dir, {}, {}, open);
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

/**
 * @brief MainWindow::execConfigUserDialog
 * @param global_user グローバルユーザー
 * @param local_user ローカルユーザー
 * @param enable_local_user ローカルユーザーを有効にする
 * @param reponame リポジトリ名
 *
 * ユーザー設定ダイアログを表示する
 */
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

/**
 * @brief MainWindow::setGitCommand
 * @param path パス
 * @param save 保存する
 *
 * gitコマンドのパスを設定する
 */
void MainWindow::setGitCommand(QString const &path, bool save)
{
	appsettings()->git_command = m->gcx.git_command = executableOrEmpty(path);

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
		GitPtr g = git();
		g->configGpgProgram(global->appsettings.gpg_command, true);
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
	appsettings()->ssh_command = m->gcx.ssh_command = executableOrEmpty(path);

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
		if (misc::isExecutable(gitCommand())) {
			return true;
		}
		if (selectGitCommand(true).isEmpty()) {
			close();
			return false;
		}
	}
}

/**
 * @brief MainWindow::saveBlobAs
 * @param frame フレーム
 * @param id ID
 * @param dstpath 保存先
 *
 * ファイルを保存する
 */
bool MainWindow::saveBlobAs(RepositoryWrapperFrame *frame, const QString &id, const QString &dstpath)
{
	Git::Object obj = internalCatFile(frame, id);
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
	writeLog(text, false);
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
	GitPtr g = git();
	QString s = g->version();
	if (!s.isEmpty()) {
		s += '\n';
		writeLog(s, true);
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
	setCurrentBranch(Git::Branch());
	m->github = GitHubRepositoryInfo();
}

/**
 * @brief MainWindow::checkUser
 *
 * ユーザーをチェックする
 */
void MainWindow::checkUser()
{
	Git g(m->gcx, {}, {}, {});
	while (1) {
		Git::User user = g.getUser(Git::Source::Global);
		if (!user.name.isEmpty() && !user.email.isEmpty()) {
			return; // ok
		}
		if (!execSetGlobalUserDialog()) { // ユーザー設定ダイアログを表示
			return;
		}
	}
}

/**
 * @brief MainWindow::openRepository
 * @param validate バリデート
 * @param waitcursor ウェイトカーソル
 * @param keep_selection 選択を保持する
 *
 * リポジトリを開く
 */
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

	internalOpenRepository(g, true, keep_selection);
}

void MainWindow::reopenRepository()
{
	openRepository(true, true, false);
}

/**
 * @brief MainWindow::setCurrentRepository
 * @param repo リポジトリ
 * @param clear_authentication 認証情報をクリアする
 *
 * 現在のリポジトリを設定する
 */
void MainWindow::setCurrentRepository(const RepositoryData &repo, bool clear_authentication)
{
	if (clear_authentication) {
		clearAuthentication();
	}
	m->current_repo = repo;
}

/**
 * @brief MainWindow::openSelectedRepository
 *
 * 選択されたリポジトリを開く
 */
void MainWindow::openSelectedRepository()
{
	RepositoryData const *repo = selectedRepositoryItem();
	if (repo) {
		setCurrentRepository(*repo, true);
		reopenRepository();
	}
}

/**
 * @brief MainWindow::makeDiffs
 * @param frame フレーム
 * @param id ID
 *
 * 差分を作成する
 */
std::optional<QList<Git::Diff>> MainWindow::makeDiffs(GitPtr g, RepositoryWrapperFrame *frame, Git::CommitID id)
{
	QList<Git::Diff> out;

	if (!isValidWorkingCopy(g)) {
		return std::nullopt;
	}

	if (!id && !isThereUncommitedChanges()) {
		id = getObjCache(frame)->revParse(g, "HEAD");
	}

	QList<Git::SubmoduleItem> mods;
	updateSubmodules(g, id, &mods);
	setSubmodules(mods);

	bool uncommited = (!id && isThereUncommitedChanges());

	GitDiff dm(getObjCache(frame));
	if (uncommited) {
		dm.diff_uncommited(g, submodules(), &out);
	} else {
		dm.diff(g, id, submodules(), &out);
	}

	return out;
}

/**
 * @brief MainWindow::updateRemoteInfo
 *
 * リモート情報を更新する
 */
void MainWindow::updateRemoteInfo(GitPtr g)
{
	queryRemotes(g);

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

/**
 * @brief MainWindow::queryRemotes
 * @param g git
 *
 * リモートを取得する
 */
void MainWindow::queryRemotes(GitPtr g)
{
	if (!g) return;
	m->remotes = g->getRemotes();
	std::sort(m->remotes.begin(), m->remotes.end());
}

void MainWindow::internalAfterFetch()
{
	ASSERT_MAIN_THREAD();

	GitPtr g = git();
	detectGitServerType(g);
	queryCommitLog(frame(), g);
	updateRemoteInfo(g);
}

#define RUN_PTY_CALLBACK [this](ProcessStatus const &status, QVariant const &userdata)
void MainWindow::runPtyGit(GitPtr g, std::shared_ptr<AbstractGitCommandItem> params, std::function<void (ProcessStatus const &status, QVariant const &userdata)> callback, QVariant const &userdata)
{
	params->g = g->dup();

	QApplication::setOverrideCursor(Qt::WaitCursor);
	setProgress(-1.0f);
	showProgress(params->progress_message, false);

	GitProcessRequest req;
	req.run = [this](GitProcessRequest const &req){
		setCompletedHandler([this](bool ok, const QVariant &d){
			hideProgress();
			GitProcessRequest const &req = d.value<GitProcessRequest>();
			PtyProcessCompleted data;
			data.status.ok = ok;
			data.callback = req.callback;
			data.userdata = req.userdata;
			emit sigPtyProcessCompleted(true, data);
		}, QVariant::fromValue(req));
		setPtyProcessOk(true);
		req.params->pty = getPtyProcess();

		if (req.params->run()) {
			// nop
		} else {
			emit sigPtyProcessCompleted(false, {});
		}
	};
	req.params = params;
	req.callback = callback;
	req.userdata = userdata;
	m->git_process_thread.request(std::move(req));
}

void MainWindow::onPtyProcessCompleted(bool ok, PtyProcessCompleted const &data)
{
	ASSERT_MAIN_THREAD();

	if (ok) {
		if (data.callback) {
			data.callback(data.status, data.userdata);
		}
	}

	QApplication::restoreOverrideCursor();
}

void MainWindow::connectPtyProcessCompleted() const
{
	connect(this, &MainWindow::sigPtyProcessCompleted, this, &MainWindow::onPtyProcessCompleted);
}

void MainWindow::doReopenRepository(ProcessStatus const &status, QVariant const &userdata)
{
	ASSERT_MAIN_THREAD();

	if (status.ok) {
		RepositoryData const &r = userdata.value<RepositoryData>();
		saveRepositoryBookmark(r);
		setCurrentRepository(r, false);
		reopenRepository();
	}
}

/**
 * @brief MainWindow::cloneRepository
 * @param clonedata クローンデータ
 * @param repodata リポジトリデータ
 * @return
 *
 * リポジトリをクローンする
 */
void MainWindow::clone(GitPtr g, Git::CloneData const &clonedata, RepositoryData const &repodata)
{
	std::shared_ptr<GitCommandItem_clone> params = std::make_shared<GitCommandItem_clone>(tr("Cloning..."), clonedata);
	runPtyGit(g, params, RUN_PTY_CALLBACK{
		doReopenRepository(status, userdata);
	}, QVariant::fromValue(repodata));
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
	clone(g, clonedata, repodata);

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
	
	std::shared_ptr<GitCommandItem_submodule_add> params = std::make_shared<GitCommandItem_submodule_add>(tr("Submodule..."), data, force);
	runPtyGit(g, params, nullptr, {});
}

/**
 * @brief MainWindow::selectedCommitItem
 * @param frame フレーム
 * @return コミットアイテム
 *
 * 選択されたコミットアイテムを返す
 */
Git::CommitItem MainWindow::selectedCommitItem(RepositoryWrapperFrame *frame) const
{
	int i = selectedLogIndex(frame);
	return commitItem(frame, i);
}

//
void MainWindow::setupUpdateCommitLog()
{
	connect(this, &MainWindow::signalUpdateCommitLog, this, &MainWindow::onUpdateCommitLog);
}

void MainWindow::onUpdateCommitLog()
{
	openRepositoryMain(frame(), git(), false, false, false, true);
}

void MainWindow::updateCommitLog()
{
	emit signalUpdateCommitLog();
}


/**
 * @brief MainWindow::commit
 * @param frame フレーム
 * @param amend
 *
 * コミットする
 */
void MainWindow::commit(RepositoryWrapperFrame *frame, bool amend)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QList<gpg::Data> gpg_keys;
	gpg::listKeys(global->appsettings.gpg_command, &gpg_keys);

	QString message;
	QString previousMessage;

	{
		std::lock_guard lock(frame->commit_log_mutex);

		if (amend) {
			message = frame->commit_log[0].message;
		} else {
			QString id = g->getCherryPicking();
			if (Git::isValidID(id)) {
				message = g->getMessage(id);
			} else {
				for (Git::CommitItem const &item : frame->commit_log.list) {
					if (item.commit_id.isValid()) {
						previousMessage = item.message;
						break;
					}
				}
			}
		}
	}

	while (1) {
		Git::User user = g->getUser(Git::Source::Default);
		QString sign_id = g->signingKey(Git::Source::Default);
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
				ok = g->commit_amend_m(text, sign, pty);
			} else {
				ok = g->commit(text, sign, pty);
			}
			while (!pty->wait(1)); // wait for the process to finish

			if (ok) {
				setForceFetch(true);
				updateStatusBarText(frame);
				reopenRepository();
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

/**
 * @brief MainWindow::commitAmend
 * @param frame フレーム
 *
 * コミットを修正する
 */
void MainWindow::commitAmend(RepositoryWrapperFrame *frame)
{
	commit(frame, true);
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

	auto params = GitCommandItem_push::make(tr("Pushing..."), set_upstream, remote, branch, force);
	runPtyGit(git(), params, RUN_PTY_CALLBACK{
		ASSERT_MAIN_THREAD();
		if (status.exit_code == 128) {
			if (status.error_message.indexOf("Connection refused") >= 0) {
				QMessageBox::critical(this, qApp->applicationName(), tr("Connection refused."));
				return;
			}
		}
		updateRemoteInfo(git());
		reopenRepository();
	}, {});
}

void MainWindow::fetch(GitPtr g, bool prune)
{
	auto params = GitCommandItem_fetch::make(tr("Fetching..."), prune);
	runPtyGit(g, params, RUN_PTY_CALLBACK{
		global->mainwindow->internalAfterFetch();
	}, {});
}

void MainWindow::stage(GitPtr g, QStringList const &paths)
{
	auto params = GitCommandItem_stage::make(tr("Stageing..."), paths);
	runPtyGit(g, params, nullptr, {});
}

void MainWindow::fetch_tags_f(GitPtr g)
{
	auto params = GitCommandItem_fetch_tags_f::make(tr("Fetching tags..."));
	runPtyGit(g, params, nullptr, {});
}

void MainWindow::pull(GitPtr g)
{
	auto params = GitCommandItem_pull::make(tr("Pulling..."));
	runPtyGit(g, params, RUN_PTY_CALLBACK{
		doReopenRepository(status, userdata);
	}, QVariant::fromValue(m->current_repo));
}

void MainWindow::push_tags(GitPtr g)
{
	auto params = GitCommandItem_push_tags::make(tr("Pushing tags..."));
	runPtyGit(g, params, nullptr, {});
}

void MainWindow::delete_tags(GitPtr g, const QStringList &tagnames)
{
	auto params = std::make_shared<GitCommandItem_delete_tags>(QString{}, tagnames);
	runPtyGit(g, params, nullptr, {});
}

void MainWindow::add_tag(GitPtr g, const QString &name, Git::CommitID const &commit_id)
{
	auto params = GitCommandItem_add_tag::make({}, name, commit_id);
	runPtyGit(g, params, nullptr, {});
}

/**
 * @brief MainWindow::push
 * @return
 *
 * pushする
 */
bool MainWindow::push()
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
		push(set_upstream, remote, branch, force);
		return true;
	}

	return false;
}

/**
 * @brief MainWindow::deleteBranch
 * @param frame フレーム
 * @param commit コミット
 *
 * ブランチを削除する
 */
void MainWindow::deleteBranch(RepositoryWrapperFrame *frame, Git::CommitItem const &commit)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QStringList all_branch_names;
	QStringList current_local_branch_names;
	{
		NamedCommitList named_commits = namedCommitItems(frame, Branches);
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

/**
 * @brief MainWindow::deleteBranch
 * @param frame フレーム
 *
 * ブランチを削除する
 */
void MainWindow::deleteSelectedBranch(RepositoryWrapperFrame *frame)
{
	deleteBranch(frame, selectedCommitItem(frame));
}

/**
 * @brief MainWindow::resetFile
 * @param paths パス
 *
 * ファイルをリセットする
 */
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
			reopenRepository();
		}
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
 * @brief MainWindow::internalDeleteTags
 * @param tagnames タグ名
 *
 * タグを削除する
 */
void MainWindow::internalDeleteTags(const QStringList &tagnames)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	if (!tagnames.isEmpty()) {
		delete_tags(g, tagnames);
	}
}

/**
 * @brief MainWindow::internalAddTag
 * @param frame フレーム
 * @param name 名前
 * @return
 *
 * タグを追加する
 */
void MainWindow::internalAddTag(RepositoryWrapperFrame *frame, const QString &name)
{
	if (name.isEmpty()) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	Git::CommitID commit_id = selectedCommitItem(frame).commit_id;
	if (!Git::isValidID(commit_id)) return;

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
						r.set_url(remote_url);
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

/**
 * @brief MainWindow::initRepository
 * @param path パス
 * @param reponame リポジトリ名
 * @param remote リモート
 *
 * リポジトリを初期化する
 */
void MainWindow::initRepository(QString const &path, QString const &reponame, Git::Remote const &remote)
{
	if (QFileInfo(path).isDir()) {
		if (Git::isValidWorkingCopy(path)) {
			// A valid git repository already exists there.
		} else {
			GitPtr g = git(path, {}, remote.ssh_key);
			if (g->init()) {
				if (!remote.name.isEmpty() && !remote.url_fetch.isEmpty()) {
					g->addRemoteURL(remote);
					changeSshKey(path, remote.ssh_key, false);
				}
				addExistingLocalRepository(path, reponame, remote.ssh_key, true);
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
		if (dlg.mode() == AddRepositoryDialog::Clone) { // クローン
			auto cdata = dlg.makeCloneData();
			auto rdata = dlg.makeRepositoryData();
			cloneRepository(cdata, rdata);
		} else if (dlg.mode() == AddRepositoryDialog::AddExisting) { // 既存のリポジトリを追加
			QString dir = dlg.localPath(false);
			addExistingLocalRepository(dir, true);
		} else if (dlg.mode() == AddRepositoryDialog::Initialize) { // リポジトリを初期化する
			QString dir = dlg.localPath(false);
			QString name = dlg.repositoryName();
			Git::Remote r;
			r.name = dlg.remoteName();
			r.set_url(dlg.remoteURL());
			r.ssh_key = dlg.overridedSshKey();
			initRepository(dir, name, r);
		}
	}
}

void MainWindow::scanFolderAndRegister(QString const &group)
{
	QString path = QFileDialog::getExistingDirectory(this, tr("Select a folder"), QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	if (!path.isEmpty()) {
		QStringList dirs;
		std::set<QString> existing;
		for (RepositoryData const &item : m->repos) {
			existing.insert(item.local_dir);
		}
		QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot);
		while (it.hasNext()) {
			it.next();
			QFileInfo info = it.fileInfo();
			QString local_dir = info.absoluteFilePath();
			if (existing.find(local_dir) == existing.end()) {
				if (Git::isValidWorkingCopy(local_dir)) {
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
				saveRepositoryBookmarks(true);
			}
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

/**
 * @brief MainWindow::doGitCommand
 * @param callback コールバック
 *
 * gitコマンドを実行する
 */
void MainWindow::doGitCommand(const std::function<void (GitPtr)> &callback)
{
	GitPtr g = git();
	if (isValidWorkingCopy(g)) {
		OverrideWaitCursor;
		callback(g);
		openRepository(false, false, false);
	}
}

/**
 * @brief MainWindow::updateCommitLogTable
 * @param frame
 * @param delay_ms
 *
 * コミットログテーブルを更新する
 */
void MainWindow::updateCommitLogTable(RepositoryWrapperFrame *frame, int delay_ms)
{
	if (delay_ms == 0) {
		frame->logtablewidget()->viewport()->update();
	}
	if (!m->update_commit_log_timer.isActive()) {
		m->update_commit_log_timer.setSingleShot(true);
		m->update_commit_log_timer.start(delay_ms);
	}
}

void MainWindow::updateCommitLogTable(int delay_ms)
{
	updateCommitLogTable(frame(), delay_ms);
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

void MainWindow::onAvatarReady()
{
	updateAvatar(currentGitUser(), false);
	updateCommitLogTable(300);
}

void MainWindow::onCommitDetailGetterReady()
{
	updateCommitLogTable(300);
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
	auto it = ptrCommitToTagMap(frame)->find(commit.commit_id);
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

/**
 * @brief MainWindow::findBranch
 * @param frame
 * @param id
 *
 * コミットIDからブランチを検索する
 */
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
	return getObjCache(frame)->getCommitIdFromTag(git(), tag);
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

	setRepositoryList(RepositoryBookmark::load(path));
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
 * @brief ファイルリストを消去
 * @param frame
 */
void MainWindow::clearFileList(RepositoryWrapperFrame *frame)
{
	ASSERT_MAIN_THREAD();
	frame->unstagedFileslistwidget()->clear();
	frame->stagedFileslistwidget()->clear();
	frame->filelistwidget()->clear();
}

/**
 * @brief 差分ビューを消去
 * @param frame
 */
void MainWindow::clearDiffView(RepositoryWrapperFrame *frame)
{
	std::lock_guard lock(frame->commit_log_mutex);
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
void MainWindow::updateSubmodules(GitPtr g, Git::CommitID const &id, QList<Git::SubmoduleItem> *out)
{
	*out = {};
	QList<Git::SubmoduleItem> submodules;
	if (!id) {
		submodules = g->submodules();
	} else {
		GitTreeItemList list;
		GitObjectCache objcache;
		objcache.setup(g);
		// サブモジュールリストを取得する
		{
			GitCommit tree;
			GitCommit::parseCommit(g, &objcache, id, &tree);
			parseGitTreeObject(g, &objcache, tree.tree_id, {}, &list);
			for (GitTreeItem const &item : list) {
				if (item.type == GitTreeItem::Type::BLOB && item.name == ".gitmodules") {
					Git::Object obj = objcache.catFile(g, Git::CommitID(item.id));
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
								submodules[i].id = Git::CommitID(list[k].id);
								goto done;
							}
						} else if (list[k].type == GitTreeItem::Type::TREE) {
							Git::Object obj = objcache.catFile(g, Git::CommitID(list[k].id));
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

void MainWindow::changeRepositoryBookmarkName(RepositoryData item, QString new_name)
{
	item.name = new_name;
	saveRepositoryBookmark(item);
}

int MainWindow::rowFromCommitId(RepositoryWrapperFrame *frame, Git::CommitID const &id)
{
	std::lock_guard lock(frame->commit_log_mutex);

	auto const &logs = frame->commit_log;
	for (size_t i = 0; i < logs.size(); i++) {
		Git::CommitItem const &item = logs[i];
		if (item.commit_id == id) {
			return (int)i;
		}
	}
	return -1;
}

QList<Git::Tag> MainWindow::findTag(RepositoryWrapperFrame *frame, Git::CommitID const &id)
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
 * @brief MainWindow::updateCommitGraph
 * @param frame
 *
 * 樹形図情報を構築する
 */
void MainWindow::updateCommitGraph(Git::CommitItemList *logs)
{
	const int LogCount = (int)logs->size();
	if (LogCount > 0) {
		auto LogItem = [&](int i)->Git::CommitItem &{ return logs->at((size_t)i); };
		enum { // 有向グラフを構築するあいだ CommitItem::marker_depth をフラグとして使用する
			UNKNOWN = 0,
			KNOWN = 1,
		};
		for (Git::CommitItem &item : logs->list) {
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
		for (Git::CommitItem &item : logs->list) {
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

void MainWindow::updateCommitGraph(RepositoryWrapperFrame *frame)
{
	std::lock_guard lock(frame->commit_log_mutex);
	updateCommitGraph(&frame->commit_log);
}

/**
 * @brief MainWindow::queryCommitLog
 * @param p
 * @param g
 *
 * コミットログとブランチ情報を取得
 */
void MainWindow::queryCommitLog(RepositoryWrapperFrame *frame, GitPtr g)
{
	auto Do = [this, g](BasicCommitLog *p){

		p->commit_log = retrieveCommitLog(g); // コミットログを取得

		// Uncommited changes がある場合、その親を取得するためにブランチ情報が必要
		QList<Git::Branch> branches = g->branches(); // ブランチを取得
		for (Git::Branch const &b : branches) {
			if (b.isCurrent()) {
				setCurrentBranch(b);
			}
			p->branch_map[b.id].append(b);
		}

		// Uncommited changes の処理
		updateUncommitedChanges();
		if (isThereUncommitedChanges()) {
			Git::CommitItem item;
			item.parent_ids.push_back(currentBranch().id);
			item.message = tr("Uncommited changes");
			p->commit_log.list.insert(p->commit_log.list.begin(), item);
			p->commit_log.updateIndex();
		}

		updateCommitGraph(&p->commit_log);
	};

	BasicCommitLog log;
	Do(&log);
	{
		std::lock_guard lock(frame->commit_log_mutex);
		frame->commit_log = log.commit_log;
		frame->branch_map = log.branch_map;
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

PtyProcess *MainWindow::getPtyProcess()
{
	return &m->pty_process;
}

bool MainWindow::getPtyProcessOk() const
{
	return m->pty_process_ok;
}

// MainWindow::PtyCondition MainWindow::getPtyCondition()
// {
// 	return m->pty_condition;
// }

void MainWindow::setCompletedHandler(std::function<void (bool, const QVariant &)> fn, QVariant const &userdata)
{
	m->pty_process.setCompletedHandler(fn, userdata);
}

void MainWindow::setPtyProcessOk(bool pty_process_ok)
{
	m->pty_process_ok = pty_process_ok;
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
Git::CommitItemList MainWindow::retrieveCommitLog(GitPtr g) const
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
					if (parent == list[j].commit_id) {
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
			list.list.erase(list.list.begin() + (int)i);
			list.list.insert(list.list.begin() + (int)newpos, t);
			i = newpos;
			limit--;
		}
		i++;
	}

	list.updateIndex();
	return list;
}

std::map<Git::CommitID, QList<Git::Branch>> &MainWindow::commitToBranchMapRef(RepositoryWrapperFrame *frame)
{
	return frame->branch_map;
}

void MainWindow::updateWindowTitle(Git::User const &user)
{
	if (user) {
		setWindowTitle_(user);
	} else {
		setUnknownRepositoryInfo();
	}
}

void MainWindow::updateWindowTitle(GitPtr g)
{
	Git::User user;
	if (isValidWorkingCopy(g)) {
		user = g->getUser(Git::Source::Default);
	}
	updateWindowTitle(user);
}

/**
 * @brief コミット情報のテキストを作成
 * @param frame
 * @param row
 * @param label_list
 * @return
 */
QString MainWindow::makeCommitInfoText(RepositoryWrapperFrame *frame, int row, QList<BranchLabel> *label_list, bool lock)
{
	if (lock) {
		std::lock_guard lock(frame->commit_log_mutex);
		return makeCommitInfoText(frame, row, label_list, false);
	}

	QString message_ex;

	Git::CommitItem commit = frame->commit_log[row];

	{ // branch
		if (label_list) {
			if (commit.commit_id == getHeadId()) {
				BranchLabel label(BranchLabel::Head);
				label.text = "HEAD";
				label_list->push_back(label);
			}
		}
		QList<Git::Branch> list = findBranch(frame, commit.commit_id);
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
		QList<Git::Tag> list = findTag(frame, commit.commit_id);
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
		saveRepositoryBookmarks(true); // 保存
	}
	auto list = *repos;
	setRepositoryList(std::move(list));
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
	return GitObjectManager::findObject(id, m->current_repo.local_dir);
}

void MainWindow::writeLog(const char *ptr, int len, bool record)
{
	internalWriteLog(ptr, len, record);
}

void MainWindow::writeLog(const std::string_view &str, bool record)
{
	internalWriteLog(str.data(), (int)str.size(), record);
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
	appsettings()->saveSettings();
}

void MainWindow::loadApplicationSettings()
{
	*appsettings() = ApplicationSettings::loadSettings();
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

Git::CommitID MainWindow::getObjectID(QListWidgetItem *item)
{
	if (!item) return {};
	return Git::CommitID(item->data(ObjectIdRole).toString());
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

void MainWindow::updateUncommitedChanges()
{
	m->uncommited_changes_file_list = git()->status_s();
	setUncommitedChanges(!m->uncommited_changes_file_list.empty());
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

void MainWindow::onAddFileObjectData(ExchangeData const &data)
{
	clearFileList(data.frame);

	for (ObjectData const &obj : data.object_data) {
		QListWidgetItem *item = newListWidgetFileItem(obj);
		switch (data.files_list_type) {
		case FilesListType::SingleList:
			data.frame->filelistwidget()->addItem(item);
			break;
		case FilesListType::SideBySide:
			if (obj.staged) {
				data.frame->stagedFileslistwidget()->addItem(item);
			} else {
				data.frame->unstagedFileslistwidget()->addItem(item);
			}
			break;
		}
	}
}

void MainWindow::addFileObjectData(ExchangeData const &data)
{
	emit signalAddFileObjectData(data);
}

void MainWindow::setupShowFileListHandler()
{
	connect(this, &MainWindow::signalShowFileList, this, &MainWindow::onShowFileList);
}

void MainWindow::onShowFileList(FilesListType files_list_type)
{
	ASSERT_MAIN_THREAD();

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

void MainWindow::showFileList(FilesListType files_list_type)
{
	emit signalShowFileList(files_list_type);
}


void MainWindow::updateFileList(RepositoryWrapperFrame *frame, Git::CommitID const &id)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	clearFileList(frame);

	if (m->update_files_list_thread.joinable()) {
		m->update_files_list_thread.join();
	}

	FilesListType files_list_type = FilesListType::SingleList;
	if (!id) {
		updateUncommitedChanges();
		if (isThereUncommitedChanges()) {
			files_list_type = FilesListType::SideBySide;
		}
	}
	showFileList(files_list_type);
	
	{

		ExchangeData xdata;
		xdata.frame = frame;
		xdata.files_list_type = FilesListType::SingleList;

		if (id) {
			auto diffs = makeDiffs(g, frame, Git::CommitID(id));
			if (diffs) {
				setDiffResult(*diffs);
			} else {
				setDiffResult({});
				return;
			}
			showFileList(xdata.files_list_type);
			xdata.frame = frame;
			xdata.files_list_type = xdata.files_list_type;

			auto AddItem = [&](ObjectData const &obj){
				xdata.object_data.push_back(obj);
			};
			addDiffItems(diffResult(), AddItem);

		} else { // Uncommited changes が選択されているとき

			bool uncommited = isThereUncommitedChanges();
			if (uncommited) {
				xdata.files_list_type = FilesListType::SideBySide;
			}
			auto diffs = makeDiffs(g, frame, uncommited ? Git::CommitID() : id);
			if (diffs) {
				setDiffResult(*diffs);
			} else {
				setDiffResult({});
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

			for (Git::FileStatus const &s : m->uncommited_changes_file_list) {
				bool staged = (s.isStaged() && s.code_y() == ' ');
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
				ObjectData obj;
				obj.path = path;
				obj.header = header;
				obj.idiff = idiff;
				obj.staged = staged;
				if (diff) {
					obj.submod = diff->b_submodule.item; // TODO:
					if (obj.submod) {
						GitPtr g = git(obj.submod);
						auto sc = g->queryCommitItem(obj.submod.id);
						if (sc) {
							obj.submod_commit = *sc;
						}
					}
				}
				xdata.frame = frame;
				xdata.files_list_type = xdata.files_list_type;
				xdata.object_data.push_back(obj);
			}
		}

		addFileObjectData(xdata);

		for (Git::Diff const &diff : *diffResult()) {
			QString key = GitDiff::makeKey(diff);
			frame->diff_cache[key] = diff;
		}
	}
}

/**
 * @brief ファイルリストを更新
 * @param id
 * @param diff_list
 * @param listwidget
 */
void MainWindow::updateFileList2(RepositoryWrapperFrame *frame, Git::CommitID const &id, QList<Git::Diff> *diff_list, QListWidget *listwidget)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	listwidget->clear();

	auto AddItem = [&](ObjectData const &data){
		QListWidgetItem *item = newListWidgetFileItem(data);
		listwidget->addItem(item);
	};

	GitDiff dm(getObjCache(frame));
	if (!dm.diff(git(), id, submodules(), diff_list)) return;

	addDiffItems(diff_list, AddItem);
}

void MainWindow::execCommitViewWindow(const Git::CommitItem *commit)
{
	CommitViewWindow win(this, commit);
	win.exec();
}

void MainWindow::updateFileList(RepositoryWrapperFrame *frame, Git::CommitItem const &commit)
{
	Git::CommitID id;
	if (Git::isUncommited(commit)) {
		// empty id for uncommited changes
	} else {
		id = commit.commit_id;
	}
	updateFileList(frame, id);
}

void MainWindow::updateCurrentFilesList(RepositoryWrapperFrame *frame)
{
	ASSERT_MAIN_THREAD();

	Git::CommitItem commit;
	{
		QTableWidgetItem *item = frame->logtablewidget()->item(selectedLogIndex(frame, false), 0);
		if (!item) return;
		int index = item->data(IndexRole).toInt();
		{
			std::lock_guard lock(frame->commit_log_mutex);
			auto const &logs = frame->commit_log;
			const int count = (int)logs.size();
			Q_ASSERT(index >= 0 && index < count);
			commit = logs[index];
		}
	}
	// if commit is invalid, it means uncommited changes
	updateFileList(frame, commit);
}

void MainWindow::detectGitServerType(GitPtr g)
{
	*ptrGitHub() = GitHubRepositoryInfo();

	QString url;
	{
		Git::Remote remote;
		std::vector<Git::Remote> remotes;
		g->remote_v(&remotes);
		for (Git::Remote const &r : remotes) {
			if (r.name == "origin") {
				remote = r;
				break;
			}
			if (!r.url().isEmpty()) {
				remote = r;
			}
		}
		url = remote.url();
	}

	auto Check = [&](QString const &s){
		int i = (int)url.indexOf(s);
		if (i > 0) return i + (int)s.size();
		return 0;
	};

	// check GitHub
	auto pos = Check("@github.com:");
	if (pos == 0) {
		pos = Check("://github.com/");
	}
	if (pos > 0) {
		auto end = url.size();
		{
			QString s = ".git";
			if (url.endsWith(s)) {
				end -= s.size();
			}
		}
		QString s = url.mid(pos, end - pos);
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

Git::Object MainWindow::internalCatFile(RepositoryWrapperFrame *frame, GitPtr g, const QString &id) //@TODO:
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
			return getObjCache(frame)->catFile(g, Git::CommitID(id));
		}
	}
	return {};
}

Git::Object MainWindow::internalCatFile(RepositoryWrapperFrame *frame, const QString &id) //@TODO:
{
	return internalCatFile(frame, git(), id);
}

Git::Object MainWindow::catFile(QString const &id)
{
	return internalCatFile(frame(), git(), id);
}

void MainWindow::internalOpenRepository(GitPtr g, bool fetch, bool keep_selection)
{
	openRepositoryMain(frame(), g, true, true, fetch, keep_selection);
}

void MainWindow::makeCommitLog(RepositoryWrapperFrame *frame, int scroll_pos, int select_row)
{
	ASSERT_MAIN_THREAD();

	LogTableWidget *logtablewidget = frame->logtablewidget();
	bool block = logtablewidget->blockSignals(true);
	{
		frame->prepareLogTableWidget();

		Git::CommitItemList commit_log;
		{
			std::lock_guard lock(frame->commit_log_mutex);
			commit_log = frame->commit_log;
		}

		const int count = (int)commit_log.size();

		logtablewidget->setRowCount(count);

		int selrow = 0;

		for (int row = 0; row < count; row++) {
			Git::CommitItem const *commit = &commit_log[row];
			{
				auto *item = new QTableWidgetItem;
				item->setData(IndexRole, row);
				logtablewidget->setItem(row, 0, item);
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
				logtablewidget->setItem(row, col, item);
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
					bool uncommited_changes = isThereUncommitedChanges();
					if (isHEAD && !uncommited_changes) { // HEADで、未コミットがないとき
						bold = true; // 太字
						selrow = row;
					}
					commit_id = abbrevCommitID(*commit);
				}
				datetime = misc::makeDateTimeString(commit->commit_date);
				author = commit->author;
				message = commit->message;
				message_ex = makeCommitInfoText(frame, row, &(*getLabelMap(frame))[row], false);
			}
			AddColumn(commit_id, false, QString());
			AddColumn(datetime, false, QString());
			AddColumn(author, false, QString());
			AddColumn(message, bold, message + message_ex);
			logtablewidget->setRowHeight(row, 24);
		}
		int t = logtablewidget->columnWidth(0);
		logtablewidget->resizeColumnsToContents();
		logtablewidget->setColumnWidth(0, t);
		logtablewidget->horizontalHeader()->setStretchLastSection(false);
		logtablewidget->horizontalHeader()->setStretchLastSection(true);

		m->last_focused_file_list = nullptr;

		logtablewidget->setFocus();

		if (select_row < 0) {
			setCurrentLogRow(frame, selrow);
		} else {
			setCurrentLogRow(frame, select_row);
			logtablewidget->verticalScrollBar()->setValue(scroll_pos >= 0 ? scroll_pos : 0);
		}
	}
	logtablewidget->blockSignals(block);

	updateUI();
}

void MainWindow::queryTags(GitPtr g)
{
	// タグを取得
	ptrCommitToTagMap(frame())->clear();
	QList<Git::Tag> tags = g->tags();
	for (Git::Tag const &tag : tags) {
		(*ptrCommitToTagMap(frame()))[tag.id].push_back(tag);
	}
}

void MainWindow::updateHEAD(GitPtr g)
{
	auto head = getObjCache(frame())->revParse(g, "HEAD");
	setHeadId(head);
}

void MainWindow::openRepositoryMain(RepositoryWrapperFrame *frame, GitPtr g, bool query, bool clear_log, bool do_fetch, bool keep_selection)
{
	ASSERT_MAIN_THREAD();

	if (!isValidWorkingCopy(g)) return;

	PtyProcess *pty = getPtyProcess();
	if (pty) {
		pty->wait();
	}

	getObjCache(frame)->setup(g);

	if (clear_log) { // ログをクリア
		std::lock_guard lock(frame->commit_log_mutex);
		frame->commit_log.clear();
	}

	// リポジトリ情報をクリア
	{
		clearLabelMap(frame);
		setUncommitedChanges(false);
		frame->clearLogContents();

		internalClearRepositoryInfo();
		ui->label_repo_name->setText(QString());
		ui->label_branch_name->setText(QString());
	}

	Git::User user;

	//// スレッドで並列処理したら、タイミングによって、queryCommitLogが失敗することがあるので、ひとつずつ実行する
	// std::thread th([this, g, &user](){
	{
		// HEAD を取得
		updateHEAD(g);

		// タグを取得
		queryTags(g);

		// ユーザー情報を取得
		user = g->getUser(Git::Source::Default);
	}
	// });

	// コミットログとブランチ情報を取得
	queryCommitLog(frame, g);

	// th.join(); // HEADとタグとユーザー情報の取得が終わるまで待つ

	// ポジトリの情報を設定
	{
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

	// コミットログを作成
	{
		int scroll_pos = -1;
		int select_row = -1;
		if (keep_selection) {
			scroll_pos = frame->logtablewidget()->verticalScrollBar()->value();
			select_row = frame->logtablewidget()->currentRow();
		}

		makeCommitLog(frame, scroll_pos, select_row);
	}

	// ウィンドウタイトルを更新
	updateWindowTitle(user);

	// コミットログテーブルを更新
	updateCommitLogTable(frame, 0);

	// ファイルリストを更新
	onLogCurrentItemChanged(frame);

	//

	m->commit_detail_getter.stop();
	m->commit_detail_getter.start(g->dup());

	if (do_fetch) {
		do_fetch = isOnlineMode() && (getForceFetch() || appsettings()->automatically_fetch_when_opening_the_repository);
		setForceFetch(false);
		if (do_fetch) {
			fetch(g, false);
		}
	}
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
		QTableWidgetItem *item = frame->logtablewidget()->item(selectedLogIndex(frame, false), 0);
		if (item) {
			std::lock_guard lock(frame->commit_log_mutex);
			auto const &logs = frame->commit_log;
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
							.arg(makeCommitInfoText(frame, row, nullptr, false))
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
	reopenRepository();
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
		reopenRepository();
	}
}

void MainWindow::on_action_rebase_abort_triggered()
{
	doGitCommand([&](GitPtr g){
		g->rebase_abort();
	});
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
		auto head = g->queryCommitItem(g->rev_parse("HEAD"));
		auto pick = g->queryCommitItem(commit->commit_id);
		QList<Git::CommitItem> parents;
		for (int i = 0; i < n; i++) {
			QString id = commit->commit_id.toQString() + QString("^%1").arg(i + 1);
			Git::CommitID id2 = g->rev_parse(id);
			auto item = g->queryCommitItem(id2);
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

	reopenRepository();
}

void MainWindow::merge(RepositoryWrapperFrame *frame, Git::CommitItem commit)
{
	if (isThereUncommitedChanges()) return;

	if (!commit) {
		int row = selectedLogIndex(frame);
		commit = commitItem(frame, row);
		if (!commit) return;
	}

	if (!Git::isValidID(commit.commit_id.toQString())) return;

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

	labels.push_back(commit.commit_id.toQString());

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
	if (!isOnlineMode()) {
		reopenRepository();
		return;
	}

	fetch(git(), false);
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
	updateStatusBarText(frame());
}

void MainWindow::on_treeWidget_repos_itemDoubleClicked(QTreeWidgetItem * /*item*/, int /*column*/)
{
	openSelectedRepository();
}

void MainWindow::execCommitPropertyDialog(QWidget *parent, Git::CommitItem const &commit)
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
		menu.addSeparator();
		QAction *a_add_repository = menu.addAction(tr("&Add repository"));
		QAction *a_scan_folder_and_add = menu.addAction(tr("&Scan folder and add"));
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
			if (a == a_add_repository) {
				QString group = treeItemGroup(treeitem) / treeItemName(treeitem);
				addRepository({}, group);
				return;
			}
			if (a == a_scan_folder_and_add) {
				QString group = treeItemGroup(treeitem) / treeItemName(treeitem);
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
			std::vector<Git::Remote> remotes;
			git(repo->local_dir, {}, {})->remote_v(&remotes);
			for (Git::Remote const &r : remotes) {
				urls.push_back(r.url_fetch);
				urls.push_back(r.url_push);
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
	int row = selectedLogIndex(frame());

	Git::CommitItem const commit = commitItem(frame(), row);
	bool is_valid_commit_id = Git::isValidID(commit.commit_id.toQString());
	// if is_valid_commit_id == false, commit is uncommited changes.

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
		if (commit.has_child) return false; // 子がないこと
		if (Git::isUncommited(commit)) return false; // 未コミットがないこと
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
	QAction *a_delrembranch = remoteBranches(frame(), commit.commit_id, nullptr).isEmpty() ? nullptr : menu.addAction(tr("Delete remote branch..."));

	menu.addSeparator();

	QAction *a_explore = is_valid_commit_id ? menu.addAction(tr("Explore")) : nullptr;
	QAction *a_properties = addMenuActionProperty(&menu);

	QAction *a = menu.exec(frame()->logtablewidget()->viewport()->mapToGlobal(pos) + QPoint(8, -8));
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
			revertCommit(frame());
			return;
		}
		if (a == a_explore) {
			execCommitExploreWindow(frame(), this, &commit);
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
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QListWidgetItem *item = frame()->filelistwidget()->currentItem();

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

	QPoint pt = frame()->filelistwidget()->mapToGlobal(pos) + QPoint(8, -8);
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
				openRepository(false, true, false);
			}
		} else if (a == a_untrack) {
			if (askAreYouSureYouWantToRun("Untrack", tr("rm --cached files"))) {
				for_each_selected_files([&](QString const &path){
					g->rm_cached(path);
				});
				openRepository(false, true, false);
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
					openRepository(false, true, false);
				}
				return;
			}
			if (a == a_untrack) {
				if (askAreYouSureYouWantToRun("Untrack", "rm --cached")) {
					for_each_selected_files([&](QString const &path){
						g->rm_cached(path);
					});
					openRepository(false, true, false);
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
					openRepository(false, true, false);
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
			// サブモジュールのときは新しいプロセスを起動する
			QString commit_id = getSubmoduleCommitId(item);
			QString path = currentWorkingCopyDir() / submodpath;
			qDebug() << path << commit_id;
			QProcess::execute(global->this_executive_program, {path, "--commit-id", commit_id});
		} else {
			// ファイルプロパティダイアログを表示する
			QString path = getFilePath(item);
			Git::CommitID id = getObjectID(item);
			FilePropertyDialog dlg(this);
			dlg.exec(path, id);
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
		openRepository(false, true, false);
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

std::optional<Git::CommitItem> MainWindow::queryCommit(Git::CommitID const &id)
{
	return git()->queryCommitItem(id);
}

void MainWindow::checkout(RepositoryWrapperFrame *frame, QWidget *parent, Git::CommitItem const &commit, std::function<void ()> accepted_callback)
{
	// if (!commit) return;

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
		Git::CommitID id = commit.commit_id;
		if (!id.isValid() && !commit.parent_ids.isEmpty()) {
			id = commit.parent_ids.front();
		}
		bool ok = false;
		setLogEnabled(g, true);
		if (op == CheckoutDialog::Operation::HeadDetached) {
			if (id.isValid()) {
				ok = g->git(QString("checkout \"%1\"").arg(id.toQString()));
			}
		} else if (op == CheckoutDialog::Operation::CreateLocalBranch) {
			if (!name.isEmpty() && id.isValid()) {
				ok = g->git(QString("checkout -b \"%1\" \"%2\"").arg(name).arg(id.toQString()));
				if (!ok) {
					Git::CommitID id = g->rev_parse(name);
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
			if (!name.isEmpty()) {
				ok = g->git(QString("checkout \"%1\"").arg(name));
			}
		}
		if (ok) {
			reopenRepository();
		}
	}
}

void MainWindow::checkout(RepositoryWrapperFrame *frame)
{
	checkout(frame, this, selectedCommitItem(frame));
}

void MainWindow::jumpToCommit(RepositoryWrapperFrame *frame, QString const &id)
{
	GitPtr g = git();
	Git::CommitID id2 = g->rev_parse(id);
	if (id2.isValid()) {
		int row = rowFromCommitId(frame, id2);
		setCurrentLogRow(frame, row);
	}
}

bool MainWindow::saveAs(RepositoryWrapperFrame *frame, const QString &id, const QString &dstpath)
{
	if (id.startsWith(PATH_PREFIX)) {
		return saveFileAs(id.mid(1), dstpath);
	} else {
		return saveBlobAs(frame, id, dstpath);
	}
}

QString MainWindow::determinFileType(QByteArray const &in) const
{
	if (in.isEmpty()) return QString();

	QByteArray in2 = in;

	if (in2.size() > 10) {
		if (memcmp(in2.data(), "\x1f\x8b\x08", 3) == 0) { // gzip
			QBuffer buf;
			MemoryReader reader(in2.data(), in2.size());
			reader.open(MemoryReader::ReadOnly);
			buf.open(QBuffer::WriteOnly);
			gunzip z;
			z.set_maximul_size(100000);
			z.decode(&reader, &buf);
			in2 = buf.buffer();
		}
	}

	QString mimetype;
	if (!in2.isEmpty()) {
		mimetype = global->filetype.mime_by_data(in2.data(), in2.size());
	}
	return mimetype;
}

QString MainWindow::determinFileType(QString const &path) const
{
	QFile file(path);
	if (file.open(QIODevice::ReadOnly)) {
		QByteArray ba;
		ba = file.read(65536);
		file.close();
		return determinFileType(ba);
	}
	return QString();
}

QList<Git::Tag> MainWindow::queryTagList(RepositoryWrapperFrame *frame)
{
	QList<Git::Tag> list;
	Git::CommitItem const commit = selectedCommitItem(frame);
	if (commit && Git::isValidID(commit.commit_id.toQString())) {
		list = findTag(frame, commit.commit_id);
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
	return lookupFileID(git(), getObjCache(frame), commit_id, file);
}

Git::CommitItem MainWindow::commitItem(RepositoryWrapperFrame const *frame, int row) const
{
	std::lock_guard lock(frame->commit_log_mutex);

	if (row >= 0 && row < (int)frame->commit_log.size()) {
		return frame->commit_log[row];
	}

	return {};
}

Git::CommitItem MainWindow::commitItem(RepositoryWrapperFrame const *frame, Git::CommitID const &id) const
{
	if (!frame) { // TODO:
		frame = this->frame();
	}
	
	Git::CommitItem ret;
	
	{
		std::lock_guard lock(frame->commit_log_mutex);
		ret = *frame->commit_log.find(id);
	}
	
	return ret;
}

QImage MainWindow::committerIcon(RepositoryWrapperFrame *frame, int row, QSize size) const
{
	QImage icon;
	if (isAvatarEnabled() && isOnlineMode()) {
		Git::CommitItem commit;
		{
			std::lock_guard lock(frame->commit_log_mutex);
			auto const &logs = frame->commit_log;
			if (row >= 0 && row < (int)logs.size()) {
				commit = logs[row];
			}
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
	setRepositoryList(std::move(repos));

	if (save && changed) {
		saveRepositoryBookmarks(false);
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
void MainWindow::onLogCurrentItemChanged(RepositoryWrapperFrame *frame)
{
	showFileList(FilesListType::SingleList);
	clearFileList(frame);

	// ステータスバー更新
	updateStatusBarText(frame);
	// 少し待ってファイルリストを更新する
	postUserFunctionEvent([&](QVariant const &, void *p){
		RepositoryWrapperFrame *frame = reinterpret_cast<RepositoryWrapperFrame *>(p);
		updateCurrentFilesList(frame);
	}, {}, reinterpret_cast<void *>(frame), 300); // 300ms後（キーボードのオートリピート想定）

	updateAncestorCommitMap(frame);
	frame->logtablewidget()->viewport()->update();
}

void MainWindow::findNext(RepositoryWrapperFrame *frame)
{
	if (m->search_text.isEmpty()) {
		return;
	}

	std::lock_guard lock(frame->commit_log_mutex);

	auto const &logs = frame->commit_log;
	for (int pass = 0; pass < 2; pass++) {
		int row = 0;
		if (pass == 0) {
			row = selectedLogIndex(frame, false);
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
	std::lock_guard lock(frame->commit_log_mutex);

	auto const &logs = frame->commit_log;
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

	std::lock_guard lock(frame->commit_log_mutex);

	auto *logsp = getCommitLogPtr(frame);
	const int LogCount = (int)logsp->size();
	const int index = selectedLogIndex(frame, false);
	if (index >= 0 && index < LogCount) {
		// ok
	} else {
		return;
	}

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
	reopenRepository();
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
	onLogCurrentItemChanged(frame());
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
#if 0
			g->stage(list);
#else
			stage(g, list);

#endif
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

int MainWindow::selectedLogIndex(RepositoryWrapperFrame *frame, bool lock) const
{
	if (lock) {
		std::lock_guard lock(frame->commit_log_mutex);
		return selectedLogIndex(frame, false);
	}

	auto const &logs = frame->commit_log;
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
	QList<Git::Diff> const *diffs = diffResult();
	if (idiff >= 0 && idiff < diffs->size()) {
		Git::Diff const &diff = diffs->at(idiff);
		bool updatediffview = false;
		bool uncommited = false;
		{
			std::lock_guard lock(frame->commit_log_mutex);
			auto const &logs = frame->commit_log;
			int row = frame->logtablewidget()->currentRow();
			uncommited = (row >= 0 && row < (int)logs.size() && Git::isUncommited(logs[row]));
			updatediffview = true;
		}
		if (updatediffview) {
			frame->filediffwidget()->updateDiffView(diff, uncommited);
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
	updateDiffView(frame(), frame()->filelistwidget()->currentItem());
}

void MainWindow::enableDragAndDropOnRepositoryTree(bool enabled)
{
	ui->treeWidget_repos->setDragEnabled(enabled);
	ui->treeWidget_repos->setAcceptDrops(enabled);
	ui->treeWidget_repos->setDropIndicatorShown(enabled);
	ui->treeWidget_repos->viewport()->setAcceptDrops(enabled);
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
		Git::CommitItem const commit = selectedCommitItem(frame());
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
#if 0
		if (global->appsettings.openai_api_key != newsettings.openai_api_key) {
			ApplicationSettings::saveOpenAiApiKey(newsettings.openai_api_key);
		}
#endif
		setAppSettings(newsettings);
		setupExternalPrograms();
		updateAvatar(currentGitUser(), true);
	}
}

void MainWindow::on_action_add_repository_triggered()
{
	addRepository(QString());
}

void MainWindow::onCtrlA()
{
	if (QApplication::focusWidget() == ui->treeWidget_repos) {
		on_action_add_repository_triggered();
	}
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
 * @brief リポジトリフィルタを設定する
 * @param text
 */
void MainWindow::setRepositoryFilterText(QString const &text)
{
	bool b = ui->lineEdit_filter->blockSignals(true);
	ui->lineEdit_filter->setText(text);
	ui->lineEdit_filter->blockSignals(b);

	m->repository_filter_text = text;

	updateRepositoriesList();

	bool enabled = text.isEmpty();
	enableDragAndDropOnRepositoryTree(enabled);
}

/**
 * @brief リポジトリフィルタを消去する
 * @return
 */
void MainWindow::clearRepoFilter()
{
	setRepositoryFilterText({});
}

/**
 * @brief リポジトリフィルタに文字を追加する
 * @return
 */
void MainWindow::appendCharToRepoFilter(ushort c)
{
	if (QChar(c).isLetter()) {
		c = QChar(c).toLower().unicode();
	}
	QString text = getRepositoryFilterText() + QChar(c);
	setRepositoryFilterText(text);
}

/**
 * @brief リポジトリフィルタの文字列から1文字削除する
 * @return
 */
void MainWindow::backspaceRepoFilter()
{
	QString text = getRepositoryFilterText();
	int n = text.size();
	if (n > 0) {
		text = text.mid(0, n - 1);
	}
	setRepositoryFilterText(text);
}

void MainWindow::on_lineEdit_filter_textChanged(QString const &text)
{
	setRepositoryFilterText(text);
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

	Git::CommitItem const commit = selectedCommitItem(frame);
	if (commit) {
		g->revert(commit.commit_id);
		openRepository(false, true, false);
	}
}

void MainWindow::addTag(RepositoryWrapperFrame *frame, QString const &name)
{
	int row = frame->logtablewidget()->currentRow();

	internalAddTag(frame, name);

	frame->selectLogTableRow(row);
}

void MainWindow::on_action_push_all_tags_triggered()
{
	push_tags(git());
}

void MainWindow::on_tableWidget_log_itemDoubleClicked(QTableWidgetItem *)
{
	Git::CommitItem const commit = selectedCommitItem(frame());
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
	Git::Option opt;
	opt.chdir = false;
	opt.pty = getPtyProcess();
	bool f = g->git(cmd, opt);
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

void MainWindow::jump(GitPtr g, Git::CommitID const &id)
{
	if (!id.isValid()) return;

	int row = rowFromCommitId(frame(), id);
	if (row < 0) {
		// No such commit
	} else {
		setCurrentLogRow(frame(), row);
	}
}

void MainWindow::jump(GitPtr g, QString const &text)
{
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
			id = Git::CommitID(dlg2.text());
			if (!id.isValid()) return;
		}
	}

	if (g->objectType(id) == "tag") {
		id = getObjCache(frame())->getCommitIdFromTag(g, text);
	}

	int row = rowFromCommitId(frame(), id);
	if (row < 0) {
		QMessageBox::warning(this, tr("Jump"), QString("%1\n(%2)\n\n").arg(text).arg(text) + tr("No such commit"));
	} else {
		setCurrentLogRow(frame(), row);
	}
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
		jump(g, text);
	}
}

void MainWindow::on_action_repo_checkout_triggered()
{
	checkout(frame());
}

void MainWindow::on_action_delete_branch_triggered()
{
	deleteSelectedBranch(frame());
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
	openRepository(false, true, false);
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
		QApplication::setOverrideCursor(Qt::WaitCursor);
		BlameWindow win(this, path, list);
		QApplication::restoreOverrideCursor();
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

void MainWindow::deleteRemoteBranch(RepositoryWrapperFrame *frame, Git::CommitItem const &commit)
{
	if (!commit) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QStringList all_branches;
	QStringList remote_branches = remoteBranches(frame, commit.commit_id, &all_branches);
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
				push(true, remote, branch, false);
			}
		}
	}
}

QStringList MainWindow::remoteBranches(RepositoryWrapperFrame *frame, Git::CommitID const &id, QStringList *all)
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
	Git::CommitItem const commit = selectedCommitItem(frame());
	if (commit && Git::isValidID(commit.commit_id.toQString())) {
		EditTagsDialog dlg(this, &commit);
		dlg.exec();
	}
}

void MainWindow::on_action_delete_remote_branch_triggered()
{
	auto *f = frame();
	deleteRemoteBranch(f, selectedCommitItem(f));
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

	{ // TODO:
		auto *f = frame();
		std::lock_guard lock(f->commit_log_mutex);

		if (f->commit_log.empty()) {
			return;
		}
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
		auto commit = g2->queryCommitItem(mod.id);
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
	updateCommitLogTable(frame.pointer, 100); // コミットログを更新（100ms後）
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
}

