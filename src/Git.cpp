#include "Git.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include <QTimer>
#include <set>
#include "joinpath.h"
#include "misc.h"
#include "LibGit2.h"
#include "GitObjectManager.h"

#define DEBUGLOG 0

struct Git::Private {
	QString git_command;
	QByteArray result;
	int process_exit_code = 0;
	QString working_repo_dir;
	QString error_message;
	callback_t callback_func = nullptr;
	void *callback_cookie = nullptr;
};

Git::Git()
	: m(new Private)
{
}

Git::Git(const Context &cx, QString const &repodir)
	: m(new Private)
{
	setGitCommand(cx.git_command);
	setWorkingRepositoryDir(repodir);
}

Git::~Git()
{
	delete m;
}

void Git::setLogCallback(callback_t func, void *cookie)
{
	m->callback_func = func;
	m->callback_cookie = cookie;
}

void Git::setWorkingRepositoryDir(QString const &repo)
{
	m->working_repo_dir = repo;
}

QString const &Git::workingRepositoryDir() const
{
	return m->working_repo_dir;
}

bool Git::isValidID(const QString &id)
{
	int zero = 0;
	int n = id.size();
	if (n > 2 && n <= GIT_ID_LENGTH) {
		ushort const *p = id.utf16();
		for (int i = 0; i < n; i++) {
			uchar c = p[i];
			if (c == '0') {
				zero++;
			} else if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
				// ok
			} else {
				return false;
			}
		}
		if (zero == GIT_ID_LENGTH) {
			return false;
		}
		return true; // ok
	}
	return false;
}

QByteArray Git::result() const
{
	return m->result;
}

void Git::setGitCommand(QString const &path)
{
	m->git_command = path;
}

QString Git::gitCommand() const
{
	Q_ASSERT(m);
	return m->git_command;
}

void Git::clearResult()
{
	m->result.clear();
	m->process_exit_code = 0;
	m->error_message = QString();
}

QString Git::resultText() const
{
	return QString::fromUtf8(result());
}

int Git::getProcessExitCode() const
{
	return m->process_exit_code;
}

bool Git::chdirexec(std::function<bool()> fn)
{
	bool ok = false;
	QDir cwd = QDir::current();
	QString dir = workingRepositoryDir();
	if (isValidWorkingCopy(dir) && QDir::setCurrent(dir)) {

		ok = fn();

		QDir::setCurrent(cwd.path());
	}
	return ok;
}

bool Git::git(const QString &arg, bool chdir, bool errout)
{
	QFileInfo info(gitCommand());
	if (!info.isExecutable()) {
		qDebug() << "Invalid git command: " << gitCommand();
		return false;
	}
#if DEBUGLOG
	qDebug() << "exec: git " << arg;
	QTime timer;
	timer.start();
#endif
	clearResult();

	auto DoIt = [&](){
		QString cmd = QString("\"%1\" ").arg(gitCommand());
		cmd += arg;

		if (m->callback_func) {
			QString prompt = "> ";
			QByteArray ba = (prompt + arg).toUtf8();
			ba.push_back('\n');
			m->callback_func(m->callback_cookie, ba.data(), (int)ba.size());
		}
//		qDebug() << cmd;
#if 1
		QProcess proc;
		proc.setReadChannel(errout ? QProcess::StandardError : QProcess::StandardOutput);
		proc.start(cmd);
		proc.waitForStarted();
		proc.closeWriteChannel();
		while (1) {
			QProcess::ProcessState s = proc.state();
			if (proc.waitForReadyRead(1)) {
				while (1) {
					char tmp[1024];
					int len = (int)proc.read(tmp, sizeof(tmp));
					if (len < 1) break;
					m->result.append(tmp, len);
					if (m->callback_func) {
						bool ok = m->callback_func(m->callback_cookie, tmp, len);
						if (!ok) {
							proc.terminate();
							m->error_message = "Interrupted by user";
							qDebug() << m->error_message;
							m->process_exit_code = -1;
							return false;
						}
					}
				}
			} else if (s == QProcess::NotRunning) {
				break;
			}
		}
		m->process_exit_code = proc.exitCode();
#else
		m->process_exit_code = misc::runCommand(cmd, &m->result);
#endif

		if (m->process_exit_code != 0) {
			m->error_message = QString::fromUtf8(proc.readAllStandardError());
#if 1//DEBUGLOG
			qDebug() << QString("Process exit code: %1").arg(getProcessExitCode());
			qDebug() << m->error_message;
#endif
		}
		return m->process_exit_code == 0;
	};

	bool ok = false;

	if (chdir) {
		ok = chdirexec(DoIt);
	} else {
		ok = DoIt();
	}

#if DEBUGLOG
	qDebug() << timer.elapsed() << "ms";
#endif

	return ok;
}

