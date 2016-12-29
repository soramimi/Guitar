#include "Git.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QProcess>
#include <QTimer>
#include <set>
#include "joinpath.h"
#include "misc.h"
#include "LibGit2.h"

#define DEBUGLOG 0

struct Git::Private {
	QString git_command;
	QByteArray result;
	int process_exit_code = 0;
	QString working_repo_dir;
	QString error_message;
};

Git::Git(const Context &cx, QString const &repodir)
{
	pv = new Private();
	setGitCommand(cx.git_command);
	setWorkingRepositoryDir(repodir);
}

Git::~Git()
{
	delete pv;
}

void Git::setWorkingRepositoryDir(QString const &repo)
{
	pv->working_repo_dir = repo;
}

QString const &Git::workingRepositoryDir() const
{
	return pv->working_repo_dir;
}

bool Git::isValidID(const QString &s)
{
	int n = s.size();
	if (n > 0) {
		ushort const *p = s.utf16();
		for (int i = 0; i < n; i++) {
			uchar c = QChar::toUpper(p[i]);
			if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) {
				// ok
			} else {
				return false;
			}
		}
		return true;
	}
	return false;
}

QByteArray Git::result() const
{
	return pv->result;
}

void Git::setGitCommand(QString const &path)
{
	pv->git_command = path;
}

QString Git::gitCommand() const
{
	return pv->git_command;
}

void Git::clearResult()
{
	pv->result.clear();
	pv->process_exit_code = 0;
	pv->error_message = QString();
}

QString Git::resultText() const
{
	return QString::fromUtf8(result());
}

int Git::getProcessExitCode() const
{
	return pv->process_exit_code;
}

bool Git::chdirexec(std::function<bool()> fn)
{
	bool ok = false;
	QDir cwd = QDir::current();
	if (QDir::setCurrent(workingRepositoryDir())) {

		ok = fn();

		QDir::setCurrent(cwd.path());
	}
	return ok;
}

bool Git::git(const QString &arg, bool chdir)
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

	auto Do = [&](){
		QString cmd = QString("\"%1\" ").arg(gitCommand());
		cmd += arg;

		QProcess proc;
		proc.start(cmd);
		proc.closeWriteChannel();
		proc.setReadChannel(QProcess::StandardOutput);
		while (1) {
			QProcess::ProcessState s = proc.state();
			if (proc.waitForReadyRead(1)) {
				while (1) {
					char tmp[1024];
					qint64 len = proc.read(tmp, sizeof(tmp));
					if (len < 1) break;
					pv->result.append(tmp, len);
				}
			} else if (s == QProcess::NotRunning) {
				break;
			}
		}
		pv->process_exit_code = proc.exitCode();

//		pv->process_exit_code = misc::qtRunCommand(cmd, &pv->result);

#if DEBUGLOG
		qDebug() << QString("Process exit code: %1").arg(getProcessExitCode());
		if (pv->process_exit_code != 0) {
			pv->error_message = QString::fromUtf8(proc.readAllStandardError());
			qDebug() << pv->error_message;
		}
#endif
		return pv->process_exit_code == 0;
	};

	bool ok = false;

	if (chdir) {
		ok = chdirexec(Do);
	} else {
		ok = Do();
	}

#if DEBUGLOG
	qDebug() << timer.elapsed() << "ms";
#endif

	return ok;
}

QString Git::errorMessage() const
{
	return pv->error_message;
}

GitPtr Git::dup() const
{
	Git *p = new Git();
	p->pv = new Private(*pv);
	return GitPtr(p);
}

