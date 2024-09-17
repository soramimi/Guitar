
#include "Git.h"
#include "ApplicationGlobal.h"
#include "GitObjectManager.h"
#include "MainWindow.h"
#include "MyProcess.h"
#include "common/joinpath.h"
#include "common/misc.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include <QThread>
#include <QTimer>
#include <optional>
#include <thread>

Git::CommitID::CommitID()
{
	
}

Git::CommitID::CommitID(const QString &qid)
{
	assign(qid);
}

Git::CommitID::CommitID(const char *id)
{
	assign(id);
}

void Git::CommitItem::setParents(const QStringList &list)
{
	parent_ids.clear();
	for (QString const &id : list) {
		parent_ids.append(Git::CommitID(id));
	}
}

void Git::CommitID::assign(const QString &qid)
{
	if (qid.isEmpty()) {
		valid = false;
	} else {
		valid = true;
		if (qid.size() == GIT_ID_LENGTH) {
			char tmp[3];
			tmp[2] = 0;
			for (int i = 0; i < GIT_ID_LENGTH / 2; i++) {
				unsigned char c = qid[i * 2 + 0].toLatin1();
				unsigned char d = qid[i * 2 + 1].toLatin1();
				if (std::isxdigit(c) && std::isxdigit(d)) {
					tmp[0] = c;
					tmp[1] = d;
					id[i] = strtol(tmp, nullptr, 16);
				} else {
					valid = false;
					break;
				}
			}
		}
	}
}

QString Git::CommitID::toQString(int maxlen) const
{
	if (valid) {
		char tmp[GIT_ID_LENGTH + 1];
		for (int i = 0; i < GIT_ID_LENGTH / 2; i++) {
			sprintf(tmp + i * 2, "%02x", id[i]);
		}
		if (maxlen < 0 || maxlen > GIT_ID_LENGTH) {
			maxlen = GIT_ID_LENGTH;
		}
		tmp[maxlen] = 0;
		return QString::fromLatin1(tmp, maxlen);
	}
	return {};
}

bool Git::CommitID::isValid() const
{
	if (!valid) return false;
	uint8_t c = 0;
	for (std::size_t i = 0; i < sizeof(id); i++) {
		c |= id[i];
	}
	return c != 0; // すべて0ならfalse
}


// using callback_t = Git::callback_t;

struct Git::Private {
	struct Info {
		QString git_command;
		QString working_repo_dir;
		QString submodule_path;
		std::function<bool (void *, const char *, int)> fn_log_writer_callback;
		void *callback_cookie = nullptr;
	};
	Info info;
	QString ssh_command;// = "C:/Program Files/Git/usr/bin/ssh.exe";
	QString ssh_key_override;// = "C:/a/id_rsa";
	std::vector<char> result;
	ProcessStatus exit_status;
};

Git::Git()
	: m(new Private)
{
}

Git::Git(const Context &cx, QString const &repodir, const QString &submodpath, const QString &sshkey)
	: m(new Private)
{
	setGitCommand(cx.git_command, cx.ssh_command);
	setWorkingRepositoryDir(repodir, submodpath, sshkey);
}

Git::~Git()
{
	delete m;
}

void Git::setLogCallback(std::function<bool (void *, const char *, int)> func, void *cookie)
{
	m->info.fn_log_writer_callback = func;
	m->info.callback_cookie = cookie;
}

void Git::setWorkingRepositoryDir(QString const &repo, const QString &submodpath, QString const &sshkey)
{
	m->info.working_repo_dir = repo;
	m->info.submodule_path = submodpath;
	m->ssh_key_override = sshkey;
}

QString Git::workingDir() const
{
	QString dir = m->info.working_repo_dir;
	if (!m->info.submodule_path.isEmpty()) {
		dir = dir / m->info.submodule_path;
	}
	return dir;
}

QString const &Git::sshKey() const
{
	return m->ssh_key_override;
}

void Git::setSshKey(QString const &sshkey) const
{
	m->ssh_key_override = sshkey;
}

bool Git::isValidID(QString const &id)
{
	int zero = 0;
	int n = id.size();
	if (n >= 4 && n <= GIT_ID_LENGTH) {
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
		if (zero == GIT_ID_LENGTH) { // 全部 0 の時は false を返す
			return false;
		}
		return true; // ok
	}
	return false;
}

QString Git::status()
{
	git("status");
	return resultQString();
}

QByteArray Git::toQByteArray() const
{
	if (m->result.empty()) return QByteArray();
	return QByteArray(&m->result[0], m->result.size());
}

std::string Git::resultStdString() const
{
	auto const &v = m->result;
	if (v.empty()) return {};
	return std::string(v.begin(), v.end());
}

QString Git::resultQString() const
{
	return QString::fromUtf8(toQByteArray());
}

void Git::setGitCommand(QString const &gitcmd, QString const &sshcmd)
{
	m->info.git_command = gitcmd;
	m->ssh_command = sshcmd;
}

QString Git::gitCommand() const
{
	Q_ASSERT(m);
	return m->info.git_command;
}

void Git::clearResult()
{
	m->result.clear();
	m->exit_status.exit_code = 0;
	m->exit_status.error_message = QString();
}

QString Git::errorMessage() const
{
	return m->exit_status.error_message;
}

int Git::getProcessExitCode() const
{
	return m->exit_status.exit_code;
}

bool Git::chdirexec(std::function<bool()> const &fn)
{
	bool ok = false;
	QString cwd = QDir::currentPath();
	QString dir = workingDir();
	if (QDir::setCurrent(dir)) {

		ok = fn();

		QDir::setCurrent(cwd);
	}
	return ok;
}