QString Git::errorMessage() const
{
	return m->error_message;
}

GitPtr Git::dup() const
{
	Git *p = new Git();
	p->m->git_command = m->git_command;
	p->m->working_repo_dir = m->working_repo_dir;
	p->m->callback_func = m->callback_func;
	p->m->callback_cookie = m->callback_cookie;
	return GitPtr(p);
}

bool Git::isValidWorkingCopy(QString const &dir)
{
	if (dir.isEmpty()) return false;
	return QDir(dir / ".git").exists();
}

bool Git::isValidWorkingCopy()
{
	return isValidWorkingCopy(workingRepositoryDir());
}

QString Git::version()
{
	git("--version", false);
	return resultText().trimmed();
}

bool Git::init()
{
	bool ok = false;
	QDir cwd = QDir::current();
	QString dir = workingRepositoryDir();
	if (QDir::setCurrent(dir)) {
		QString gitdir = dir / ".git";
		if (!QFileInfo(gitdir).isDir()) {
			git("init", false);
			if (QFileInfo(gitdir).isDir()) {
				ok = true;
			}
		}
		QDir::setCurrent(cwd.path());
	}
	return ok;
}

QString Git::rev_parse(QString const &name)
{
	QString cmd = "rev-parse %1";
	cmd = cmd.arg(name);
	if (git(cmd)) {
		return resultText().trimmed();
	}
	return QString();
}

QStringList Git::refs()
{
	QStringList lines;
	{
		QString path = workingRepositoryDir() / ".git/packed-refs";
		QFile file(path);
		if (file.open(QFile::ReadOnly)) {
			while (!file.atEnd()) {
				QByteArray ba = file.readLine();
				QString line = QString::fromLatin1(ba).trimmed();
				if (line.indexOf(" refs/") > 0) {
					lines.push_back(line);
				}
			}
		}
	}
	{
		QString refspath = workingRepositoryDir() / ".git/refs/tags";
		QDirIterator it(refspath);
		while (it.hasNext()) {
			it.next();
			QString path = it.filePath();
			if (QFileInfo(path).isFile()) {
				QString name = it.fileName();
				QFile file(path);
				if (file.open(QFile::ReadOnly)) {
					QByteArray ba = file.readAll();
					QString id = QString::fromLatin1(ba).trimmed();
					if (Git::isValidID(id)) {
						QString line = id + " refs/tags/" + name;
						lines.push_back(line);
					}
				}
			}
		}
	}
	return lines;
}

QList<Git::Tag> Git::tags()
{
	auto MidCmp = [](QString const &line, int i, char const *ptr){
		ushort const *p = line.utf16();
		for (int j = 0; ptr[j]; j++) {
			if (p[i + j] != ptr[j]) return false;
		}
		return true;
	};
	QList<Git::Tag> list;
	QStringList lines = refs();
	for (QString const &line : lines) {
		Tag tag;
		if (line.isEmpty()) continue;
		int i = line.indexOf(' ');
		if (i == GIT_ID_LENGTH) {
			tag.id = line.mid(0, i);
			if (MidCmp(line, i, " refs/tags/")) {
				tag.name = line.mid(i + 11).trimmed();
				if (tag.name.isEmpty()) continue;
				list.push_back(tag);
			}
		}
	}
	return list;
}

void Git::tag(const QString &name, const QString &id)
{
	QString cmd = "tag \"%1\" %2";
	cmd = cmd.arg(name).arg(id);
	git(cmd);
}

void Git::delete_tag(const QString &name, bool remote)
{
	QString cmd = "tag --delete \"%1\"";
	cmd = cmd.arg(name);
	git(cmd);

	if (remote) {
		QString cmd = "push --delete origin \"%1\"";
		cmd = cmd.arg(name);
		git(cmd);
	}
}

#if USE_LIBGIT2

QString Git::diffHeadToWorkingDir_()
{
	std::string dir = workingRepositoryDir().toStdString();
	LibGit2::Repository r = LibGit2::openRepository(dir);
	std::string s = r.diff_head_to_workdir();
	return QString::fromStdString(s);
}

