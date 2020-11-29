
#include "ApplicationGlobal.h"
#include "BasicMainWindow.h"
#include "CheckoutDialog.h"
#include "CloneDialog.h"
#include "CommitDialog.h"
#include "CommitExploreWindow.h"
#include "CommitViewWindow.h"
#include "CreateRepositoryDialog.h"
#include "DeleteBranchDialog.h"
#include "DoYouWantToInitDialog.h"
#include "FileHistoryWindow.h"
#include "FilePropertyDialog.h"
#include "FileUtil.h"
#include "Git.h"
#include "GitDiff.h"
#include "MemoryReader.h"
#include "MySettings.h"
#include "PushDialog.h"
#include "RepositoryPropertyDialog.h"
#include "SelectCommandDialog.h"
#include "SetGlobalUserDialog.h"
#include "SetUserDialog.h"
#include "SettingsDialog.h"
#include "Terminal.h"
#include "TextEditDialog.h"
#include "UserEvent.h"
#include "WelcomeWizardDialog.h"
#include "common/joinpath.h"
#include "common/misc.h"
#include "SubmoduleAddDialog.h"
#include "gpg.h"
#include "gunzip.h"
#include "platform.h"
#include "webclient.h"
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QEvent>
#include <QFileIconProvider>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTreeWidgetItem>
#include <functional>
#include <memory>

