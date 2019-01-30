
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
#include "JumpDialog.h"
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
#include "WelcomeWizardDialog.h"
#include "common/joinpath.h"
#include "common/misc.h"
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

//

struct BasicMainWindow::Private {
	ApplicationSettings appsettings;

	QIcon repository_icon;
	QIcon folder_icon;
	QIcon signature_good_icon;
	QIcon signature_dubious_icon;
	QIcon signature_bad_icon;
	QPixmap transparent_pixmap;

	QString starting_dir;
	Git::Context gcx;
	RepositoryItem current_repo;

	QList<RepositoryItem> repos;
	Git::CommitItemList logs;
	QList<Git::Diff> diff_result;

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
	PtyCondition pty_condition = PtyCondition::None;

	WebContext webcx;

	AvatarLoader avatar_loader;
	int update_files_list_counter = 0;
	int update_commit_table_counter = 0;

	bool interaction_canceled = false;
	InteractionMode interaction_mode = InteractionMode::None;

	QString repository_filter_text;
	bool uncommited_changes = false;

	std::map<QString, QList<Git::Branch>> branch_map;
	std::map<QString, QList<Git::Tag>> tag_map;
	std::map<int, QList<Label>> label_map;
	std::map<QString, Git::Diff> diff_cache;
	GitObjectCache objcache;

	bool remote_changed = false;

	ServerType server_type = ServerType::Standard;
	GitHubRepositoryInfo github;

	QString head_id;
	bool force_fetch = false;

	RepositoryItem temp_repo_for_clone_complete;
	QVariant pty_process_completion_data;
};

BasicMainWindow::BasicMainWindow(QWidget *parent)
	: QMainWindow(parent)
	, m(new Private)
{
	SettingsDialog::loadSettings(&m->appsettings);
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
}

BasicMainWindow::~BasicMainWindow()
{
	setRemoteMonitoringEnabled(false);
	deleteTempFiles();
	delete m;
}

ApplicationSettings *BasicMainWindow::appsettings()
{
	return &m->appsettings;
}

const ApplicationSettings *BasicMainWindow::appsettings() const
{
	return &m->appsettings;
}

WebContext *BasicMainWindow::webContext()
{
	return &m->webcx;
}

QString BasicMainWindow::gitCommand() const
{
	return m->gcx.git_command;
}

namespace {

enum UserEvent {
	EventUserFunction = QEvent::User,
};

class UserFunctionEvent : public QEvent {
public:
	std::function<void(QVariant &)> func;
	QVariant var;

	explicit UserFunctionEvent(std::function<void(QVariant const &)> const &func, QVariant const &var)
		: QEvent((QEvent::Type)EventUserFunction)
		, func(func)
		, var(var)
	{
	}
};

} // namespace

bool BasicMainWindow::event(QEvent *event)
{
	if (event->type() == EventUserFunction) {
		if (auto *e = dynamic_cast<UserFunctionEvent *>(event)) {
			e->func(e->var);
			return true;
		}
	}
	return QMainWindow::event(event);
}