QString Git::diff_(QString const &old_id, QString const &new_id)
{
	if (new_id.isEmpty()) {
		return diffHeadToWorkingDir_();
	} else {
		std::string dir = workingRepositoryDir().toStdString();
		LibGit2::Repository r = LibGit2::openRepository(dir);
		std::string s = r.diff2(old_id.toStdString(), new_id.toStdString());
		return QString::fromStdString(s);
	}
}

#endif

QString Git::diff(QString const &old_id, QString const &new_id)
{
#if 1
	QString cmd = "diff --full-index -a %1 %2";
	cmd = cmd.arg(old_id).arg(new_id);
	git(cmd);
	return resultText();
#else
	return diff_(old_id, new_id);
#endif
}

QList<Git::DiffRaw> Git::diff_raw(QString const &old_id, QString const &new_id)
{
	QList<DiffRaw> list;
	QString cmd = "diff --raw --abbrev=%1 %2 %3";
	cmd = cmd.arg(GIT_ID_LENGTH).arg(old_id).arg(new_id);
	git(cmd);
	QString text = resultText();
	QStringList lines = text.split('\n', QString::SkipEmptyParts);
	for (QString const &line : lines) { // raw format: e.g. ":100644 100644 08bc10d... 18f0501... M  src/MainWindow.cpp"
		DiffRaw item;
		int colon = line.indexOf(':');
		int tab = line.indexOf('\t');
		if (colon >= 0 && colon < tab) {
			QStringList header = line.mid(colon + 1, tab - colon - 1).split(' ', QString::SkipEmptyParts); // コロンとタブの間の文字列を空白で分割
			if (header.size() >= 5) {
				QStringList files = line.mid(tab + 1).split('\t', QString::SkipEmptyParts); // タブより後ろはファイルパス
				if (files.size() > 0) {
					for (QString &file : files) {
						file = Git::trimPath(file);
					}
					item.a.id = header[2];
					item.b.id = header[3];
					item.a.mode = header[0];
					item.b.mode = header[1];
					item.state = header[4];
					item.files = files;
					list.push_back(item);
				}
			}
		}
	}
	return list;
}

QString Git::diff_to_file(QString const &old_id, QString const &path)
{
#if 1
	QString cmd = "diff --full-index -a %1 -- %2";
	cmd = cmd.arg(old_id).arg(path);
	git(cmd);
	return resultText();
#else
	return diff_(old_id, new_id);
#endif
}



QString Git::getCurrentBranchName()
{
	if (isValidWorkingCopy()) {
		git("rev-parse --abbrev-ref HEAD");
		QString s = resultText().trimmed();
		if (!s.isEmpty() && !s.startsWith("fatal:") && s.indexOf(' ') < 0) {
			return s;
		}
	}
	return QString();
}

QStringList Git::getUntrackedFiles()
{
	QStringList files;
	Git::FileStatusList stats = status();
	for (FileStatus const &s : stats) {
		if (s.code() == FileStatusCode::Untracked) {
			files.push_back(s.path1());
		}
	}
	return files;
}




void Git::parseAheadBehind(QString const &s, Branch *b)
{
	ushort const *begin = s.utf16();
	ushort const *end = begin + s.size();
	ushort const *ptr = begin;

	auto NCompare = [](ushort const *a, char const *b, size_t n){
		for (size_t i = 0; i < n; i++) {
			if (*a < (unsigned char)*b) return false;
			if (*a > (unsigned char)*b) return false;
			if (!*a) break;
			a++;
			b++;
		}
		return true;
	};

	auto SkipSpace = [&](){
		while (ptr < end && QChar(*ptr).isSpace()) {
			ptr++;
		}
	};

	auto GetNumber = [&](){
		int n = 0;
		while (ptr < end && QChar(*ptr).isDigit()) {
			n = n * 10 + (*ptr - '0');
			ptr++;
		}
		return n;
	};

	if (*ptr == '[') {
		ptr++;
		while (*ptr) {
			if (*ptr == ',' || QChar(*ptr).isSpace()) {
				ptr++;
			} else if (NCompare(ptr, "ahead ", 6)) {
				ptr += 6;
				SkipSpace();
				b->ahead = GetNumber();
			} else if (NCompare(ptr, "behind ", 7)) {
				ptr += 7;
				SkipSpace();
				b->behind = GetNumber();
			} else {
				break;
			}
		}
	}
}