bool Git::isValidWorkingCopy(QString const &dir)
{
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

QString Git::rev_parse(QString const &name)
{
	QString cmd = "rev-parse %1";
	cmd = cmd.arg(name);
	git(cmd);
	return resultText().trimmed();
}

QList<Git::Tag> Git::tags()
{
	QList<Git::Tag> list;
	git("tag");
	QStringList lines = misc::splitLines(resultText());
	for (QString const &line : lines) {
		Tag tag;
		if (line.isEmpty()) continue;
		tag.name = line.trimmed();
		if (tag.name.isEmpty()) continue;
		tag.id = rev_parse(tag.name);
		list.push_back(tag);
	}
	return list;
}

void Git::tag(const QString &name)
{
	QString cmd = "tag \"%1\"";
	cmd = cmd.arg(name);
	git(cmd);
}

void Git::delete_tag(const QString &name, bool remote)
{
	QString cmd = "tag --delete \"%1\"";
	cmd = cmd.arg(name);
	git(cmd);
	{
		QString cmd = "push --delete origin \"%1\"";
		cmd = cmd.arg(name);
		git(cmd);
	}
}

QString Git::rev_parse_HEAD()
{
	return rev_parse("HEAD");
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
	if (old_id.isEmpty() && new_id == "HEAD") {
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
	git("branch -v -a --abbrev=40");
	QString s = resultText();
#if DEBUGLOG
	qDebug() << s;
#endif
	ushort const *begin = s.utf16();
	ushort const *end = begin + s.size();
	ushort const *ptr = begin;
	while (ptr + 2 < end) {
		ushort attr = *ptr;
		ptr += 2;
		QStringList list;
		ushort const *left = ptr;
		while (1) {
			ushort c = 0;
			if (ptr < end) c = *ptr;
			if (c == 0 || QChar::isSpace(c)) {
				if (left < ptr) {
					list.push_back(QString::fromUtf16(left, ptr - left));
					if (list.size() == 2) {
						while (ptr < end && QChar(*ptr).isSpace()) ptr++;
						left = ptr;
						while (ptr < end) {
							ushort c = *ptr;
							ptr++;
							if (c == '\n' || c == '\r') {
								list.push_back(QString::fromUtf16(left, ptr - left));
								break;
							}
						}
						c = 0;
					}
				}
				if (c == 0) break;
				ptr++;
				if (c == '\n' || c == '\r') break;
				left = ptr;
			} else {
				ptr++;
			}
		}
		if (list.size() >= 2) {
			Branch b;
			b.name = list[0];
			if (list[1].size() == 40) {
				b.id = list[1];
				if (list.size() > 2) {
					parseAheadBehind(list[2], &b);
				}
			} else if (list[1] == "->") {
				if (list.size() >= 3) {
					b.id = ">" + list[2];
				}
			}
			if (attr == '*') {
				b.flags |= Branch::Current;
			}
			branches.push_back(b);
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

Git::CommitItemList Git::log_all(QString const &id, int maxcount, QDateTime limit_time)
{
	CommitItemList items;
	QString text;
#if 1
	QString cmd = "log --pretty=format:\"commit:%H#parent:%P#author:%an#mail:%ae#date:%ci##%s\" --all -%1 %2";
	cmd = cmd.arg(maxcount).arg(id);
	git(cmd);
	text = resultText().trimmed();
	if (0) {
		QFile file("test.log");
		if (file.open(QFile::WriteOnly)) {
			file.write(text.toUtf8());
		}
	}
#else
	{
		QFile file("test.log");
		if (file.open(QFile::ReadOnly)) {
			text = QString::fromUtf8(file.readAll());
		}
	}
#endif
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
			if (item.commit_date < limit_time) {
				break;
			}
			items.push_back(item);
		}
	}
	return std::move(items);
}

Git::CommitItemList Git::log(int maxcount, QDateTime limit_time)
{
#if 1
	return log_all(QString(), maxcount, limit_time);
#else
	std::string dir = workingRepositoryDir().toStdString();
	LibGit2::Repository r = LibGit2::openRepository(dir);
	return r.log(maxcount);
#endif
}

bool Git::clone(QString const &location, QString const &path)
{
	QString basedir;
	QString subdir;
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
		basedir = path;
		subdir = GitBaseName(location);
	} else {
		QFileInfo info(path);
		basedir = info.dir().path();
		subdir = info.fileName();
	}

	pv->working_repo_dir = misc::normalizePathSeparator(basedir / subdir);

	bool ok = false;
	QDir cwd = QDir::current();
	if (QDir::setCurrent(basedir)) {

		QString cmd = "clone \"%1\" \"%2\"";
		cmd = cmd.arg(location).arg(subdir);
		ok = git(cmd, false);

		QDir::setCurrent(cwd.path());
	}
	return ok;
}

bool Git::commit_(QString const &msg, bool amend)
{
	QString cmd = "commit";
	if (amend) {
		cmd += " --amend";
	}
	int n = 0;
	QString text = msg.trimmed();
	if (!text.isEmpty()) {
		QStringList lines = misc::splitLines(text);
		for (QString const &line : lines) {
			QString s = line.trimmed();
			s = s.replace('|', ' ');
			s = s.replace('<', ' ');
			s = s.replace('>', ' ');
			s = s.replace('\"', '\'');
			if (n > 0 || !s.isEmpty()) {
				n++;
			}
			cmd += QString(" -m \"%1\"").arg(s);
		}
	}
	if (n == 0) {
		cmd += " -m \"no message\"";
	}
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
	return std::move(files);
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

QByteArray Git::cat_file_(QString const &id)
{
#if 1
	git("cat-file -p " + id);
	return pv->result;
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

void Git::revertFile(const QString &path)
{
#if 1
	git("checkout -- " + path);
#else
	std::string dir = workingRepositoryDir().toStdString();
	LibGit2::Repository r = LibGit2::openRepository(dir);
	r.revert_a_file(path.toStdString());
#endif
}

void Git::revertAllFiles()
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
		cmd += " \"";
		cmd += path;
		cmd += "\"";
	}
	git(cmd);
}

void Git::unstage(QString const &path)
{
	git("reset HEAD " + path);
}

void Git::unstage(QStringList const &paths)
{
	QString cmd = "reset HEAD";
	for (QString const &path : paths) {
		cmd += ' ';
		cmd += path;
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

void Git::mergeBranch(QString const &name)
{
	git("merge " + name);

}

void Git::test()
{
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

void Git::FileStatus::parse(const QString &text)
{
	if (text.size() > 3) {
		ushort const *p = text.utf16();
		if (p[2] == ' ') {
			data.code_x = p[0];
			data.code_y = p[1];

			int i = text.indexOf(" -> ", 3);
			if (i > 3) {
				data.path1 = trimPath(text.mid(3, i - 3));
				data.path2 = trimPath(text.mid(i + 4));
			} else {
				data.path1 = trimPath(text.mid(3));
				data.path2 = QString();
			}

			data.code = parseFileStatusCode(data.code_x, data.code_y);
		}
	}
}

//