class AsyncExecGitThread_ : public QThread {
private:
	GitPtr g;
	std::function<void(GitPtr g)> callback;
public:
		AsyncExecGitThread_(GitPtr const &g, std::function<void(GitPtr const &g)> const &callback)
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

//


BasicMainWindow::BasicMainWindow(QWidget *parent)
	: QMainWindow(parent)
//	, m1(new Private1)
{
}

BasicMainWindow::~BasicMainWindow()
{
//	delete m1;
}

ApplicationSettings *MainWindow::appsettings()
{
	return &global->appsettings;
//	return &m1->appsettings;
}

const ApplicationSettings *MainWindow::appsettings() const
{
	return &global->appsettings;
//	return &m1->appsettings;
}

WebContext *MainWindow::webContext()
{
	return &m1->webcx;
}

QString MainWindow::gitCommand() const
{
	return m1->gcx.git_command;
}

void MainWindow::autoOpenRepository(QString dir)
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

GitPtr MainWindow::git(QString const &dir, QString const &submodpath, QString const &sshkey) const
{
	GitPtr g = std::make_shared<Git>(m1->gcx, dir, submodpath, sshkey);
	if (g && QFileInfo(g->gitCommand()).isExecutable()) {
		g->setLogCallback(git_callback, (void *)this);
		return g;
	} else {
		QString text = tr("git command not specified") + '\n';
		const_cast<MainWindow *>(this)->writeLog(text);
		return GitPtr();
	}
}

GitPtr MainWindow::git()
{
	RepositoryItem const &item = currentRepository();
	return git(item.local_dir, {}, item.ssh_key);
}


GitPtr MainWindow::git(Git::SubmoduleItem const &submod)
{
	if (!submod) return {};
	RepositoryItem const &item = currentRepository();
	return git(item.local_dir, submod.path, item.ssh_key);
}

QPixmap MainWindow::getTransparentPixmap()
{
	return m1->transparent_pixmap;
}

QIcon MainWindow::committerIcon(RepositoryWrapperFrame *frame, int row) const
{
	QIcon icon;
	if (isAvatarEnabled() && isOnlineMode()) {
		auto const &logs = getLogs(frame);
		if (row >= 0 && row < (int)logs.size()) {
			Git::CommitItem const &commit = logs[row];
			if (commit.email.indexOf('@') > 0) {
				std::string email = commit.email.toStdString();
				icon = getAvatarLoader()->fetch(frame, email, true); // from gavatar
			}
		}
	}
	return icon;
}

Git::CommitItem const *MainWindow::commitItem(RepositoryWrapperFrame *frame, int row) const
{
	auto const &logs = getLogs(frame);
	if (row >= 0 && row < (int)logs.size()) {
		return &logs[row];
	}
	return nullptr;
}

QIcon MainWindow::verifiedIcon(char s) const
{
	Git::SignatureGrade g = Git::evaluateSignature(s);
	switch (g) {
	case Git::SignatureGrade::Good:
		return m1->signature_good_icon;
	case Git::SignatureGrade::Bad:
		return m1->signature_bad_icon;
	case Git::SignatureGrade::Unknown:
	case Git::SignatureGrade::Dubious:
	case Git::SignatureGrade::Missing:
		return m1->signature_dubious_icon;
	}
	return QIcon();
}

QString MainWindow::currentWorkingCopyDir() const
{
	return m1->current_repo.local_dir;
}

bool MainWindow::isRepositoryOpened() const
{
	return Git::isValidWorkingCopy(currentWorkingCopyDir());
}

QList<BranchLabel> const *MainWindow::label(RepositoryWrapperFrame const *frame, int row) const
{
	auto it = getLabelMap(frame)->find(row);
	if (it != getLabelMap(frame)->end()) {
		return &it->second;
	}
	return nullptr;
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

bool MainWindow::saveAs(RepositoryWrapperFrame *frame, QString const &id, QString const &dstpath)
{
	if (id.startsWith(PATH_PREFIX)) {
		return saveFileAs(id.mid(1), dstpath);
	} else {
		return saveBlobAs(frame, id, dstpath);
	}
}

bool MainWindow::testRemoteRepositoryValidity(QString const &url, QString const &sshkey)
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

void MainWindow::addWorkingCopyDir(QString const &dir, bool open)
{
	addWorkingCopyDir(dir, QString(), open);
}

bool MainWindow::queryCommit(QString const &id, Git::CommitItem *out)
{
	*out = Git::CommitItem();
	GitPtr g = git();
	return g->queryCommit(id, out);
}

QAction *MainWindow::addMenuActionProperty(QMenu *menu)
{
	return menu->addAction(tr("&Property"));
}

void MainWindow::jumpToCommit(RepositoryWrapperFrame *frame, QString id)
{
	GitPtr g = git();
	id = g->rev_parse(id);
	if (!id.isEmpty()) {
		int row = rowFromCommitId(frame, id);
		setCurrentLogRow(frame, row);
	}
}

void MainWindow::execCommitViewWindow(Git::CommitItem const *commit)
{
	CommitViewWindow win(this, commit);
	win.exec();
}

void MainWindow::execCommitExploreWindow(RepositoryWrapperFrame *frame, QWidget *parent, Git::CommitItem const *commit)
{
	CommitExploreWindow win(parent, this, getObjCache(frame), commit);
	win.exec();
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

QString MainWindow::selectFileCommand(bool save)
{
	char const *exe = FILE_COMMAND;

	QString path = global->appsettings.file_command;

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
	QString path = m1->gcx.ssh_command;

	auto fn = [&](QString const &path){
		setSshCommand(path, save);
	};

	QStringList list = whichCommand_(SSH_COMMAND);

	QStringList cmdlist;
	cmdlist.push_back(SSH_COMMAND);
	return selectCommand_("ssh", cmdlist, list, path, fn);
}

Git::Branch const &MainWindow::currentBranch() const
{
	return m1->current_branch;
}

void MainWindow::setCurrentBranch(Git::Branch const &b)
{
	m1->current_branch = b;
}

RepositoryItem const &MainWindow::currentRepository() const
{
	return m1->current_repo;
}

QString MainWindow::currentRepositoryName() const
{
	return currentRepository().name;
}

QString MainWindow::currentRemoteName() const
{
	return m1->current_remote_name;
}

QString MainWindow::currentBranchName() const
{
	return currentBranch().name;
}

bool MainWindow::isValidWorkingCopy(const GitPtr &g) const
{
	return g && g->isValidWorkingCopy();
}

QString MainWindow::determinFileType_(QString const &path, bool mime, std::function<void (QString const &, QByteArray *)> const &callback) const
{
	const_cast<MainWindow *>(this)->checkFileCommand();
	return misc::determinFileType(global->appsettings.file_command, path, mime, callback);
}

QString MainWindow::determinFileType(QString const &path, bool mime)
{
	return determinFileType_(path, mime, [](QString const &cmd, QByteArray *ba){
		misc2::runCommand(cmd, ba);
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
		int r = misc2::runCommand(cmd, &in, ba);
		if (r != 0) {
			ba->clear();
		}
	});
}

QList<Git::Tag> MainWindow::queryTagList(RepositoryWrapperFrame *frame)
{
	QList<Git::Tag> list;
	Git::CommitItem const *commit = selectedCommitItem(frame);
	if (commit && Git::isValidID(commit->commit_id)) {
		list = findTag(frame, commit->commit_id);
	}
	return list;
}

int MainWindow::limitLogCount() const
{
	int n = appsettings()->maximum_number_of_commit_item_acquisitions;
	return (n >= 1 && n <= 100000) ? n : 10000;
}

TextEditorThemePtr MainWindow::themeForTextEditor()
{
	return global->theme->text_editor_theme;
}

Git::Object MainWindow::cat_file_(RepositoryWrapperFrame *frame, GitPtr const &g, QString const &id)
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

Git::Object MainWindow::cat_file(RepositoryWrapperFrame *frame, QString const &id)
{
	return cat_file_(frame, git(), id);
}

QString MainWindow::newTempFilePath()
{
	QString tmpdir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
	QString path = tmpdir / tempfileHeader() + QString::number(m1->temp_file_counter);
	m1->temp_file_counter++;
	return path;
}

QString MainWindow::findFileID(RepositoryWrapperFrame *frame, QString const &commit_id, QString const &file)
{
	return lookupFileID(getObjCache(frame), commit_id, file);
}

void MainWindow::setAppSettings(const ApplicationSettings &appsettings)
{
	global->appsettings = appsettings;
//	m1->appsettings = appsettings;
}

QPixmap MainWindow::getTransparentPixmap() const
{
	return m1->transparent_pixmap;
}

QIcon MainWindow::getSignatureBadIcon() const
{
	return m1->signature_bad_icon;
}

QIcon MainWindow::getSignatureDubiousIcon() const
{
	return m1->signature_dubious_icon;
}

QIcon MainWindow::getSignatureGoodIcon() const
{
	return m1->signature_good_icon;
}

QIcon MainWindow::getFolderIcon() const
{
	return m1->folder_icon;
}

QIcon MainWindow::getRepositoryIcon() const
{
	return m1->repository_icon;
}













BasicMainWindow::PtyCondition MainWindow::getPtyCondition()
{
	return m1->pty_condition;
}

void MainWindow::setPtyUserData(QVariant const &userdata)
{
	m1->pty_process.setVariant(userdata);
}

void MainWindow::setPtyCondition(const PtyCondition &ptyCondition)
{
	m1->pty_condition = ptyCondition;
}


PtyProcess *MainWindow::getPtyProcess()
{
	return &m1->pty_process;
}

bool MainWindow::getPtyProcessOk() const
{
	return m1->pty_process_ok;
}

void MainWindow::setPtyProcessOk(bool pty_process_ok)
{
	m1->pty_process_ok = pty_process_ok;
}

const QList<RepositoryItem> &MainWindow::getRepos() const
{
	return m1->repos;
}

QList<RepositoryItem> *MainWindow::getReposPtr()
{
	return &m1->repos;
}

void MainWindow::setCurrentRemoteName(QString const &name)
{
	m1->current_remote_name = name;
}

AvatarLoader *MainWindow::getAvatarLoader()
{
	return &m1->avatar_loader;
}

AvatarLoader const *MainWindow::getAvatarLoader() const
{
	return &m1->avatar_loader;
}

//int *MainWindow::ptrUpdateFilesListCounter()
//{
//	return &m1->update_files_list_counter;
//}

//int *MainWindow::ptrUpdateCommitTableCounter()
//{
//	return &m1->update_commit_table_counter;
//}

bool MainWindow::interactionCanceled() const
{
	return m1->interaction_canceled;
}

void MainWindow::setInteractionCanceled(bool canceled)
{
	m1->interaction_canceled = canceled;
}

BasicMainWindow::InteractionMode MainWindow::interactionMode() const
{
	return m1->interaction_mode;
}

void MainWindow::setInteractionMode(const InteractionMode &im)
{
	m1->interaction_mode = im;
}

QString MainWindow::getRepositoryFilterText() const
{
	return m1->repository_filter_text;
}

void MainWindow::setRepositoryFilterText(QString const &text)
{
	m1->repository_filter_text = text;
}

void MainWindow::setUncommitedChanges(bool uncommited_changes)
{
	m1->uncommited_changes = uncommited_changes;
}

QList<Git::Diff> *MainWindow::diffResult()
{
	return &m1->diff_result;
}

void MainWindow::setDiffResult(const QList<Git::Diff> &diffs)
{
	m1->diff_result = diffs;
}

const QList<Git::SubmoduleItem> &MainWindow::submodules() const
{
	return m1->submodules;
}

void MainWindow::setSubmodules(const QList<Git::SubmoduleItem> &submodules)
{
	m1->submodules = submodules;
}

std::map<QString, Git::Diff> *MainWindow::getDiffCacheMap(RepositoryWrapperFrame *frame)
{
//	return &m1->diff_cache;
	return &frame->diff_cache;
}

bool MainWindow::getRemoteChanged() const
{
	return m1->remote_changed;
}

void MainWindow::setRemoteChanged(bool remote_changed)
{
	m1->remote_changed = remote_changed;
}

void MainWindow::setServerType(const ServerType &server_type)
{
	m1->server_type = server_type;
}

GitHubRepositoryInfo *MainWindow::ptrGitHub()
{
	return &m1->github;
}

std::map<int, QList<BranchLabel>> *MainWindow::getLabelMap(RepositoryWrapperFrame *frame)
{
//	return &m1->label_map;
	return &frame->label_map;
}

std::map<int, QList<BranchLabel>> const *MainWindow::getLabelMap(RepositoryWrapperFrame const *frame) const
{
//	return &m1->label_map;
	return &frame->label_map;
}

void MainWindow::clearLabelMap(RepositoryWrapperFrame *frame)
{
	frame->label_map.clear();
//	m1->label_map.clear();
}

GitObjectCache *MainWindow::getObjCache(RepositoryWrapperFrame *frame)
{
	return &frame->objcache;
//	return &m1->objcache;
}

bool MainWindow::getForceFetch() const
{
	return m1->force_fetch;
}

void MainWindow::setForceFetch(bool force_fetch)
{
	m1->force_fetch = force_fetch;
}

std::map<QString, QList<Git::Tag>> *MainWindow::ptrTagMap(RepositoryWrapperFrame *frame)
{
//	return &m1->tag_map;
	return &frame->tag_map;
}

QString MainWindow::getHeadId() const
{
	return m1->head_id;
}

void MainWindow::setHeadId(QString const &head_id)
{
	m1->head_id = head_id;
}

void MainWindow::setPtyProcessCompletionData(QVariant const &value)
{
	m1->pty_process_completion_data = value;
}

QVariant const &MainWindow::getTempRepoForCloneCompleteV() const
{
	return m1->pty_process_completion_data;
}

void MainWindow::updateCommitGraph(RepositoryWrapperFrame *frame)
{
	auto const &logs = getLogs(frame);
	auto *logsp = getLogsPtr(frame);


	const size_t LogCount = logs.size();
	// 樹形図情報を構築する
	if (LogCount > 0) {
		auto LogItem = [&](size_t i)->Git::CommitItem &{ return logsp->at(i); };
		enum { // 有向グラフを構築するあいだ CommitItem::marker_depth をフラグとして使用する
			UNKNOWN = 0,
			KNOWN = 1,
		};
		for (Git::CommitItem &item : *logsp) {
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

bool MainWindow::fetch(GitPtr const &g, bool prune)
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

bool MainWindow::fetch_tags_f(GitPtr const &g)
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

bool MainWindow::git_callback(void *cookie, const char *ptr, int len)
{
	auto *mw = (MainWindow *)cookie;
	mw->emitWriteLog(QByteArray(ptr, len));
	return true;
}

QString MainWindow::selectCommand_(QString const &cmdname, QStringList const &cmdfiles, QStringList const &list, QString path, std::function<void (QString const &)> const &callback)
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

QString MainWindow::selectCommand_(QString const &cmdname, QString const &cmdfile, QStringList const &list, QString const &path, std::function<void (QString const &)> const &callback)
{
	QStringList cmdfiles;
	cmdfiles.push_back(cmdfile);
	return selectCommand_(cmdname, cmdfiles, list, path, callback);
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

bool MainWindow::checkFileCommand()
{
	while (1) {
		if (misc::isExecutable(global->appsettings.file_command)) {
			return true;
		}
		if (selectFileCommand(true).isEmpty()) {
			close();
			return false;
		}
	}
}

bool MainWindow::checkExecutable(QString const &path)
{
	if (QFileInfo(path).isExecutable()) {
		return true;
	}
	QString text = "The specified program '%1' is not executable.\n";
	text = text.arg(path);
	writeLog(text);
	return false;
}

void MainWindow::internalSaveCommandPath(QString const &path, bool save, QString const &name)
{
	if (checkExecutable(path)) {
		if (save) {
			MySettings s;
			s.beginGroup("Global");
			s.setValue(name, path);
			s.endGroup();
		}
	}
}

QStringList MainWindow::whichCommand_(QString const &cmdfile1, QString const &cmdfile2)
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

bool MainWindow::saveByteArrayAs(const QByteArray &ba, QString const &dstpath)
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

bool MainWindow::saveBlobAs(RepositoryWrapperFrame *frame, QString const &id, QString const &dstpath)
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

void MainWindow::deleteTags(RepositoryWrapperFrame *frame, Git::CommitItem const &commit)
{
	auto it = ptrTagMap(frame)->find(commit.commit_id);
	if (it != ptrTagMap(frame)->end()) {
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
	return appsettings()->get_committer_icon;
}

bool MainWindow::isGitHub() const
{
	return m1->server_type == ServerType::GitHub;
}

QStringList MainWindow::remotes() const
{
	return m1->remotes;
}

QList<Git::Branch> MainWindow::findBranch(RepositoryWrapperFrame *frame, QString const &id)
{
	auto it = branchMapRef(frame).find(id);
	if (it != branchMapRef(frame).end()) {
		return it->second;
	}
	return QList<Git::Branch>();
}

QString MainWindow::saveAsTemp(RepositoryWrapperFrame *frame, QString const &id)
{
	QString path = newTempFilePath();
	saveAs(frame, id, path);
	return path;
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

QString MainWindow::getCommitIdFromTag(RepositoryWrapperFrame *frame, QString const &tag)
{
	return getObjCache(frame)->getCommitIdFromTag(tag);
}

bool MainWindow::isValidRemoteURL(QString const &url, QString const &sshkey)
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
		QTime time;
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
			line = QString::fromUtf8(&v[0], v.size()).trimmed();
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
		if (QFileInfo(dir).isDir()) {
			QString text;
			text += tr("The folder is not a valid git repository.") + '\n';
			text += '\n';
			text += dir + '\n';
			text += '\n';
			text += tr("Do you want to initialize it as a git repository ?") + '\n';
			int r = QMessageBox::information(this, tr("Initialize Repository") , text, QMessageBox::Yes, QMessageBox::No);
			if (r == QMessageBox::Yes) {
				createRepository(dir);
			}
		}
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
		GitPtr g = git(item.local_dir, {}, {});
		openRepository_(g);
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

void MainWindow::clearAuthentication()
{
	clearSshAuthentication();
	m1->http_uid.clear();
	m1->http_pwd.clear();
}



QStringList MainWindow::findGitObject(const QString &id) const
{
	QStringList list;
	std::set<QString> set;
	if (Git::isValidID(id)) {
		{
			QString a = id.mid(0, 2);
			QString b = id.mid(2);
			QString dir = m1->current_repo.local_dir / ".git/objects" / a;
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
			QString dir = m1->current_repo.local_dir / ".git/objects/pack";
			QDirIterator it(dir);
			while (it.hasNext()) {
				it.next();
				QFileInfo info = it.fileInfo();
				if (info.isFile() && info.fileName().startsWith("pack-") && info.fileName().endsWith(".idx")) {
					GitPackIdxV2 idx;
					idx.parse(info.absoluteFilePath());
					idx.each([&](GitPackIdxItem const *item){
						if (item->id.startsWith(id)) {
							set.insert(item->id);
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

void MainWindow::writeLog(const char *ptr, int len)
{
	internalWriteLog(ptr, len);
}

void MainWindow::writeLog(const QString &str)
{
	std::string s = str.toStdString();
	writeLog(s.c_str(), s.size());
}

void MainWindow::emitWriteLog(QByteArray const &ba)
{
	emit signalWriteLog(ba);
}

void MainWindow::writeLog_(QByteArray ba)
{
	if (!ba.isEmpty()) {
		writeLog(ba.data(), ba.size());
	}
}

void MainWindow::queryRemotes(GitPtr const &g)
{
	if (!g) return;
	m1->remotes = g->getRemotes();
	std::sort(m1->remotes.begin(), m1->remotes.end());
}

void MainWindow::msgNoRepositorySelected()
{
	QMessageBox::warning(this, qApp->applicationName(), tr("No repository selected"));
}

bool MainWindow::runOnRepositoryDir(std::function<void (QString)> const &callback, RepositoryItem const *repo)
{
	if (!repo) {
		repo = &m1->current_repo;
	}
	QString dir = repo->local_dir;
	dir.replace('\\', '/');
	if (QFileInfo(dir).isDir()) {
		callback(dir);
		return true;
	}
	msgNoRepositorySelected();
	return false;
}

void MainWindow::clearSshAuthentication()
{
	m1->ssh_passphrase_user.clear();
	m1->ssh_passphrase_pass.clear();
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

//int MainWindow::getHunkIndex(QListWidgetItem *item)
//{
//	if (!item) return -1;
//	return item->data(HunkIndexRole).toInt();
//}

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
	webContext()->set_http_proxy(http_proxy);
	webContext()->set_https_proxy(https_proxy);
}

QString MainWindow::abbrevCommitID(Git::CommitItem const &commit)
{
	return commit.commit_id.mid(0, 7);
}

bool MainWindow::saveRepositoryBookmarks() const
{
	QString path = getBookmarksFilePath();
	return RepositoryBookmark::save(path, &getRepos());
}

QString MainWindow::getBookmarksFilePath() const
{
	return global->app_config_dir / "bookmarks.xml";
}

void MainWindow::stopPtyProcess()
{
	getPtyProcess()->stop();
	QDir::setCurrent(m1->starting_dir);
}

void MainWindow::abortPtyProcess()
{
	stopPtyProcess();
	setPtyProcessOk(false);
	setInteractionCanceled(true);
}

void MainWindow::saveApplicationSettings()
{
	SettingsDialog::saveSettings(appsettings());
//	SettingsDialog::saveSettings(&m1->appsettings);
}

void MainWindow::loadApplicationSettings()
{
	SettingsDialog::loadSettings(appsettings());
}

bool MainWindow::execWelcomeWizardDialog()
{
	WelcomeWizardDialog dlg(this);
	dlg.set_git_command_path(appsettings()->git_command);
	dlg.set_file_command_path(appsettings()->file_command);
	dlg.set_default_working_folder(appsettings()->default_working_dir);
	if (misc::isExecutable(appsettings()->git_command)) {
		gitCommand() = appsettings()->git_command;
		Git g(m1->gcx, {}, {}, {});
		Git::User user = g.getUser(Git::Source::Global);
		dlg.set_user_name(user.name);
		dlg.set_user_email(user.email);
	}
	if (dlg.exec() == QDialog::Accepted) {
		setGitCommand(dlg.git_command_path(), false);
		setFileCommand(dlg.file_command_path(), false);
//		appsettings()->git_command  = dlg.git_command_path();
		appsettings()->file_command = dlg.file_command_path();
//		m->gcx.git_command = appsettings()->git_command;
		global->appsettings.file_command = appsettings()->file_command;
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

void MainWindow::execRepositoryPropertyDialog(RepositoryItem const &repo, bool open_repository_menu)
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
	RepositoryPropertyDialog dlg(this, &m1->gcx, g, repo, open_repository_menu);
	dlg.exec();
	if (dlg.isRemoteChanged()) {
		emit remoteInfoChanged();
	}
}

void MainWindow::execSetUserDialog(Git::User const &global_user, Git::User const &repo_user, QString const &reponame)
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

QString MainWindow::executableOrEmpty(QString const &path)
{
	return checkExecutable(path) ? path : QString();
}

void MainWindow::setGitCommand(QString path, bool save)
{
	appsettings()->git_command = m1->gcx.git_command = executableOrEmpty(path);

	internalSaveCommandPath(path, save, "GitCommand");
}

void MainWindow::setFileCommand(QString path, bool save)
{
	appsettings()->file_command = global->appsettings.file_command = executableOrEmpty(path);

	internalSaveCommandPath(path, save, "FileCommand");
}

void MainWindow::setGpgCommand(QString path, bool save)
{
	appsettings()->gpg_command = global->appsettings.gpg_command = executableOrEmpty(path);

	internalSaveCommandPath(path, save, "GpgCommand");
	if (!global->appsettings.gpg_command.isEmpty()) {
		GitPtr g = git();
		g->configGpgProgram(global->appsettings.gpg_command, true);
	}
}

void MainWindow::setSshCommand(QString path, bool save)
{
	appsettings()->ssh_command = executableOrEmpty(path);

	internalSaveCommandPath(path, save, "SshCommand");
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

void MainWindow::setUnknownRepositoryInfo()
{
	setRepositoryInfo("---", "");

	Git g(m1->gcx, {}, {}, {});
	Git::User user = g.getUser(Git::Source::Global);
	setWindowTitle_(user);
}

void MainWindow::internalClearRepositoryInfo()
{
	setHeadId(QString());
	setCurrentBranch(Git::Branch());
	setServerType(ServerType::Standard);
	m1->github = GitHubRepositoryInfo();
}

void MainWindow::checkUser()
{
	Git g(m1->gcx, {}, {}, {});
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
		openRepository(false, false, keep_selection);
		return;
	}

	GitPtr g = git(); // ポインタの有効性チェックはしない（nullptrでも続行）
	openRepository_(g, keep_selection);
}

void MainWindow::updateRepository()
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	OverrideWaitCursor;
	openRepository_(g);
}

void MainWindow::reopenRepository(bool log, std::function<void(GitPtr const &)> const &callback)
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

void MainWindow::openSelectedRepository()
{
	RepositoryItem const *repo = selectedRepositoryItem();
	if (repo) {
		setCurrentRepository(*repo, true);
		openRepository(true);
	}
}

bool MainWindow::isThereUncommitedChanges() const
{
	return m1->uncommited_changes;
}

/**
 * @brief コミットに対応する差分情報を作成
 * @param id コミットID
 * @param ok
 * @return
 */
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
		id = getObjCache(frame)->revParse("HEAD");
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

/**
 * @brief コミットログを取得する
 * @param g
 * @return
 */
Git::CommitItemList MainWindow::retrieveCommitLog(GitPtr const &g)
{
	Git::CommitItemList list = g->log(limitLogCount());

	// 親子関係を調べて、順番が狂っていたら、修正する。

	std::set<QString> set;

	const size_t count = list.size();
	size_t limit = count;

	size_t i = 0;
	while (i < count) {
		size_t newpos = -1;
		for (QString const &parent : list[i].parent_ids) {
			if (set.find(parent) != set.end()) {
				for (size_t j = 0; j < i; j++) {
					if (parent == list[j].commit_id) {
						if (newpos == (size_t)-1 || j < newpos) {
							newpos = j;
						}
						qDebug() << "fix commit order" << list[i].commit_id;
						break;
					}
				}
			}
		}
		set.insert(set.end(), list[i].commit_id);
		if (newpos != (size_t)-1) {
			if (limit == 0) break; // まず無いと思うが、もし、無限ループに陥ったら
			Git::CommitItem t = list[i];
			t.strange_date = true;
			list.erase(list.begin() + i);
			list.insert(list.begin() + newpos, t);
			i = newpos;
			limit--;
		}
		i++;
	}

	return list;
}

void MainWindow::queryBranches(RepositoryWrapperFrame *frame, GitPtr const &g)
{
	Q_ASSERT(g);
//	m1->branch_map.clear();
	frame->branch_map.clear();
	QList<Git::Branch> branches = g->branches();
	for (Git::Branch const &b : branches) {
		if (b.isCurrent()) {
			setCurrentBranch(b);
		}
		branchMapRef(frame)[b.id].append(b);
	}
}

std::map<QString, QList<Git::Branch>> &MainWindow::branchMapRef(RepositoryWrapperFrame *frame)
{
//	return m1->branch_map;
	return frame->branch_map;
}

void MainWindow::updateRemoteInfo()
{
	queryRemotes(git());

	m1->current_remote_name = QString();
	{
		Git::Branch const &r = currentBranch();
		m1->current_remote_name = r.remote;
	}
	if (m1->current_remote_name.isEmpty()) {
		if (m1->remotes.size() == 1) {
			m1->current_remote_name = m1->remotes[0];
		}
	}

	emit remoteInfoChanged();
}

void MainWindow::updateWindowTitle(GitPtr const &g)
{
	if (isValidWorkingCopy(g)) {
		Git::User user = g->getUser(Git::Source::Default);
		setWindowTitle_(user);
	} else {
		setUnknownRepositoryInfo();
	}
}

QString MainWindow::makeCommitInfoText(RepositoryWrapperFrame *frame, int row, QList<BranchLabel> *label_list)
{
	QString message_ex;
	Git::CommitItem const *commit = &getLogs(frame)[row];
	{ // branch
		if (label_list) {
			if (commit->commit_id == getHeadId()) {
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
		QList<Git::Tag> list = findTag(frame, commit->commit_id);
		for (Git::Tag const &t : list) {
			BranchLabel label(BranchLabel::Tag);
			label.text = t.name;
			message_ex += QString(" {#%1}").arg(label.text);
			if (label_list) label_list->push_back(label);
		}
	}
	return message_ex;
}

void MainWindow::removeRepositoryFromBookmark(int index, bool ask)
{
	if (ask) {
		int r = QMessageBox::warning(this, tr("Confirm Remove"), tr("Are you sure you want to remove the repository from bookmarks ?") + '\n' + tr("(Files will NOT be deleted)"), QMessageBox::Ok, QMessageBox::Cancel);
		if (r != QMessageBox::Ok) return;
	}
	auto *repos = getReposPtr();
	if (index >= 0 && index < repos->size()) {
		repos->erase(repos->begin() + index);
		saveRepositoryBookmarks();
		updateRepositoriesList();
	}
}

void MainWindow::clone(QString url, QString dir)
{
	if (!isOnlineMode()) return;

	if (dir.isEmpty()) {
		dir = defaultWorkingDir();
	}

	while (1) {
		QString ssh_key;
		CloneDialog dlg(this, url, dir, &m1->gcx);
		if (dlg.exec() != QDialog::Accepted) {
			return;
		}
		const CloneDialog::Action action = dlg.action();
		url = dlg.url();
		dir = dlg.dir();
		ssh_key = dlg.overridedSshKey();

		RepositoryItem repos_item_data;
		repos_item_data.local_dir = dir;
		repos_item_data.local_dir.replace('\\', '/');
		repos_item_data.name = makeRepositoryName(dir);
		repos_item_data.ssh_key = ssh_key;

		// クローン先ディレクトリを求める

		Git::CloneData clone_data = Git::preclone(url, dir);

		if (action == CloneDialog::Action::Clone) {
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

			GitPtr g = git({}, {}, repos_item_data.ssh_key);
			setPtyUserData(QVariant::fromValue<RepositoryItem>(repos_item_data));
			setPtyCondition(PtyCondition::Clone);
			setPtyProcessOk(true);
			g->clone(clone_data, getPtyProcess());
		} else if (action == CloneDialog::Action::AddExisting) {
			addWorkingCopyDir(dir, true);
		}

		return; // done
	}
}

void MainWindow::submodule_add(QString url, QString local_dir)
{
	if (!isOnlineMode()) return;
	if (local_dir.isEmpty()) return;

	QString dir = local_dir;

	while (1) {
		SubmoduleAddDialog dlg(this, url, dir, &m1->gcx);
		if (dlg.exec() != QDialog::Accepted) {
			return;
		}
		url = dlg.url();
		dir = dlg.dir();
		const QString ssh_key = dlg.overridedSshKey();

		RepositoryItem repos_item_data;
		repos_item_data.local_dir = dir;
		repos_item_data.local_dir.replace('\\', '/');
		repos_item_data.name = makeRepositoryName(dir);
		repos_item_data.ssh_key = ssh_key;

		Git::CloneData data = Git::preclone(url, dir);
		bool force = dlg.isForce();

		GitPtr g = git(local_dir, {}, repos_item_data.ssh_key);

		auto callback = [&](GitPtr const &g){
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



void MainWindow::commit(RepositoryWrapperFrame *frame, bool amend)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QString message;
	QString previousMessage;

	if (amend) {
		message = getLogs(frame)[0].message;
	} else {
		QString id = g->getCherryPicking();
		if (Git::isValidID(id)) {
			message = g->getMessage(id);
		} else {
			for (Git::CommitItem const &item : getLogs(frame)) {
				if (!item.commit_id.isEmpty()) {
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
				writeLog(err);
			}
		}
		break;
	}
}

void MainWindow::commitAmend(RepositoryWrapperFrame *frame)
{
	commit(frame, true);
}

void MainWindow::pushSetUpstream(QString const &remote, QString const &branch)
{
	if (remote.isEmpty()) return;
	if (branch.isEmpty()) return;

	int exitcode = 0;
	QString errormsg;

	reopenRepository(true, [&](GitPtr g){
		g->push_u(remote, branch, getPtyProcess());
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

bool MainWindow::pushSetUpstream(bool testonly)
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

void MainWindow::push()
{
	if (!isOnlineMode()) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	if (g->getRemotes().isEmpty()) {
		QMessageBox::warning(this, qApp->applicationName(), tr("No remote repository is registered."));
		RepositoryItem const &repo = currentRepository();
		execRepositoryPropertyDialog(repo, true);
		return;
	}

	int exitcode = 0;
	QString errormsg;

	reopenRepository(true, [&](GitPtr g){
		g->push(false, getPtyProcess());
		while (1) {
			if (getPtyProcess()->wait(1)) break;
			QApplication::processEvents();
		}
		exitcode = getPtyProcess()->getExitCode();
		errormsg = getPtyProcess()->getMessage();
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

void MainWindow::openTerminal(RepositoryItem const *repo)
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

void MainWindow::openExplorer(RepositoryItem const *repo)
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

Git::CommitItem const *MainWindow::selectedCommitItem(RepositoryWrapperFrame *frame) const
{
	int i = selectedLogIndex(frame);
	return commitItem(frame, i);
}

void MainWindow::deleteBranch(RepositoryWrapperFrame *frame, Git::CommitItem const *commit)
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
			if (item.id == commit->commit_id) {
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
				writeLog(tr("Failed to delete the branch '%1'").arg(name) + '\n');
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

QStringList MainWindow::remoteBranches(RepositoryWrapperFrame *frame, QString const &id, QStringList *all)
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

void MainWindow::deleteRemoteBranch(RepositoryWrapperFrame *frame, Git::CommitItem const *commit)
{
	if (!commit) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QStringList all_branches;
	QStringList remote_branches = remoteBranches(frame, commit->commit_id, &all_branches);
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
				pushSetUpstream(remote, branch);
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

bool MainWindow::editFile(QString const &path, QString const &title)
{
	return TextEditDialog::editFile(this, path, title);
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

void MainWindow::saveRepositoryBookmark(RepositoryItem item)
{
	if (item.local_dir.isEmpty()) return;

	if (item.name.isEmpty()) {
		item.name = tr("Unnamed");
	}

	auto *repos = getReposPtr();

	bool done = false;
	for (auto &repo : *repos) {
		RepositoryItem *p = &repo;
		if (item.local_dir == p->local_dir) {
			*p = item;
			done = true;
			break;
		}
	}
	if (!done) {
		repos->push_back(item);
	}
	saveRepositoryBookmarks();
	updateRepositoriesList();
}

void MainWindow::setCurrentRepository(RepositoryItem const &repo, bool clear_authentication)
{
	if (clear_authentication) {
		clearAuthentication();
	}
	m1->current_repo = repo;
}

void MainWindow::internalDeleteTags(QStringList const &tagnames)
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

bool MainWindow::internalAddTag(RepositoryWrapperFrame *frame, QString const &name)
{
	if (name.isEmpty()) return false;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return false;

	QString commit_id;

	Git::CommitItem const *commit = selectedCommitItem(frame);
	if (commit && !commit->commit_id.isEmpty()) {
		commit_id = commit->commit_id;
	}

	if (!Git::isValidID(commit_id)) return false;

	bool ok = false;
	reopenRepository(false, [&](GitPtr g){
		ok = g->tag(name, commit_id);
	});

	return ok;
}

NamedCommitList MainWindow::namedCommitItems(RepositoryWrapperFrame *frame, int flags)
{
	NamedCommitList items;
	if (flags & Branches) {
		for (auto const &pair: branchMapRef(frame)) {
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
		for (auto const &pair: *ptrTagMap(frame)) {
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

int MainWindow::rowFromCommitId(RepositoryWrapperFrame *frame, QString const &id)
{
	auto const &logs = getLogs(frame);
	for (size_t i = 0; i < logs.size(); i++) {
		Git::CommitItem const &item = logs[i];
		if (item.commit_id == id) {
			return (int)i;
		}
	}
	return -1;
}

void MainWindow::createRepository(QString const &dir)
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
						addWorkingCopyDir(path, name, true);
					}
					QString remote_name = dlg.remoteName();
					QString remote_url = dlg.remoteURL();
					if (!remote_name.isEmpty() && !remote_url.isEmpty()) {
						Git::Remote r;
						r.name = remote_name;
						r.url = remote_url;
						g->addRemoteURL(r);
					}
				}
			}
		} else {
			// not dir
		}
	}
}

void MainWindow::setLogEnabled(GitPtr const &g, bool f)
{
	if (f) {
		g->setLogCallback(git_callback, this);
	} else {
		g->setLogCallback(nullptr, nullptr);
	}
}

QList<Git::Tag> MainWindow::findTag(RepositoryWrapperFrame *frame, QString const &id)
{
	auto it = ptrTagMap(frame)->find(id);
	if (it != ptrTagMap(frame)->end()) {
		return it->second;
	}
	return QList<Git::Tag>();
}

void MainWindow::sshSetPassphrase(std::string const &user, std::string const &pass)
{
	m1->ssh_passphrase_user = user;
	m1->ssh_passphrase_pass = pass;
}

std::string MainWindow::sshPassphraseUser() const
{
	return m1->ssh_passphrase_user;
}

std::string MainWindow::sshPassphrasePass() const
{
	return m1->ssh_passphrase_pass;
}

void MainWindow::httpSetAuthentication(std::string const &user, std::string const &pass)
{
	m1->http_uid = user;
	m1->http_pwd = pass;
}

std::string MainWindow::httpAuthenticationUser() const
{
	return m1->http_uid;
}

std::string MainWindow::httpAuthenticationPass() const
{
	return m1->http_pwd;
}

void MainWindow::doGitCommand(std::function<void(GitPtr g)> const &callback)
{
	GitPtr g = git();
	if (isValidWorkingCopy(g)) {
		OverrideWaitCursor;
		callback(g);
		openRepository(false, false);
	}
}