QList<Git::Branch> Git::branches_()
{
	QList<Branch> branches;
	git(QString("branch -v -a --abbrev=%1").arg(GIT_ID_LENGTH));
	QString s = resultText();
#if DEBUGLOG
	qDebug() << s;
#endif

	QStringList lines = misc::splitLines(s);
	for (QString const &line : lines) {
		if (line.size() > 2) {
			QString name;
			QString text = line.mid(2);
			int pos = 0;
			if (text.startsWith('(')) {
				pos = text.indexOf(')');
				if (pos > 1) {
					name = text.mid(1, pos - 1);
					pos++;
				}
			} else {
				while (pos < text.size() && !QChar::isSpace(text.utf16()[pos])) {
					pos++;
				}
				name = text.mid(0, pos);
			}
			if (!name.isEmpty()) {
				while (pos < text.size() && QChar::isSpace(text.utf16()[pos])) {
					pos++;
				}
				text = text.mid(pos);

				Branch b;

				if (name.startsWith("HEAD detached at ")) {
					b.flags |= Branch::HeadDetached;
					name = name.mid(17);
				}

				if (name.startsWith("origin/")) {
					name = name.mid(7);
				}

				b.name = name;

				if (text.startsWith("-> ")) {
					b.id = ">" + text.mid(3);
				} else {
					int i = text.indexOf(' ');
					if (i == GIT_ID_LENGTH) {
						b.id = text.mid(0, GIT_ID_LENGTH);
					}
					while (i < text.size() && QChar::isSpace(text.utf16()[i])) {
						i++;
					}
					text = text.mid(i);
					parseAheadBehind(text, &b);
				}

				if (line.startsWith('*')) {
					b.flags |= Branch::Current;
				}

				branches.push_back(b);
			}
		}
	}
	for (int i = 0; i < branches.size(); i++) {
		Branch *b = &branches[i];
		if (b->id.startsWith('>')) {
			QString name = b->id.mid(1);
			if (name.startsWith("origin/")) {
				name = name.mid(7);
			}
			for (int j = 0; j < branches.size(); j++) {
				if (j != i) {
					if (branches[j].name == name) {
						branches[i].id = branches[j].id;
						break;
					}
				}
			}
		}
	}
	return branches;
}

QList<Git::Branch> Git::branches()
{
#if 1
	return branches_();
#else
	std::string dir = workingRepositoryDir().toStdString();
	LibGit2::Repository r = LibGit2::openRepository(dir);
	return r.branches();
#endif
}

Git::CommitItemList Git::log_all(QString const &id, int maxcount)
{
	CommitItemList items;
	QString text;

	QString cmd = "log --pretty=format:\"commit:%H#parent:%P#author:%an#mail:%ae#date:%ci##%s\" --all -%1 %2";
	cmd = cmd.arg(maxcount).arg(id);
	git(cmd);
	if (getProcessExitCode() == 0) {
		text = resultText().trimmed();
		QStringList lines = misc::splitLines(text);
		for (QString const &line : lines) {
			int i = line.indexOf("##");
			if (i > 0) {
				Git::CommitItem item;
				item.message = line.mid(i + 2);
				QStringList atts = line.mid(0, i).split('#');
				for (QString const &s : atts) {
					int j = s.indexOf(':');
					if (j > 0) {
						QString key = s.mid(0, j);
						QString val = s.mid(j + 1);
						if (key == "commit") {
							item.commit_id = val;
						} else if (key == "parent") {
							item.parent_ids = val.split(' ', QString::SkipEmptyParts);
						} else if (key == "author") {
							item.author = val;
						} else if (key == "mail") {
							item.mail = val;
						} else if (key == "date") {
							item.commit_date = QDateTime::fromString(val, Qt::ISODate).toLocalTime();
						} else if (key == "debug") {
						}
					}
				}
//				if (item.commit_date < limit_time) {
//					break;
//				}
				items.push_back(item);
			}
		}
	}
	return items;
}

Git::CommitItemList Git::log(int maxcount)
{
#if 1
	return log_all(QString(), maxcount);
#else
	std::string dir = workingRepositoryDir().toStdString();
	LibGit2::Repository r = LibGit2::openRepository(dir);
	return r.log(maxcount);
#endif
}