bool Git::git(QString const &arg, Option const &opt, bool debug_)
{
	if (debug_) return false;
	// qDebug() << "git: " << arg;
	QFileInfo info(gitCommand());
	if (!info.isExecutable()) {
		qDebug() << "Invalid git command: " << gitCommand();
		return false;
	}

	clearResult();

	QString env;
	if (m->ssh_command.isEmpty() || m->ssh_key_override.isEmpty()) {
		// nop
	} else {
		if (m->ssh_command.indexOf('\"') >= 0) return false;
		if (m->ssh_key_override.indexOf('\"') >= 0) return false;
		if (!QFileInfo(m->ssh_command).isExecutable()) return false;
		env = QString("GIT_SSH_COMMAND=\"%1\" -i \"%2\" ").arg(m->ssh_command).arg(m->ssh_key_override);
	}

	auto DoIt = [&](){
		QString cmd;
#ifdef _WIN32
		cmd = opt.prefix;
#else

#endif
		cmd += QString("\"%1\" --no-pager ").arg(gitCommand());
		cmd += arg;

		if (opt.log && m->info.fn_log_writer_callback) {
			QByteArray ba;
			ba.append("> git ");
			ba.append(arg.toUtf8());
			ba.append('\n');
			m->info.fn_log_writer_callback(m->info.callback_cookie, ba.data(), (int)ba.size());
		}

		if (opt.pty) {
			opt.pty->start(cmd, env);
			m->exit_status.exit_code = 0; // バックグラウンドで実行を継続するけど、とりあえず成功したことにしておく
		} else {
			Process proc;
			proc.start(cmd, false);
			m->exit_status.exit_code = proc.wait();

			if (opt.errout) {
				m->result = proc.errbytes;
			} else {
				if (!proc.errbytes.empty()) {
					qDebug() << proc.errstring();
				}
				m->result = proc.outbytes;
			}
			m->exit_status.error_message = proc.errstring();
		}

		return m->exit_status.exit_code == 0;
	};

	bool ok = false;

	if (opt.chdir) {
		if (opt.pty) {
			opt.pty->setChangeDir(workingDir());
			ok = DoIt();
		} else {
			ok = chdirexec(DoIt);
		}
	} else {
		ok = DoIt();
	}

	// qDebug() << _timer.elapsed() << "ms";

	return ok;
}

GitPtr Git::dup() const
{
	Git *p = new Git();
	p->m->info = m->info;
	return GitPtr(p);
}

bool Git::isValidWorkingCopy(QString const &dir)
{
	if (QFileInfo(dir).isDir()) {
		QString git = dir / ".git";
		QFileInfo info(git);
		if (info.isFile()) { // submodule?
			QFile file(git);
			if (file.open(QFile::ReadOnly)) {
				QString line = QString::fromUtf8(file.readLine());
				if (line.startsWith("gitdir:")) {
					return true;
				}
			}
		} else if (info.isDir()) { // regular dir
			if (QFileInfo(git).isDir()) {
				if (QFileInfo(git / "config").isFile()) { // git repository
					return true;
				}
			}
		}
	}
	return false;
}

bool Git::isValidWorkingCopy() const
{
	return isValidWorkingCopy(workingDir());
}

QString Git::version()
{
	git_nochdir("--version", nullptr);
	return resultQString().trimmed();
}

bool Git::init()
{
	bool ok = false;
	QDir cwd = QDir::current();
	QString dir = workingDir();
	if (QDir::setCurrent(dir)) {
		QString gitdir = dir / ".git";
		if (!QFileInfo(gitdir).isDir()) {
			git_nochdir("init", nullptr);
			if (QFileInfo(gitdir).isDir()) {
				ok = true;
			}
		}
		QDir::setCurrent(cwd.path());
	}
	return ok;
}

Git::CommitID Git::rev_parse(QString const &name)
{
	QString cmd = "rev-parse %1";
	cmd = cmd.arg(name);
	if (git(cmd)) {
		return Git::CommitID(resultQString().trimmed());
	}
	return {};
}

QList<Git::Tag> Git::tags()
{
#if 0
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
#else
	return tags2();
#endif
}

QList<Git::Tag> Git::tags2()
{
	QList<Tag> list;
	git("show-ref");
	QStringList lines = misc::splitLines(resultQString());
	for (QString const &line : lines) {
		QStringList l = misc::splitWords(line);
		if (l.size() >= 2) {
			if (isValidID(l[0]) && l[1].startsWith("refs/tags/")) {
				Tag t;
				t.name = l[1].mid(10);
				t.id = Git::CommitID(l[0]);
				list.push_back(t);
			}
		}
	}
	return list;
}

bool Git::tag(QString const &name, CommitID const &id)
{
	QString cmd = "tag \"%1\" %2";
	cmd = cmd.arg(name).arg(id.toQString());
	return git(cmd);
}

bool Git::delete_tag(QString const &name, bool remote)
{
	QString cmd = "tag --delete \"%1\"";
	cmd = cmd.arg(name);
	git(cmd);

	if (remote) {
		QString cmd = "push --delete origin \"%1\"";
		cmd = cmd.arg(name);
		git(cmd);
	}

	return true;
}

QString Git::diff(QString const &old_id, QString const &new_id)
{
	QString cmd = "diff --full-index -a %1 %2";
	cmd = cmd.arg(old_id).arg(new_id);
	git(cmd);
	return resultQString();
}

QString Git::diff_file(QString const &old_path, QString const &new_path)
{
	QString cmd = "diff --full-index -a -- %1 %2";
	cmd = cmd.arg(old_path).arg(new_path);
	git(cmd);
	return resultQString();
}