void BasicMainWindow::postUserFunctionEvent(const std::function<void(QVariant const &)> &fn, QVariant const &v)
{
	qApp->postEvent(this, new UserFunctionEvent(fn, v));
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

GitPtr BasicMainWindow::git(QString const &dir) const
{
	const_cast<BasicMainWindow *>(this)->checkGitCommand();

	GitPtr g = std::make_shared<Git>(m->gcx, dir);
	g->setLogCallback(git_callback, (void *)this);
	return g;
}

GitPtr BasicMainWindow::git()
{
	return git(currentWorkingCopyDir());
}

QPixmap BasicMainWindow::getTransparentPixmap()
{
	return m->transparent_pixmap;
}

QIcon BasicMainWindow::committerIcon(int row) const
{
	QIcon icon;
	if (isAvatarEnabled() && isRemoteOnline()) {
		auto const &logs = getLogs();
		if (row >= 0 && row < (int)logs.size()) {
			Git::CommitItem const &commit = logs[row];
			if (commit.email.indexOf('@') > 0) {
				std::string email = commit.email.toStdString();
				icon = getAvatarLoader()->fetch(email, true); // from gavatar
			}
		}
	}
	return icon;
}

Git::CommitItem const *BasicMainWindow::commitItem(int row) const
{
	auto const &logs = getLogs();
	if (row >= 0 && row < (int)logs.size()) {
		return &logs[row];
	}
	return nullptr;
}

QIcon BasicMainWindow::verifiedIcon(char s) const
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

QString BasicMainWindow::currentWorkingCopyDir() const
{
	QString workdir = m->current_repo.local_dir;
	return workdir.isEmpty() ? QString() : workdir;
}

QList<BasicMainWindow::Label> const *BasicMainWindow::label(int row)
{
	auto it = getLabelMap()->find(row);
	if (it != getLabelMap()->end()) {
		return &it->second;
	}
	return nullptr;
}

bool BasicMainWindow::saveAs(QString const &id, QString const &dstpath)
{
	if (id.startsWith(PATH_PREFIX)) {
		return saveFileAs(id.mid(1), dstpath);
	} else {
		return saveBlobAs(id, dstpath);
	}
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

void BasicMainWindow::addWorkingCopyDir(QString dir, bool open)
{
	addWorkingCopyDir(dir, QString(), open);
}

bool BasicMainWindow::queryCommit(QString const &id, Git::CommitItem *out)
{
	*out = Git::CommitItem();
	GitPtr g = git();
	return g->queryCommit(id, out);
}

QAction *BasicMainWindow::addMenuActionProperty(QMenu *menu)
{
	return menu->addAction(tr("&Property"));
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
//		JumpDialog::sort(&named_commits);
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

void BasicMainWindow::jumpToCommit(QString id)
{
	GitPtr g = git();
	id = g->rev_parse(id);
	if (!id.isEmpty()) {
		int row = rowFromCommitId(id);
		setCurrentLogRow(row);
	}
}

void BasicMainWindow::execCommitViewWindow(Git::CommitItem const *commit)
{
	CommitViewWindow win(this, commit);
	win.exec();
}

void BasicMainWindow::execCommitExploreWindow(QWidget *parent, Git::CommitItem const *commit)
{
	CommitExploreWindow win(parent, this, getObjCache(), commit);
	win.exec();
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

void BasicMainWindow::execFilePropertyDialog(QListWidgetItem *item)
{
	if (item) {
		QString path = getFilePath(item);
		QString id = getObjectID(item);
		FilePropertyDialog dlg(this);
		dlg.exec(this, path, id);
	}
}

QString BasicMainWindow::selectGitCommand(bool save)
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

Git::Branch const &BasicMainWindow::currentBranch() const
{
	return m->current_branch;
}

void BasicMainWindow::setCurrentBranch(Git::Branch const &b)
{
	m->current_branch = b;
}

RepositoryItem const &BasicMainWindow::currentRepository() const
{
	return m->current_repo;
}

QString BasicMainWindow::currentRepositoryName() const
{
	return currentRepository().name;
}

QString BasicMainWindow::currentRemoteName() const
{
	return m->current_remote_name;
}

QString BasicMainWindow::currentBranchName() const
{
	return currentBranch().name;
}

bool BasicMainWindow::isValidWorkingCopy(const GitPtr &g) const
{
	return g && g->isValidWorkingCopy();
}

QString BasicMainWindow::determinFileType_(QString const &path, bool mime, std::function<void (QString const &, QByteArray *)> callback) const
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

QList<Git::Tag> BasicMainWindow::queryTagList()
{
	QList<Git::Tag> list;
	Git::CommitItem const *commit = selectedCommitItem();
	if (commit && Git::isValidID(commit->commit_id)) {
		list = findTag(commit->commit_id);
	}
	return list;
}

int BasicMainWindow::limitLogCount() const
{
	int n = appsettings()->maximum_number_of_commit_item_acquisitions;
	return (n >= 1 && n <= 100000) ? n : 10000;
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
			return getObjCache()->catFile(id);;
		}
	}
	return Git::Object();
}

Git::Object BasicMainWindow::cat_file(QString const &id)
{
	return cat_file_(git(), id);
}

QString BasicMainWindow::newTempFilePath()
{
	QString tmpdir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
	QString path = tmpdir / tempfileHeader() + QString::number(m->temp_file_counter);
	m->temp_file_counter++;
	return path;
}

QString BasicMainWindow::findFileID(GitPtr, QString const &commit_id, QString const &file)
{
	return lookupFileID(getObjCache(), commit_id, file);
}

void BasicMainWindow::updateFilesList(QString const &id, QList<Git::Diff> *diff_list, QListWidget *listwidget)
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

	GitDiff dm(getObjCache());
	if (!dm.diff(id, diff_list)) return;

	addDiffItems(diff_list, AddItem);
}

void BasicMainWindow::writeLog(const char *ptr, int len)
{
	internalWriteLog(ptr, len);
}

void BasicMainWindow::setAppSettings(const ApplicationSettings &appsettings)
{
	m->appsettings = appsettings;
}

QPixmap BasicMainWindow::getTransparentPixmap() const
{
	return m->transparent_pixmap;
}

QIcon BasicMainWindow::getSignatureBadIcon() const
{
	return m->signature_bad_icon;
}

QIcon BasicMainWindow::getSignatureDubiousIcon() const
{
	return m->signature_dubious_icon;
}

QIcon BasicMainWindow::getSignatureGoodIcon() const
{
	return m->signature_good_icon;
}

QIcon BasicMainWindow::getFolderIcon() const
{
	return m->folder_icon;
}

QIcon BasicMainWindow::getRepositoryIcon() const
{
	return m->repository_icon;
}

const Git::CommitItemList &BasicMainWindow::getLogs() const
{
	return m->logs;
}

Git::CommitItem const *BasicMainWindow::getLog(int index) const
{
	return (index >= 0 && index < (int)m->logs.size()) ? &m->logs[index] : nullptr;
}

Git::CommitItemList *BasicMainWindow::getLogsPtr()
{
	return &m->logs;
}

void BasicMainWindow::setLogs(const Git::CommitItemList &logs)
{
	m->logs = logs;
}

void BasicMainWindow::clearLogs()
{
	m->logs.clear();
}

BasicMainWindow::PtyCondition BasicMainWindow::getPtyCondition()
{
	return m->pty_condition;
}

void BasicMainWindow::setPtyUserData(QVariant const &userdata)
{
	m->pty_process.setVariant(userdata);
}

void BasicMainWindow::setPtyCondition(const PtyCondition &ptyCondition)
{
	m->pty_condition = ptyCondition;
}


PtyProcess *BasicMainWindow::getPtyProcess()
{
	return &m->pty_process;
}

bool BasicMainWindow::getPtyProcessOk() const
{
	return m->pty_process_ok;
}

void BasicMainWindow::setPtyProcessOk(bool pty_process_ok)
{
	m->pty_process_ok = pty_process_ok;
}

const QList<RepositoryItem> &BasicMainWindow::getRepos() const
{
	return m->repos;
}

QList<RepositoryItem> *BasicMainWindow::getReposPtr()
{
	return &m->repos;
}

void BasicMainWindow::setCurrentRemoteName(QString const &name)
{
	m->current_remote_name = name;
}

AvatarLoader *BasicMainWindow::getAvatarLoader()
{
	return &m->avatar_loader;
}

AvatarLoader const *BasicMainWindow::getAvatarLoader() const
{
	return &m->avatar_loader;
}

int *BasicMainWindow::ptrUpdateFilesListCounter()
{
	return &m->update_files_list_counter;
}

int *BasicMainWindow::ptrUpdateCommitTableCounter()
{
	return &m->update_commit_table_counter;
}

bool BasicMainWindow::interactionCanceled() const
{
	return m->interaction_canceled;
}

void BasicMainWindow::setInteractionCanceled(bool canceled)
{
	m->interaction_canceled = canceled;
}

BasicMainWindow::InteractionMode BasicMainWindow::interactionMode() const
{
	return m->interaction_mode;
}

void BasicMainWindow::setInteractionMode(const InteractionMode &im)
{
	m->interaction_mode = im;
}

QString BasicMainWindow::getRepositoryFilterText() const
{
	return m->repository_filter_text;
}

void BasicMainWindow::setRepositoryFilterText(QString const &text)
{
	m->repository_filter_text = text;
}

void BasicMainWindow::setUncommitedChanges(bool uncommited_changes)
{
	m->uncommited_changes = uncommited_changes;
}

QList<Git::Diff> *BasicMainWindow::diffResult()
{
	return &m->diff_result;
}

std::map<QString, Git::Diff> *BasicMainWindow::getDiffCacheMap()
{
	return &m->diff_cache;
}

bool BasicMainWindow::getRemoteChanged() const
{
	return m->remote_changed;
}

void BasicMainWindow::setRemoteChanged(bool remote_changed)
{
	m->remote_changed = remote_changed;
}

void BasicMainWindow::setServerType(const ServerType &server_type)
{
	m->server_type = server_type;
}

GitHubRepositoryInfo *BasicMainWindow::ptrGitHub()
{
	return &m->github;
}

std::map<int, QList<BasicMainWindow::Label>> *BasicMainWindow::getLabelMap()
{
	return &m->label_map;
}

void BasicMainWindow::clearLabelMap()
{
	m->label_map.clear();
}

GitObjectCache *BasicMainWindow::getObjCache()
{
	return &m->objcache;
}



bool BasicMainWindow::getForceFetch() const
{
	return m->force_fetch;
}

void BasicMainWindow::setForceFetch(bool force_fetch)
{
	m->force_fetch = force_fetch;
}

std::map<QString, QList<Git::Tag>> *BasicMainWindow::ptrTagMap()
{
	return &m->tag_map;
}

QString BasicMainWindow::getHeadId() const
{
	return m->head_id;
}

void BasicMainWindow::setHeadId(QString const &head_id)
{
	m->head_id = head_id;
}

void BasicMainWindow::setPtyProcessCompletionData(QVariant const &value)
{
	m->pty_process_completion_data = value;
}

QVariant const &BasicMainWindow::getTempRepoForCloneCompleteV() const
{
	return m->pty_process_completion_data;
}

void BasicMainWindow::updateCommitGraph()
{
	auto const &logs = getLogs();
	auto *logsp = getLogsPtr();

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

bool BasicMainWindow::git_callback(void *cookie, const char *ptr, int len)
{
	auto *mw = (BasicMainWindow *)cookie;
	mw->emitWriteLog(QByteArray(ptr, len));
	return true;
}

QString BasicMainWindow::selectCommand_(QString const &cmdname, QStringList const &cmdfiles, QStringList const &list, QString path, std::function<void (QString const &)> const &callback)
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

QString BasicMainWindow::selectCommand_(QString const &cmdname, QString const &cmdfile, QStringList const &list, QString const &path, std::function<void (QString const &)> const &callback)
{
	QStringList cmdfiles;
	cmdfiles.push_back(cmdfile);
	return selectCommand_(cmdname, cmdfiles, list, path, callback);
}

bool BasicMainWindow::checkGitCommand()
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

void BasicMainWindow::emitWriteLog(QByteArray ba)
{
	emit signalWriteLog(ba);
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

bool BasicMainWindow::saveByteArrayAs(const QByteArray &ba, QString const &dstpath)
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

void BasicMainWindow::deleteTags(Git::CommitItem const &commit)
{
	auto it = ptrTagMap()->find(commit.commit_id);
	if (it != ptrTagMap()->end()) {
		QStringList names;
		QList<Git::Tag> const &tags = it->second;
		for (Git::Tag const &tag : tags) {
			names.push_back(tag.name);
		}
		deleteTags(names);
	}
}

bool BasicMainWindow::isAvatarEnabled() const
{
	return appsettings()->get_committer_icon;
}

bool BasicMainWindow::isGitHub() const
{
	return m->server_type == ServerType::GitHub;
}

QStringList BasicMainWindow::remotes() const
{
	return m->remotes;
}

QList<Git::Branch> BasicMainWindow::findBranch(QString const &id)
{
	auto it = branchMapRef().find(id);
	if (it != branchMapRef().end()) {
		return it->second;
	}
	return QList<Git::Branch>();
}

QString BasicMainWindow::saveAsTemp(QString const &id)
{
	QString path = newTempFilePath();
	saveAs(id, path);
	return path;
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

QString BasicMainWindow::getCommitIdFromTag(QString const &tag)
{
	return getObjCache()->getCommitIdFromTag(tag);
}

bool BasicMainWindow::isValidRemoteURL(QString const &url)
{
	if (url.indexOf('\"') >= 0) {
		return false;
	}
	stopPtyProcess();
	GitPtr g = git(QString());
	QString cmd = "ls-remote \"%1\" HEAD";
	bool f = g->git(cmd.arg(url), false, false, getPtyProcess());
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
		GitPtr g = git(item.local_dir);
		openRepository_(g);
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

void BasicMainWindow::clearAuthentication()
{
	clearSshAuthentication();
	m->http_uid.clear();
	m->http_pwd.clear();
}

void BasicMainWindow::onAvatarUpdated()
{
	updateCommitTableLater();
}

void BasicMainWindow::queryRemotes(GitPtr g)
{
	m->remotes = g->getRemotes();
	std::sort(m->remotes.begin(), m->remotes.end());
}

bool BasicMainWindow::runOnRepositoryDir(std::function<void (QString)> callback, RepositoryItem const *repo)
{
	if (!repo) {
		repo = &m->current_repo;
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

void BasicMainWindow::clearSshAuthentication()
{
	m->ssh_passphrase_user.clear();
	m->ssh_passphrase_pass.clear();
}

QString BasicMainWindow::getFilePath(QListWidgetItem *item)
{
	if (!item) return QString();
	return item->data(FilePathRole).toString();
}

bool BasicMainWindow::isGroupItem(QTreeWidgetItem *item)
{
	if (item) {
		int index = item->data(0, IndexRole).toInt();
		if (index == GroupItem) {
			return true;
		}
	}
	return false;
}

int BasicMainWindow::indexOfLog(QListWidgetItem *item)
{
	if (!item) return -1;
	return item->data(IndexRole).toInt();
}

int BasicMainWindow::indexOfDiff(QListWidgetItem *item)
{
	if (!item) return -1;
	return item->data(DiffIndexRole).toInt();
}

int BasicMainWindow::getHunkIndex(QListWidgetItem *item)
{
	if (!item) return -1;
	return item->data(HunkIndexRole).toInt();
}

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
	webContext()->set_http_proxy(http_proxy);
	webContext()->set_https_proxy(https_proxy);
}

QString BasicMainWindow::abbrevCommitID(Git::CommitItem const &commit)
{
	return commit.commit_id.mid(0, 7);
}

bool BasicMainWindow::saveRepositoryBookmarks() const
{
	QString path = getBookmarksFilePath();
	return RepositoryBookmark::save(path, &getRepos());
}

QString BasicMainWindow::getBookmarksFilePath() const
{
	return global->app_config_dir / "bookmarks.xml";
}

void BasicMainWindow::stopPtyProcess()
{
	getPtyProcess()->stop();
	QDir::setCurrent(m->starting_dir);
}

void BasicMainWindow::abortPtyProcess()
{
	stopPtyProcess();
	setPtyProcessOk(false);
	setInteractionCanceled(true);
}

bool BasicMainWindow::execWelcomeWizardDialog()
{
	WelcomeWizardDialog dlg(this);
	dlg.set_git_command_path(appsettings()->git_command);
	dlg.set_file_command_path(appsettings()->file_command);
	dlg.set_default_working_folder(appsettings()->default_working_dir);
	if (misc::isExecutable(appsettings()->git_command)) {
		gitCommand() = appsettings()->git_command;
		Git g(m->gcx, QString());
		Git::User user = g.getUser(Git::Source::Global);
		dlg.set_user_name(user.name);
		dlg.set_user_email(user.email);
	}
	if (dlg.exec() == QDialog::Accepted) {
		appsettings()->git_command  = gitCommand()   = dlg.git_command_path();
		appsettings()->file_command = global->file_command = dlg.file_command_path();
		appsettings()->default_working_dir = dlg.default_working_folder();
		SettingsDialog::saveSettings(&m->appsettings);

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

void BasicMainWindow::setGitCommand(QString const &path, bool save)
{
	internalSetCommand(path, save, "GitCommand", &m->gcx.git_command);
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

void BasicMainWindow::logGitVersion()
{
	GitPtr g = git();
	QString s = g->version();
	if (!s.isEmpty()) {
		s += '\n';
		writeLog(s);
	}
}

void BasicMainWindow::setUnknownRepositoryInfo()
{
	setRepositoryInfo("---", "");

	Git g(m->gcx, QString());
	Git::User user = g.getUser(Git::Source::Global);
	setWindowTitle_(user);
}

void BasicMainWindow::internalClearRepositoryInfo()
{
	setHeadId(QString());
	setCurrentBranch(Git::Branch());
	setServerType(ServerType::Standard);
	m->github = GitHubRepositoryInfo();
}

void BasicMainWindow::checkUser()
{
	Git g(m->gcx, QString());
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

void BasicMainWindow::reopenRepository(bool log, std::function<void (GitPtr)> callback)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	setRemoteMonitoringEnabled(false);

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

	setRemoteMonitoringEnabled(true);
}

void BasicMainWindow::openSelectedRepository()
{
	RepositoryItem const *repo = selectedRepositoryItem();
	if (repo) {
		setCurrentRepository(*repo, true);
		openRepository(true);
	}
}

void BasicMainWindow::checkRemoteUpdate()
{
	if (getPtyProcess()->isRunning()) return;

	emit signalCheckRemoteUpdate();
}

bool BasicMainWindow::isThereUncommitedChanges() const
{
	return m->uncommited_changes;
}

bool BasicMainWindow::makeDiff(QString id, QList<Git::Diff> *out)
{
	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return false;

	Git::FileStatusList list = g->status();
	setUncommitedChanges(!list.empty());

	if (id.isEmpty() && !isThereUncommitedChanges()) {
		id = getObjCache()->revParse("HEAD");
	}

	bool uncommited = (id.isEmpty() && isThereUncommitedChanges());

	GitDiff dm(getObjCache());
	if (uncommited) {
		dm.diff_uncommited(out);
	} else {
		dm.diff(id, out);
	}

	return true; // success
}

void BasicMainWindow::addDiffItems(const QList<Git::Diff> *diff_list, const std::function<void (QString const &, QString, int)> &add_item)
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

void BasicMainWindow::queryBranches(GitPtr g)
{
	Q_ASSERT(g);
	m->branch_map.clear();
	QList<Git::Branch> branches = g->branches();
	for (Git::Branch const &b : branches) {
		if (b.isCurrent()) {
			setCurrentBranch(b);
		}
		branchMapRef()[b.id].append(b);
	}
}

std::map<QString, QList<Git::Branch>> &BasicMainWindow::branchMapRef()
{
	return m->branch_map;
}

void BasicMainWindow::updateCommitTableLater()
{
	*ptrUpdateCommitTableCounter() = 200;
}

void BasicMainWindow::updateRemoteInfo()
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

void BasicMainWindow::updateWindowTitle(GitPtr g)
{
	if (isValidWorkingCopy(g)) {
		Git::User user = g->getUser(Git::Source::Default);
		setWindowTitle_(user);
	} else {
		setUnknownRepositoryInfo();
	}
}

QString BasicMainWindow::makeCommitInfoText(int row, QList<BasicMainWindow::Label> *label_list)
{
	QString message_ex;
	Git::CommitItem const *commit = &getLogs()[row];
	{ // branch
		if (label_list) {
			if (commit->commit_id == getHeadId()) {
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

void BasicMainWindow::removeRepositoryFromBookmark(int index, bool ask)
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

void BasicMainWindow::clone(QString url, QString dir)
{
	if (!isRemoteOnline()) return;

	dir = defaultWorkingDir();
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

			RepositoryItem data;
			data.local_dir = dir;
			data.local_dir.replace('\\', '/');
			data.name = makeRepositoryName(dir);

			GitPtr g = git(QString());
			setPtyUserData(QVariant::fromValue<RepositoryItem>(data));
			setPtyCondition(PtyCondition::Clone);
			setPtyProcessOk(true);
			g->clone(clone_data, getPtyProcess());
		} else if (dlg.action() == CloneDialog::Action::AddExisting) {
			addWorkingCopyDir(dir, true);
		}

		return; // done
	}
}

void BasicMainWindow::checkout()
{
	checkout(this, selectedCommitItem());
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
			dlg.setText(getLogs()[0].message);
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
				ok = g->commit_amend_m(text, sign, getPtyProcess());
			} else {
				ok = g->commit(text, sign, getPtyProcess());
			}
			if (ok) {
				setForceFetch(true);
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

void BasicMainWindow::pushSetUpstream(QString const &remote, QString const &branch)
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

Git::CommitItem const *BasicMainWindow::selectedCommitItem() const
{
	int i = selectedLogIndex();
	return commitItem(i);
}

void BasicMainWindow::deleteBranch(Git::CommitItem const *commit)
{
	if (!commit) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QStringList all_branch_names;
	QStringList current_local_branch_names;
	{
		NamedCommitList named_commits = namedCommitItems(Branches);
//		JumpDialog::sort(&named_commits);
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
			openRepository(true);
		}
	}
}

void BasicMainWindow::deleteBranch()
{
	deleteBranch(selectedCommitItem());
}

QStringList MainWindow::remoteBranches(QString const &id, QStringList *all)
{
	if (all) all->clear();

	QStringList list;

	GitPtr g = git();
	if (isValidWorkingCopy(g)) {
		NamedCommitList named_commits = namedCommitItems(Branches | Remotes);
//		JumpDialog::sort(&named_commits);
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

void MainWindow::deleteRemoteBranch(Git::CommitItem const *commit)
{
	if (!commit) return;

	GitPtr g = git();
	if (!isValidWorkingCopy(g)) return;

	QStringList all_branches;
	QStringList remote_branches = remoteBranches(commit->commit_id, &all_branches);
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

bool BasicMainWindow::askAreYouSureYouWantToRun(QString const &title, QString const &command)
{
	QString message = tr("Are you sure you want to run the following command ?");
	QString text = "%1\n\n%2";
	text = text.arg(message).arg(command);
	return QMessageBox::warning(this, title, text, QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok;
}

bool BasicMainWindow::editFile(QString const &path, QString const &title)
{
	return TextEditDialog::editFile(this, path, title);
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

void BasicMainWindow::saveRepositoryBookmark(RepositoryItem item)
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

void BasicMainWindow::setCurrentRepository(RepositoryItem const &repo, bool clear_authentication)
{
	if (clear_authentication) {
		clearAuthentication();
	}
	m->current_repo = repo;
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

	return ok;
}

NamedCommitList BasicMainWindow::namedCommitItems(int flags)
{
	NamedCommitList items;
	if (flags & Branches) {
		for (auto pair: branchMapRef()) {
			QList<Git::Branch> const &list = pair.second;
			for (Git::Branch const &b : list) {
				if (b.isHeadDetached()) continue;
				if (flags & NamedCommitFlag::Remotes) {
					// nop
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
		for (auto pair: *ptrTagMap()) {
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
	auto const &logs = getLogs();
	for (size_t i = 0; i < logs.size(); i++) {
		Git::CommitItem const &item = logs[i];
		if (item.commit_id == id) {
			return (int)i;
		}
	}
	return -1;
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

void BasicMainWindow::setLogEnabled(GitPtr g, bool f)
{
	if (f) {
		g->setLogCallback(git_callback, this);
	} else {
		g->setLogCallback(nullptr, nullptr);
	}
}

QList<Git::Tag> BasicMainWindow::findTag(QString const &id)
{
	auto it = ptrTagMap()->find(id);
	if (it != ptrTagMap()->end()) {
		return it->second;
	}
	return QList<Git::Tag>();
}

void BasicMainWindow::sshSetPassphrase(std::string const &user, std::string const &pass)
{
	m->ssh_passphrase_user = user;
	m->ssh_passphrase_pass = pass;
}

std::string BasicMainWindow::sshPassphraseUser() const
{
	return m->ssh_passphrase_user;
}

std::string BasicMainWindow::sshPassphrasePass() const
{
	return m->ssh_passphrase_pass;
}

void BasicMainWindow::httpSetAuthentication(std::string const &user, std::string const &pass)
{
	m->http_uid = user;
	m->http_pwd = pass;
}

std::string BasicMainWindow::httpAuthenticationUser() const
{
	return m->http_uid;
}

std::string BasicMainWindow::httpAuthenticationPass() const
{
	return m->http_pwd;
}

void BasicMainWindow::doGitCommand(std::function<void(GitPtr g)> const &callback)
{
	GitPtr g = git();
	if (isValidWorkingCopy(g)) {
		OverrideWaitCursor;
		callback(g);
		openRepository(false, false);
	}
}

QStringList BasicMainWindow::findGitObject(QString const &id) const
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