Git::CloneData Git::preclone(QString const &url, QString const &path)
{
	CloneData d;
	d.url = url;
	if (path.endsWith('/') || path.endsWith('\\')) {
		auto GitBaseName = [](QString location){
			int i = location.lastIndexOf('/');
			int j = location.lastIndexOf('\\');
			if (i < j) i = j;
			i = i < 0 ? 0 : (i + 1);
			j = location.size();
			if (location.endsWith(".git")) {
				j -= 4;
			}
			if (i < j) {
				location = location.mid(i, j - i);
			}
			return location;
		};
		d.basedir = path;
		d.subdir = GitBaseName(url);
	} else {
		QFileInfo info(path);
		d.basedir = info.dir().path();
		d.subdir = info.fileName();
	}
	return d;
}

bool Git::clone(CloneData const &data)
{
	m->working_repo_dir = misc::normalizePathSeparator(data.basedir / data.subdir);

	bool ok = false;
	QDir cwd = QDir::current();
	if (QDir::setCurrent(data.basedir)) {

		QString cmd = "clone --progress \"%1\" \"%2\"";
		cmd = cmd.arg(data.url).arg(data.subdir);
		ok = git(cmd, false, true);

		QDir::setCurrent(cwd.path());
	}
	return ok;
}

QString Git::encodeQuotedText(const QString &str)
{
	std::vector<ushort> vec;
	ushort const *begin = str.utf16();
	ushort const *end = begin + str.size();
	ushort const *ptr = begin;
	vec.push_back('\"');
	while (ptr < end) {
		ushort c = *ptr;
		ptr++;
		if (c == '\"') { // triple quotes
			vec.push_back(c);
			vec.push_back(c);
			vec.push_back(c);
		} else {
			vec.push_back(c);
		}
	}
	vec.push_back('\"');
	return QString::fromUtf16(&vec[0], vec.size());
}

bool Git::commit_(QString const &msg, bool amend)
{
	QString cmd = "commit";
	if (amend) {
		cmd += " --amend";
	}

	QString text = msg.trimmed();
	if (text.isEmpty()) {
		text = "no message";
	}
	text = encodeQuotedText(text);
	cmd += QString(" -m %1").arg(text);

	return git(cmd);
}

bool Git::commit(QString const &text)
{
#if 1
	return commit_(text, false);
#else
	LibGit2::Repository r = LibGit2::openRepository(workingRepositoryDir().toStdString());
	return r.commit(text.toStdString());
#endif
}

bool Git::commit_amend_m(const QString &text)
{
	return commit_(text, true);
}

bool Git::revert(const QString &id)
{
	QString cmd = "revert %1";
	cmd = cmd.arg(id);
	return git(cmd);

}

void Git::push_u(QString const &remote, QString const &branch)
{
	QString cmd = "push -u %1 %2";
	git(cmd.arg(remote).arg(branch));
}

void Git::push_u_origin_master()
{
	QString cmd = "push -u origin master";
	git(cmd);
}

void Git::push_(bool tags)
{
	QString cmd = "push";
	if (tags) {
		cmd += " --tags";
	}
	git(cmd);
}

void Git::push(bool tags)
{
#if 1
	push_(tags);
#else
	LibGit2::Repository r = LibGit2::openRepository(workingRepositoryDir().toStdString());
	r.push_();
#endif
}


Git::FileStatusList Git::status_()
{
	FileStatusList files;
	if (git("status -s -u --porcelain")) {
		QString text = resultText();
		QStringList lines = misc::splitLines(text);
		for (QString const &line : lines) {
			if (line.size() > 3) {
				FileStatus s(line);
				if (s.code() != FileStatusCode::Unknown) {
					files.push_back(s);
				}
			}
		}
	}
	return files;
}

Git::FileStatusList Git::status()
{
#if 1
	return status_();
#else
	std::string repodir = workingRepositoryDir().toStdString();
	return LibGit2::status(repodir);
#endif
}

QString Git::objectType(QString const &id)
{
	git("cat-file -t " + id);
	return resultText().trimmed();
}

QByteArray Git::cat_file_(QString const &id)
{
	qDebug() << "cat_file: " << id;
#if 1
	git("cat-file -p " + id);
	return m->result;
#else
	std::string dir = workingRepositoryDir().toStdString();
	LibGit2::Repository r = LibGit2::openRepository(dir);
	return r.cat_file(id.toStdString());
#endif
}