QString Git::diff_head(std::function<bool (QString const &name, QString const &mime)> fn_accept)
{
#if 0
	QString cmd = "diff HEAD";
	git(cmd);
	return resultQString();
#else
	QString cmd = "diff --name-only HEAD";
	git(cmd);
	QStringList files = misc::splitLines(resultQString());
	
	QString diff;
	for (auto file : files) {
		if (file.isEmpty()) continue;
		QString mimetype = global->mainwindow->determinFileType(file);
		if (mimetype.startsWith("image/")) continue; // 画像ファイルはdiffしない
		if (mimetype == "application/octetstream") continue; // バイナリファイルはdiffしない
		if (mimetype == "application/pdf") continue; // PDFはdiffしない
		if (fn_accept) {
			if (!fn_accept(file, mimetype)) continue; // ファイルの種類によるフィルタリング
		}
		cmd = "diff --full-index HEAD -- " + file;
		git(cmd);
		diff += resultQString();
	}
	return diff;
#endif	
}

QList<Git::DiffRaw> Git::diff_raw(CommitID const &old_id, CommitID const &new_id)
{
	QList<DiffRaw> list;
	QString cmd = "diff --raw --abbrev=%1 %2 %3";
	cmd = cmd.arg(GIT_ID_LENGTH).arg(old_id.toQString()).arg(new_id.toQString());
	git(cmd);
	QString text = resultQString();
	QStringList lines = text.split('\n', _SkipEmptyParts);
	for (QString const &line : lines) { // raw format: e.g. ":100644 100644 08bc10d... 18f0501... M  src/MainWindow.cpp"
		DiffRaw item;
		int colon = line.indexOf(':');
		int tab = line.indexOf('\t');
		if (colon >= 0 && colon < tab) {
			QStringList header = line.mid(colon + 1, tab - colon - 1).split(' ', _SkipEmptyParts); // コロンとタブの間の文字列を空白で分割
			if (header.size() >= 5) {
				QStringList files = line.mid(tab + 1).split('\t', _SkipEmptyParts); // タブより後ろはファイルパス
				if (!files.empty()) {
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
	QString cmd = "diff --full-index -a %1 -- \"%2\"";
	cmd = cmd.arg(old_id).arg(path);
	git(cmd);
	return resultQString();
#else
	return diff_(old_id, new_id);
#endif
}

QString Git::getCurrentBranchName()
{
	if (isValidWorkingCopy()) {
		git("rev-parse --abbrev-ref HEAD");
		QString s = resultQString().trimmed();
		if (!s.isEmpty() && !s.startsWith("fatal:") && s.indexOf(' ') < 0) {
			return s;
		}
	}
	return QString();
}

QStringList Git::getUntrackedFiles()
{
	QStringList files;
	Git::FileStatusList stats = status_s();
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
		ushort const *e = nullptr;
		int n;
		for (n = 0; ptr + n < end; n++) {
			if (ptr[n] == ']') {
				e = ptr + n;
				break;
			}
		}
		if (e) {
			end = e;
			while (1) {
				if (NCompare(ptr, "ahead ", 6)) {
					ptr += 6;
					SkipSpace();
					b->ahead = GetNumber();
				} else if (NCompare(ptr, "behind ", 7)) {
					ptr += 7;
					SkipSpace();
					b->behind = GetNumber();
				} else {
					ushort c = 0;
					if (ptr < end) {
						c = *ptr;
					}
					if (c == 0) break;
					ptr++;
				}
			}
		}
	}
}

QList<Git::Branch> Git::branches()
{
	struct BranchItem {
		Branch branch;
		QString alternate_name;
	};
	QList<BranchItem> branches;
	git(QString("branch -vv -a --abbrev=%1").arg(GIT_ID_LENGTH));
	QString s = resultQString();

	QStringList lines = misc::splitLines(s);
	for (QString const &line : lines) {
		if (line.size() > 2) {
			QString name;
			QString text = line.mid(2);
			int pos = 0;
			if (text.startsWith('(')) {
				int i, paren = 1;
				for (i = 1; i < text.size(); i++) {
					if (text[i] == '(') {
						paren++;
					} else if (text[i] == ')') {
						if (paren > 0) {
							paren--;
							if (paren == 0) {
								pos = i;
								break;
							}
						}
					}
				}
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

				BranchItem item;

				if (name.startsWith("HEAD detached at ")) {
					item.branch.flags |= Branch::HeadDetachedAt;
					name = name.mid(17);
				}

				if (name.startsWith("HEAD detached from ")) {
					item.branch.flags |= Branch::HeadDetachedFrom;
					name = name.mid(19);
				}

				if (name.startsWith("remotes/")) {
					name = name.mid(8);
					int i = name.indexOf('/');
					if (i > 0) {
						item.branch.remote = name.mid(0, i);
						name = name.mid(i + 1);
					}
				}

				item.branch.name = name;

				if (text.startsWith("-> ")) {
					item.alternate_name = text.mid(3);
				} else {
					int i = text.indexOf(' ');
					if (i == GIT_ID_LENGTH) {
						item.branch.id = Git::CommitID(text.mid(0, GIT_ID_LENGTH));
					}
					while (i < text.size() && QChar::isSpace(text.utf16()[i])) {
						i++;
					}
					text = text.mid(i);
					parseAheadBehind(text, &item.branch);
				}

				if (line.startsWith('*')) {
					item.branch.flags |= Branch::Current;
				}

				branches.push_back(item);
			}
		}
	}

	QList<Branch> ret;

	for (int i = 0; i < branches.size(); i++) {
		BranchItem *b = &branches[i];
		if (b->alternate_name.isEmpty()) {
			ret.append(b->branch);
		} else {
			for (int j = 0; j < branches.size(); j++) {
				if (j != i) {
					if (branches[j].branch.name == b->alternate_name) {
						b->branch.id = branches[j].branch.id;
						ret.append(b->branch);
						break;
					}
				}
			}
		}
	}

	return ret;
}

std::optional<Git::CommitItem> Git::parseCommitItem(QString const &line)
{
	int i = line.indexOf("##");
	if (i > 0) {
		Git::CommitItem item;
		std::string sign_fp; // 署名フィンガープリント
		item.message = line.mid(i + 2);
		QStringList atts = line.mid(0, i).split('#');
		for (QString const &s : atts) {
			int j = s.indexOf(':');
			if (j > 0) {
				QString key = s.mid(0, j);
				QString val = s.mid(j + 1);
				if (key == "id") {
					item.commit_id = Git::CommitID(val);
				} else if (key == "gpg") { // %G? 署名検証結果
					item.sign.verify = *val.utf16();
				} else if (key == "key") { // %GF 署名フィンガープリント
					sign_fp = val.toStdString();
				} else if (key == "trust") {
					item.sign.trust = val;
				} else if (key == "parent") {
					item.setParents(val.split(' ', _SkipEmptyParts));
				} else if (key == "author") {
					item.author = val;
				} else if (key == "mail") {
					item.email = val;
				} else if (key == "date") {
					auto ParseDateTime = [](char const *s){
						int year, month, day, hour, minute, second;
						if (sscanf(s, "%d-%d-%d %d:%d:%d"
								   , &year
								   , &month
								   , &day
								   , &hour
								   , &minute
								   , &second
								   ) == 6) {
							return QDateTime(QDate(year, month, day), QTime(hour, minute, second));
						}
						return QDateTime();
					};
					item.commit_date = ParseDateTime(val.toStdString().c_str());
				}
			}
		}
		if (!sign_fp.empty()) {
			item.sign.key_fingerprint = misc::hex_string_to_bin(sign_fp);
		}
		return item;
	}
	return std::nullopt;
}

/**
 * @brief コミットログを取得する
 * @param id コミットID
 * @param maxcount 最大アイテム数
 * @return
 */
Git::CommitItemList Git::log_all(CommitID const &id, int maxcount)
{
	CommitItemList items;

	QString cmd = "log --pretty=format:\"id:%H#parent:%P#author:%an#mail:%ae#date:%ci##%s\" --all -%1 %2";
	cmd = cmd.arg(maxcount).arg(id.toQString());
	git_nolog(cmd, nullptr);
	if (getProcessExitCode() == 0) {
		QString text = resultQString().trimmed();
		QStringList lines = misc::splitLines(text);
		for (QString const &line : lines) {
			auto item = parseCommitItem(line);
			if (item) {
				items.list.push_back(*item);
			}
		}
	}
	return items;
}

/**
 * @brief Git::log_signature
 * @param id コミットID
 * @return
 *
 * コミットに関連する署名情報を取得する
 */
std::optional<Git::CommitItem> Git::log_signature(CommitID const &id)
{
	QString cmd = "log -1 --show-signature --pretty=format:\"id:%H#gpg:%G?#key:%GF#sub:%GP#trust:%GT##%s\" %1";
	cmd = cmd.arg(id.toQString());
	git_nolog(cmd, nullptr);
	if (getProcessExitCode() == 0) {
		auto splitLines = [&](QString const &text){ // modified from misc::splitLines
			QStringList list;
			ushort const *begin = text.utf16();
			ushort const *end = begin + text.size();
			ushort const *ptr = begin;
			ushort const *left = ptr;
			while (1) {
				ushort c = 0;
				if (ptr < end) {
					c = *ptr;
				}
				if (c == '\n' /*|| c == '\r'*/ || c == 0) { // '\r'では分割しない（Windowsで git log の結果に単独の'\r'が現れることがある）
					QString s = QString::fromUtf16((char16_t const *)left, int(ptr - left));
					s = s.trimmed(); // '\r'などの空行を除去
					list.push_back(s);
					if (c == 0) break;
					if (c == '\n') {
						ptr++;
					}
					left = ptr;
				} else {
					ptr++;
				}
			}
			return list;
		};

		QString gpgtext;
		QString text = resultQString().trimmed();
		QStringList lines = splitLines(text);
		for (QString const &line : lines) {
			if (line.startsWith("gpg:")) {
				if (!gpgtext.isEmpty()) {
					gpgtext += '\n';
				}
				gpgtext += line;
			} else if (line.startsWith("Primary key fingerprint:")) {
				// nop
			} else if (line.startsWith("id:")) {
				auto item = parseCommitItem(line);
				if (item) {
					item->sign.text = gpgtext;
					return item;
				}
			}
		}
	}
	return std::nullopt;
}

Git::CommitItemList Git::log(int maxcount)
{
	return log_all({}, maxcount);
}

Git::CommitItem Git::parseCommit(QByteArray const &ba)
{
	CommitItem out;
	QStringList lines = misc::splitLines(ba, [](char const *p, size_t n){
		return QString::fromUtf8(p, (int)n);
	});
	while (!lines.empty() && lines[lines.size() - 1].isEmpty()) {
		lines.pop_back();
	}

	bool gpgsig = false;
	bool message = false;

	int i;
	for (i = 0; i < lines.size(); i++) {
		QString const &line = lines[i];
		if (line.isEmpty()) {
			i++;
			for (; i < lines.size(); i++) {
				QString const &line = lines[i];
				if (!out.message.isEmpty()) {
					out.message.append('\n');
				}
				out.message.append(line);
			}
			break;
		}
		if (gpgsig) {
			if (line[0] == ' ') {
				QString s = line.mid(1);
				out.gpgsig += s + '\n';
				if (s == "-----END PGP SIGNATURE-----") {
					gpgsig = false;
				}
			}
			continue;
		}
		if (line.isEmpty()) {
			message = true;
			continue;
		}
		if (message) {
			if (!out.message.isEmpty()) {
				out.message += '\n';
			}
			out.message += line;
		}
		if (line.startsWith("parent ")) {
			out.parent_ids.push_back(Git::CommitID(line.mid(7)));
		} else if (line.startsWith("author ")) {
			QStringList arr = misc::splitWords(line);
			int n = arr.size();
			if (n > 4) {
				n -= 2;
				out.commit_date = QDateTime::fromSecsSinceEpoch(atol(arr[n].toStdString().c_str()));
				n--;
				out.email = arr[n];
				if (out.email.startsWith('<') && out.email.endsWith('>')) {
					int n = out.email.size();
					out.email = out.email.mid(1, n - 2);
				}
				for (int i = 1; i < n; i++) {
					if (!out.author.isEmpty()) {
						out.author += ' ';
					}
					out.author += arr[i];
				}
			}
		} else if (line.startsWith("commiter ")) {
			// nop
		} else if (line.startsWith("gpgsig -----BEGIN PGP SIGNATURE-----")) {
			out.gpgsig.append(line.mid(7));
			gpgsig = true;
		}
	}
	return out;
}

std::optional<Git::CommitItem> Git::queryCommitItem(CommitID const &id)
{
	Git::CommitItem ret;
	if (objectType(id) != "commit") return std::nullopt;

	ret.commit_id = id;
	auto ba = cat_file(id);
	if (ba) {
		ret = parseCommit(*ba);
	}
	return ret;
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

bool Git::clone(CloneData const &data, AbstractPtyProcess *pty)
{
	QString clone_to = data.basedir / data.subdir;
	m->info.working_repo_dir = misc::normalizePathSeparator(clone_to);

	bool ok = false;
	QDir cwd = QDir::current();

	auto DoIt = [&](){
		QString cmd = "clone --recurse-submodules --progress -j%1 \"%2\" \"%3\"";
		cmd = cmd.arg(std::thread::hardware_concurrency()).arg(data.url).arg(data.subdir);
		ok = git_nochdir(cmd, pty);
	};

	if (pty) {
		pty->setChangeDir(data.basedir);
		DoIt();
	} else {
		if (QDir::setCurrent(data.basedir)) {
			DoIt();
			QDir::setCurrent(cwd.path());
		}
	}

	return ok;
}

QList<Git::SubmoduleItem> Git::submodules()
{
	QList<Git::SubmoduleItem> mods;

	git("submodule");
	QString text = resultQString();
	ushort c = text.utf16()[0];
	if (c == ' ' || c == '+' || c == '-') {
		text = text.mid(1);
	}
	QStringList words = misc::splitWords(text);
	if (words.size() >= 2) {
		SubmoduleItem sm;
		sm.id = Git::CommitID(words[0]);
		sm.path = words[1];
		if (isValidID(sm.id)) {
			if (words.size() >= 3) {
				sm.refs = words[2];
				if (sm.refs.startsWith('(') && sm.refs.endsWith(')')) {
					sm.refs = sm.refs.mid(1, sm.refs.size() - 2);
				}
			}

			mods.push_back(sm);
		}
	}
	return mods;
}

bool Git::submodule_add(const CloneData &data, bool force, AbstractPtyProcess *pty)
{
	bool ok = false;

	QString cmd = "submodule add";
	if (force) {
		cmd += " -f";
	}
	cmd += " \"%1\" \"%2\"";
	cmd = cmd.arg(data.url).arg(data.subdir);
	Option opt;
	opt.errout = true;
	opt.pty = pty;
	ok = git(cmd, opt);

	return ok;
}

bool Git::submodule_update(SubmoduleUpdateData const &data, AbstractPtyProcess *pty)
{
	QString cmd = "submodule update";
	if (data.init) {
		cmd += " --init";
	}
	if (data.recursive) {
		cmd += " --recursive";
	}
	Option opt;
	opt.errout = true;
	opt.pty = pty;
	return git(cmd, opt);
}

QString Git::encodeQuotedText(QString const &str)
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
	return QString::fromUtf16((char16_t const *)&vec[0], vec.size());
}

bool Git::commit_(QString const &msg, bool amend, bool sign, AbstractPtyProcess *pty)
{
	QString cmd = "commit";
	if (amend) {
		cmd += " --amend";
	}
	if (sign) {
		cmd += " -S";
	}

	QString text = msg.trimmed();
	if (text.isEmpty()) {
		text = "no message";
	}
	text = encodeQuotedText(text);
	cmd += QString(" -m %1").arg(text);

	Option opt;
	opt.pty = pty;
	return git(cmd, opt);
}

bool Git::commit(QString const &text, bool sign, AbstractPtyProcess *pty)
{
	return commit_(text, false, sign, pty);
}

bool Git::commit_amend_m(QString const &text, bool sign, AbstractPtyProcess *pty)
{
	return commit_(text, true, sign, pty);
}

bool Git::revert(CommitID const &id)
{
	QString cmd = "revert %1";
	cmd = cmd.arg(id.toQString());
	return git(cmd);
}

bool Git::push_u(bool set_upstream, QString const &remote, QString const &branch, bool force, AbstractPtyProcess *pty)
{
	if (remote.indexOf('\"') >= 0 || branch.indexOf('\"') >= 0) {
		return false;
	}
	QString cmd = "push";
	if (force) {
		cmd += " --force";
	}
	if (set_upstream && !remote.isEmpty() && !branch.isEmpty()) {
		cmd += " -u \"%1\" \"%2\"";
		cmd = cmd.arg(remote).arg(branch);
	}
	Option opt;
	opt.pty = pty;
	return git(cmd, opt);
}

bool Git::push_tags(AbstractPtyProcess *pty)
{
	QString cmd = "push --tags";
	Option opt;
	opt.pty = pty;
	return git(cmd, opt);
}

Git::FileStatusList Git::status_s_()
{
	FileStatusList files;
	if (git("status -s -u --porcelain")) {
		QString text = resultQString();
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

Git::FileStatusList Git::status_s()
{
	return status_s_();
}

QString Git::objectType(CommitID const &id)
{
	if (isValidID(id)) {
		git("cat-file -t " + id.toQString());
		return resultQString().trimmed();
	}
	return {};
}

QByteArray Git::cat_file_(CommitID const &id)
{
	if (isValidID(id)) {
		git("cat-file -p " + id.toQString());
		return toQByteArray();
	}
	return {};
}

std::optional<QByteArray> Git::cat_file(CommitID const &id)
{
	QByteArray out;
	if (isValidID(id)) {
		return cat_file_(id);
	}
	return std::nullopt;
}

QString Git::queryEntireCommitMessage(CommitID const &id)
{
	QString ret;
	auto file = cat_file(id);
	if (file) {
		QString message = QString::fromUtf8(file->constData(), file->size());
		QStringList lines = message.split('\n');
		bool header = true;
		for (int i = 0; i < lines.size(); i++) {
			QString line = lines[i];
			if (header) {
				if (line.isEmpty()) {
					header = false;
				}
			} else {
				if (!ret.isEmpty()) {
					ret += '\n';
				}
				ret += lines[i];
			}
		}
	}
	return ret;
}

void Git::resetFile(QString const &path)
{
	git("checkout -- \"" + path + "\"");
}

void Git::resetAllFiles()
{
	git("reset --hard HEAD");
}

void Git::removeFile(QString const &path)
{
	git("rm " + path);
}

void Git::add_A()
{
	git("add -A");
}

void Git::stage(QString const &path)
{
	git("add " + path);
}

bool Git::stage(QStringList const &paths, AbstractPtyProcess *pty)
{
	if (paths.isEmpty()) return false;

	QString cmd = "add";
	for (QString const &path : paths) {
		cmd += ' ';
		cmd += '\"';
		cmd += path;
		cmd += '\"';
	}

	Option opt;
	opt.pty = pty;
	return git(cmd, opt);
}

void Git::unstage(QString const &path)
{
	QString cmd = "reset HEAD \"%1\"";
	git(cmd.arg(path));
}

void Git::unstage(QStringList const &paths)
{
	if (paths.isEmpty()) return;
	QString cmd = "reset HEAD";
	for (QString const &path : paths) {
		cmd += " \"";
		cmd += path;
		cmd += '\"';
	}
	git(cmd);
}

bool Git::unstage_all()
{
	return git("reset HEAD");
}

bool Git::pull(AbstractPtyProcess *pty)
{
	Option opt;
	opt.pty = pty;
	return git("pull", opt);
}

bool Git::fetch(AbstractPtyProcess *pty, bool prune)
{
	QString cmd = "fetch --tags -f -j%1";
    cmd = cmd.arg(std::thread::hardware_concurrency());
	if (prune) {
		cmd += " --prune";
	}
	Option opt;
	opt.pty = pty;
	return git(cmd, opt);
}

bool Git::fetch_tags_f(AbstractPtyProcess *pty)
{
	QString cmd = "fetch --tags -f -j%1";
    cmd = cmd.arg(std::thread::hardware_concurrency());
	Option opt;
	opt.pty = pty;
	return git(cmd, opt);
}

QStringList Git::make_branch_list_()
{
	QStringList list;
	QStringList l = misc::splitLines(resultQString());
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

QString Git::getCherryPicking() const
{
	QString dir = workingDir();
	QString path = dir / ".git/CHERRY_PICK_HEAD";
	QFile file(path);
	if (file.open(QFile::ReadOnly)) {
		QString line = QString::fromLatin1(file.readLine()).trimmed();
		if (isValidID(line)) {
			return line;
		}
	}
	return QString();
}

QString Git::getMessage(QString const &id)
{
	git("show --no-patch --pretty=%s " + id);
	return resultQString().trimmed();
}

void Git::mergeBranch(QString const &name, MergeFastForward ff, bool squash)
{
	QString cmd = "merge ";
	switch (ff) {
	case MergeFastForward::No:
		cmd += "--no-ff ";
		break;
	case MergeFastForward::Only:
		cmd += "--ff-only ";
		break;
	default:
		cmd += "--ff ";
		break;
	}
	if (squash) {
		cmd += "--squash ";
	}
	git(cmd + name);
}

void Git::rebaseBranch(QString const &name)
{
	git("rebase " + name);
}

void Git::rebase_abort()
{
	git("rebase --abort");
}

QStringList Git::getRemotes()
{
	QStringList ret;
	git("remote");
	QStringList lines = misc::splitLines(resultQString());
	for (QString const &line: lines) {
		if (!line.isEmpty()) {
			ret.push_back(line);
		}
	}
	return ret;
}

Git::User Git::getUser(Source purpose)
{
	User user;
	bool global = purpose == Git::Source::Global;
	bool local = purpose == Git::Source::Local;
	QString arg1;
	if (global) arg1 = "--global";
	if (local) arg1 = "--local";
	Option opt;
	opt.chdir = !global;
	if (git(QString("config %1 user.name").arg(arg1), opt)) {
		user.name = resultQString().trimmed();
	}
	if (git(QString("config %1 user.email").arg(arg1), opt)) {
		user.email = resultQString().trimmed();
	}
	return user;
}

void Git::setUser(const User &user, bool global)
{
	bool unset = !user;
	QString config = global ? "--global" : "--local";
	if (unset) {
		config += " --unset";
	}
	Option opt;
	opt.chdir = !global;
	git(QString("config %1 user.name %2").arg(config).arg(encodeQuotedText(user.name)), opt);
	git(QString("config %1 user.email %2").arg(config).arg(encodeQuotedText(user.email)), opt);
}

bool Git::reset_head1()
{
	return git("reset HEAD~1");
}

bool Git::reset_hard()
{
	return git("reset --hard");
}

bool Git::clean_df()
{
	return git("clean -df");
}

bool Git::stash()
{
	return git("stash");
}

bool Git::stash_apply()
{
	return git("stash apply");
}

bool Git::stash_drop()
{
	return git("stash drop");
}

bool Git::rm_cached(QString const &file)
{
	QString cmd = "rm --cached \"%1\"";
	return git(cmd.arg(file));
}

void Git::remote_v(std::vector<Remote> *out)
{
	out->clear();
	git("remote -v");
	QStringList lines = misc::splitLines(resultQString());
	for (QString const &line : lines) {
		int i = line.indexOf('\t');
		int j = line.indexOf(" (");
		if (i > 0 && i < j) {
			Remote r;
			r.name = line.mid(0, i);
			r.ssh_key = m->ssh_key_override;
			QString url = line.mid(i + 1, j - i - 1);
			QString type = line.mid(j + 1);
			if (type.startsWith('(') && type.endsWith(')')) {
				type = type.mid(1, type.size() - 2);
				if (type == "fetch") {
					r.url_fetch = url;
				} else if (type == "push") {
					r.url_push = url;
				}
			}
			out->push_back(r);
		}
	}
	std::sort(out->begin(), out->end(), [](Remote const &a, Remote const &b){
		return a.name < b.name;
	});
	size_t i = out->size();
	if (i > 1) {
		i--;
		while (i > 0) {
			i--;
			if ((*out)[i].name == (*out)[i + 1].name) {
				if ((*out)[i].url_fetch.isEmpty()) {
					(*out)[i].url_fetch = (*out)[i + 1].url_fetch;
				}
				if ((*out)[i].url_push.isEmpty()) {
					(*out)[i].url_push = (*out)[i + 1].url_push;
				}
				out->erase(out->begin() + i + 1);
			}
		}
	}
}

void Git::setRemoteURL(Git::Remote const &remote)
{
	QString cmd = "remote set-url %1 %2";
	cmd = cmd.arg(encodeQuotedText(remote.name)).arg(encodeQuotedText(remote.url_fetch));
	git(cmd);
}

void Git::addRemoteURL(Git::Remote const &remote)
{
	QString cmd = "remote add \"%1\" \"%2\"";
	cmd = cmd.arg(encodeQuotedText(remote.name)).arg(encodeQuotedText(remote.url_fetch));
	m->ssh_key_override = remote.ssh_key;
	git(cmd);
}

void Git::removeRemote(QString const &name)
{
	QString cmd = "remote remove %1";
	cmd = cmd.arg(encodeQuotedText(name));
	git(cmd);
}

bool Git::reflog(ReflogItemList *out, int maxcount)
{
	out->clear();
	QString cmd = "reflog --no-abbrev --raw -n %1";
	cmd = cmd.arg(maxcount);
	if (!git(cmd)) return false;
	QByteArray ba = toQByteArray();
	if (!ba.isEmpty()) {
		ReflogItem item;
		char const *begin = ba.data();
		char const *end = begin + ba.size();
		char const *left = begin;
		char const *ptr = begin;
		while (1) {
			int c = 0;
			if (ptr < end) {
				c = *ptr;
			}
			if (c == '\r' || c == '\n' || c == 0) {
				int d = 0;
				QString line = QString::fromUtf8(left, int(ptr - left));
				if (left < ptr) {
					d = *left & 0xff;
				}
				if (d == ':') {
					// ex. ":100644 100644 bb603836fb597cca994309a1f0a52251d6b20314 d6b9798854debee375bb419f0f2ed9c8437f1932 M\tsrc/MainWindow.cpp"
					int tab = line.indexOf('\t');
					if (tab > 1) {
						QString tmp = line.mid(1, tab - 1);
						QString path = line.mid(tab + 1);
						QStringList cols = misc::splitWords(tmp);
						if (!path.isEmpty() && cols.size() == 5) {
							ReflogItem::File file;
							file.atts_a = cols[0];
							file.atts_b = cols[1];
							file.id_a = cols[2];
							file.id_b = cols[3];
							file.type = cols[4];
							file.path = path;
							item.files.push_back(file);
						}
					}
				} else {
					bool start = isxdigit(d);
					if (start || c == 0) {
						if (!item.id.isEmpty()) {
							out->push_back(item);
						}
					}
					if (start) {
						// ex. "0a2a8b6b66f48bcbf985d8a2afcd14ff41676c16 HEAD@{188}: commit: message"
						item = ReflogItem();
						int i = line.indexOf(": ");
						if (i > 0) {
							int j = line.indexOf(": ", i + 2);
							if (j > 2) {
								item.head = line.mid(0, i);
								item.command = line.mid(i + 2, j - i - 2);
								item.message = line.mid(j + 2);
								if (item.head.size() > GIT_ID_LENGTH) {
									item.id = item.head.mid(0, GIT_ID_LENGTH);
									item.head = item.head.mid(GIT_ID_LENGTH + 1);
								}
							}
						}
					}
				}
				if (c == 0) break;
				ptr++;
				left = ptr;
			} else {
				ptr++;
			}
		}
	}
	return true;
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
				for (int i = 0; i < 3 && ptr < right && QChar(*ptr).isDigit(); i++) { // decode \oct
					c = c * 8 + (*ptr - '0');
					ptr++;
				}
			}
			ba.push_back(c);
		}
		return QString::fromUtf8(ba);
	}
	if (left == begin && right == end) return s;
	return QString::fromUtf16((char16_t const *)left, int(right - left));
}

Git::FileStatusCode Git::FileStatus::parseFileStatusCode(char x, char y)
{
	if (x == ' ' && y == 'A') return FileStatusCode::AddedToIndex;
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

void Git::FileStatus::parse(QString const &text)
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

QByteArray Git::blame(QString const &path)
{
	QString cmd = "blame --porcelain --abbrev=40 \"%1\"";
	cmd = cmd.arg(path);
	if (git(cmd)) {
		return toQByteArray();
	}
	return QByteArray();
}

QList<Git::RemoteInfo> Git::ls_remote()
{
	QList<RemoteInfo> list;
	QString cmd = "ls-remote";
	if (git(cmd)) {
		QStringList lines = misc::splitLines(resultQString());
		for (QString const &line : lines) {
			QStringList words = misc::splitWords(line);
			if (words.size() == 2 && isValidID(words[0])) {
				RemoteInfo info;
				info.commit_id = words[0];
				info.name = words[1];
				list.push_back(info);
			}
		}
	}
	return list;
}

QString Git::signingKey(Source purpose)
{
	QString arg1;
	if (purpose == Source::Global) arg1 = "--global";
	if (purpose == Source::Local) arg1 = "--local";
	QString cmd = "config %1 user.signingkey";
	cmd = cmd.arg(arg1);
	Option opt;
	opt.chdir = purpose != Source::Global;
	if (git(cmd, opt)) {
		return resultQString().trimmed();
	}
	return QString();
}

bool Git::setSigningKey(QString const &id, bool global)
{
	for (auto i : id) {
		if (!QChar(i).isLetterOrNumber()) return false;
	}

	QString cmd = "config %1 %2 user.signingkey %3";
	cmd = cmd.arg(global ? "--global" : "--local").arg(id.isEmpty() ? "--unset" : "").arg(id);
	Option opt;
	opt.chdir = !global;
	return git(cmd, opt);
}

Git::SignPolicy Git::signPolicy(Source source)
{
	QString arg1;
	if (source == Source::Global) arg1 = "--global";
	if (source == Source::Local)  arg1 = "--local";
	QString cmd = "config %1 commit.gpgsign";
	cmd = cmd.arg(arg1);
	Option opt;
	opt.chdir = source != Source::Global;
	if (git(cmd, opt)) {
		QString t = resultQString().trimmed();
		if (t == "false") return SignPolicy::False;
		if (t == "true")  return SignPolicy::True;
	}
	return SignPolicy::Unset;
}

bool Git::setSignPolicy(Source source, SignPolicy policy)
{
	QString arg1;
	if (source == Source::Global) arg1 = "--global";
	if (source == Source::Local)  arg1 = "--local";
	QString cmd = "config %1 %2 commit.gpgsign %3";
	QString arg2;
	QString arg3;
	if (policy == SignPolicy::False) {
		arg3 = "false";
	} else if (policy == SignPolicy::True) {
		arg3 = "true";
	} else {
		arg2 = "--unset";
	}
	cmd = cmd.arg(arg1).arg(arg2).arg(arg3);
	Option opt;
	opt.chdir = source != Source::Global;
	return git(cmd, opt);
}

bool Git::configGpgProgram(QString const &path, bool global)
{
	QString cmd = "config ";
	if (global) {
		cmd += "--global ";
	}
	if (path.isEmpty()) {
		cmd += "--unset ";
	}
	cmd += "gpg.program ";
	if (!path.isEmpty()) {
		cmd += QString("\"%1\"").arg(path);
	}
	return git_nochdir(cmd, nullptr);
}

// Diff

void Git::Diff::makeForSingleFile(Git::Diff *diff, QString const &id_a, QString const &id_b, QString const &path, QString const &mode)
{
	diff->diff = QString("diff --git a/%1 b/%2").arg(path).arg(path);
	diff->index = QString("index %1..%2 %3").arg(id_a).arg(id_b).arg(0);
	diff->blob.a_id_or_path = id_a;
	diff->blob.b_id_or_path = id_b;
	diff->path = path;
	diff->mode = mode;
	diff->type = Git::Diff::Type::Create;
}

//

void parseDiff(std::string const &s, Git::Diff const *info, Git::Diff *out)
{
	std::vector<std::string> lines;
	{
		char const *begin = s.c_str();
		char const *end = begin + s.size();
		misc::splitLines(begin, end, &lines, false);
	}


	out->diff = QString("diff --git ") + ("a/" + info->path) + ' ' + ("b/" + info->path);
	out->index = QString("index ") + info->blob.a_id_or_path + ".." + info->blob.b_id_or_path + ' ' + info->mode;
	out->path = info->path;
	out->blob = info->blob;

	bool atat = false;
	for (std::string const &line : lines) {
		int c = line[0] & 0xff;
		if (c == '@') {
			if (strncmp(line.c_str(), "@@ ", 3) == 0) {
				out->hunks.push_back(Git::Hunk());
				out->hunks.back().at = line;
				atat = true;
			}
		} else {
			if (atat && c == '\\') { // e.g. \ No newline at end of file...
				// ignore this line
			} else {
				if (atat) {
					if (c == ' ' || c == '-' || c == '+') {
						// nop
					} else {
						atat = false;
					}
				}
				if (atat) {
					if (!out->hunks.isEmpty()) {
						out->hunks.back().lines.push_back(line);
					}
				}
			}
		}
	}
}


void parseGitSubModules(const QByteArray &ba, QList<Git::SubmoduleItem> *out)
{
	*out = {};
	QStringList lines = misc::splitLines(QString::fromUtf8(ba));
	Git::SubmoduleItem submod;
	auto Push = [&](){
		if (!submod.name.isEmpty()) {
			out->push_back(submod);
		}
		submod = {};
	};
	for (int i = 0; i < lines.size(); i++) {
		QString line = lines[i].trimmed();
		if (line.startsWith('[')) {
			Push();
			if (line.startsWith("[submodule ") && line.endsWith(']')) {
				int i = 11;
				int j = line.size() - 1;
				if (i + 1 < j && line[i] == '\"') {
					if (line[j - 1] == '\"') {
						i++;
						j--;
					}
				}
				submod.name = line.mid(i, j - i);
			}
		} else {
			int i = line.indexOf('=');
			if (i > 0) {
				QString key = line.mid(0, i).trimmed();
				QString val = line.mid(i + 1).trimmed();
				if (key == "path") {
					submod.path = val;
				} else if (key == "url") {
					submod.url = val;
				}
			}
		}
	}
	Push();
}