bool Git::cat_file(QString const &id, QByteArray *out)
{
	if (isValidID(id)) {
		*out = cat_file_(id);
		return true;
	}
	return false;
}

void Git::resetFile(const QString &path)
{
#if 1
	git("checkout -- " + path);
#else
	std::string dir = workingRepositoryDir().toStdString();
	LibGit2::Repository r = LibGit2::openRepository(dir);
	r.revert_a_file(path.toStdString());
#endif
}

void Git::resetAllFiles()
{
	git("reset --hard HEAD");
}

void Git::removeFile(const QString &path)
{
	git("rm " + path);
}

void Git::stage(QString const &path)
{
	git("add " + path);
}

void Git::stage(QStringList const &paths)
{
	QString cmd = "add";
	for (QString const &path : paths) {
		cmd += ' ';
		cmd += '\"';
		cmd += path;
		cmd += '\"';
	}
	git(cmd);
}

void Git::unstage(QString const &path)
{
	QString cmd = "reset HEAD \"%1\"";
	git(cmd.arg(path));
}

void Git::unstage(QStringList const &paths)
{
	QString cmd = "reset HEAD";
	for (QString const &path : paths) {
		cmd += " \"";
		cmd += path;
		cmd += '\"';
	}
	git(cmd);
}

void Git::pull()
{
	git("pull");
}

void Git::fetch()
{
	git("fetch");
}

QStringList Git::make_branch_list_()
{
	QStringList list;
	QStringList l = misc::splitLines(resultText());
	for (QString const &s : l) {
		if (s.startsWith("* ")) list.push_back(s.mid(2));
		if (s.startsWith("  ")) list.push_back(s.mid(2));
	}
	return list;
}

void Git::createBranch(QString const &name)
{
	git("branch " + name);
}

void Git::checkoutBranch(QString const &name)
{
	git("checkout " + name);
}

void Git::cherrypick(QString const &name)
{
	git("cherry-pick " + name);
}

void Git::mergeBranch(QString const &name)
{
	git("merge " + name);
}

QStringList Git::getRemotes()
{
	QStringList ret;
	git("remote");
	QStringList lines = misc::splitLines(resultText());
	for (QString const &line: lines) {
		if (!line.isEmpty()) {
			ret.push_back(line);
		}
	}
	return ret;
}

Git::User Git::getUser(GetUser purpose)
{
	User user;
	bool global = purpose == Git::GetUserGlobal;
	bool local = purpose == Git::GetUserLocal;
	QString arg1;
	if (global) arg1 = "--global";
	if (local) arg1 = "--local";
	bool chdir = !global;
	if (git(QString("config %1 user.name").arg(arg1), chdir)) {
		user.name = resultText().trimmed();
	}
	if (git(QString("config %1 user.email").arg(arg1), chdir)) {
		user.email = resultText().trimmed();
	}
	return user;
}

void Git::setUser(const User &user, bool global)
{
	bool chdir = !global;
	git(QString("config %1 user.name %2").arg(global ? "--global" : "").arg(encodeQuotedText(user.name)), chdir);
	git(QString("config %1 user.email %2").arg(global ? "--global" : "").arg(encodeQuotedText(user.email)), chdir);
}

bool Git::reset_head1()
{
	return git("reset HEAD~1");
}

bool Git::rm_cached(QString const &file)
{
	QString cmd = "rm --cached \"%1\"";
	return git(cmd.arg(file));
}

void Git::getRemoteURLs(QList<Remote> *out)
{
	out->clear();
	git("remote -v");
	QStringList lines = misc::splitLines(resultText());
	for (QString const &line : lines) {
		int i = line.indexOf('\t');
		int j = line.indexOf(" (");
		if (i > 0 && i < j) {
			Remote r;
			r.remote = line.mid(0, i);
			r.url = line.mid(i + 1, j - i - 1);
			r.purpose = line.mid(j + 1);
			if (r.purpose.startsWith('(') && r.purpose.endsWith(')')) {
				r.purpose = r.purpose.mid(1, r.purpose.size() - 2);
			}
			out->push_back(r);
		}
	}
}

void Git::setRemoteURL(QString const &remote, QString const &url)
{
	QString cmd = "remote set-url %1 %2";
	cmd = cmd.arg(encodeQuotedText(remote)).arg(encodeQuotedText(url));
	git(cmd);
}

void Git::addRemoteURL(QString const &remote, QString const &url)
{
	QString cmd = "remote add %1 %2";
	cmd = cmd.arg(encodeQuotedText(remote)).arg(encodeQuotedText(url));
	git(cmd);
}

// Git::FileStatus

QString Git::trimPath(QString const &s)
{
	ushort const *begin = s.utf16();
	ushort const *end = begin + s.size();
	ushort const *left = begin;
	ushort const *right = end;
	while (left < right && QChar(*left).isSpace()) left++;
	while (left < right && QChar(right[-1]).isSpace()) right--;
	if (left + 1 < right && *left == '\"' && right[-1] == '\"') { // if quoted ?
		left++;
		right--;
		QByteArray ba;
		ushort const *ptr = left;
		while (ptr < right) {
			ushort c = *ptr;
			ptr++;
			if (c == '\\') {
				c = 0;
				while (ptr < right && QChar(*ptr).isDigit()) { // decode \oct
					c = c * 8 + (*ptr - '0');
					ptr++;
				}
			}
			ba.push_back(c);
		}
		return QString::fromUtf8(ba);
	}
	if (left == begin && right == end) return s;
	return QString::fromUtf16(left, right - left);
}

Git::FileStatusCode Git::FileStatus::parseFileStatusCode(char x, char y)
{
	if (x == ' ' && (y == 'M' || y == 'D')) return FileStatusCode::NotUpdated;
	if (x == 'M' && (y == 'M' || y == 'D' || y == ' ')) return FileStatusCode::UpdatedInIndex;
	if (x == 'A' && (y == 'M' || y == 'D' || y == ' ')) return FileStatusCode::AddedToIndex;
	if (x == 'D' && (y == 'M' || y == ' ')) return FileStatusCode::DeletedFromIndex;
	if (x == 'R' && (y == 'M' || y == 'D' || y == ' ')) return FileStatusCode::RenamedInIndex;
	if (x == 'C' && (y == 'M' || y == 'D' || y == ' ')) return FileStatusCode::CopiedInIndex;
	if (x == 'C' && (y == 'M' || y == 'D' || y == ' ')) return FileStatusCode::CopiedInIndex;
	if (x == 'D' && y == 'D') return FileStatusCode::Unmerged_BothDeleted;
	if (x == 'A' && y == 'U') return FileStatusCode::Unmerged_AddedByUs;
	if (x == 'U' && y == 'D') return FileStatusCode::Unmerged_DeletedByThem;
	if (x == 'U' && y == 'A') return FileStatusCode::Unmerged_AddedByThem;
	if (x == 'D' && y == 'U') return FileStatusCode::Unmerged_DeletedByUs;
	if (x == 'A' && y == 'A') return FileStatusCode::Unmerged_BothAdded;
	if (x == 'U' && y == 'U') return FileStatusCode::Unmerged_BothModified;
	if (x == '?' && y == '?') return FileStatusCode::Untracked;
	if (x == '!' && y == '!') return FileStatusCode::Ignored;
	return FileStatusCode::Unknown;
}

void Git::FileStatus::parse(const QString &text)
{
	data = Data();
	if (text.size() > 3) {
		ushort const *p = text.utf16();
		if (p[2] == ' ') {
			data.code_x = p[0];
			data.code_y = p[1];

			int i = text.indexOf(" -> ", 3);
			if (i > 3) {
				data.rawpath1 = text.mid(3, i - 3);
				data.rawpath2 = text.mid(i + 4);
				data.path1 = trimPath(data.rawpath1);
				data.path2 = trimPath(data.rawpath2);
			} else {
				data.rawpath1 = text.mid(3);
				data.path1 = trimPath(data.rawpath1);
				data.path2 = QString();
			}

			data.code = parseFileStatusCode(data.code_x, data.code_y);
		}
	}
}

// Diff

void Git::Diff::makeForSingleFile(Git::Diff *diff, const QString &id, const QString &path, QString const &mode)
{
	QString zeros(GIT_ID_LENGTH, '0');
	diff->diff = QString("diff --git a/%1 b/%2").arg(path).arg(path);
	diff->index = QString("index %1..%2 %3").arg(zeros).arg(id).arg(0);
	diff->blob.a_id = zeros;
	diff->blob.b_id = id;
	diff->path = path;
	diff->mode = mode;
	diff->type = Git::Diff::Type::Added;
}

