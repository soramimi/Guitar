
#include "Git.h"
#include "GitBasicSession.h"
#include "Profile.h"
#include "common/joinpath.h"
#include "common/strformat.h"
#include "common/q/helper.h"
#include "common/q/FileInfo.h"
#include "common/q/Dir.h"
#include <QString>

void GitCommitItem::setParents(const QStringList &list)
{
	parent_ids.clear();
	for (QString const &id : list) {
		parent_ids.append(GitHash(id));
	}
}

Git::Git()
{
}

Git::Git(const GitContext &cx, std::string const &repodir, const std::string &submodpath, const std::string &sshkey)
{
	_init(cx);
	setWorkingRepositoryDir(repodir, sshkey);
	setSubmodulePath(submodpath);
}

void Git::clearCommandCache()
{
	session_->clearCommandCache();
}

void Git::clearObjectCache()
{
	session_->clearObjectCache();
}

void Git::_init(GitContext const &cx)
{
	session_ = cx.connect();
}

void Git::setWorkingRepositoryDir(std::string const &repo, std::string const &sshkey)
{
	gitinfo().working_repo_dir = repo;
	gitinfo().ssh_key_override = sshkey;
}

void Git::setSubmodulePath(std::string const &submodpath)
{
	gitinfo().submodule_path = submodpath;
}

std::string const &Git::sshKey() const
{
	return gitinfo().ssh_key_override;
}

void Git::setSshKey(const std::string &sshkey)
{
	gitinfo().ssh_key_override = sshkey;
}

std::string Git::status()
{
	auto result = git("status");
	return resultStdString(result);
}

std::vector<char> Git::toByteArray(std::optional<GitResult> const &var) const
{
	if (var) return var->output();
	return {};
}

std::string Git::resultStdString(std::optional<GitResult> const &var) const
{
	if (!var) return {};
	auto const &v = var->output();
	if (v.empty()) return {};
	return std::string(v.data(), v.size());
}

QString Git::resultQString(std::optional<GitResult> const &var) const
{
	if (var) {
		return QString::fromUtf8(toByteArray(var));
	} else {
		return {};
	}
}

std::string Git::errorMessage(std::optional<GitResult> const &var) const
{
	if (!var) return {};
	return var->error_message();
}

bool Git::isValidWorkingCopy(std::string const &dir) const
{
	return session_->isValidWorkingCopy(dir);
}

bool Git::isValidWorkingCopy() const
{
	return isValidWorkingCopy(workingDir());
}

bool GitBasicSession::isValidWorkingCopy(const std::string &dir) const
{
	if (!dir.empty()) {
		if (FileInfo(dir).isDir()) {
			std::string gitdir = dir / ".git";
			FileInfo info(gitdir);
			if (info.isFile()) { // submodule?
				QFile file((QS)gitdir);
				if (file.open(QFile::ReadOnly)) {
					QString line = QString::fromUtf8(file.readLine());
					if (line.startsWith("gitdir:")) {
						return true;
					}
				}
			} else if (info.isDir()) { // regular dir
				if (FileInfo(gitdir).isDir()) {
					if (FileInfo(gitdir / "config").isFile()) { // git repository
						return true;
					}
				}
			}
		}
	}

	return false;
}

std::string Git::version()
{
	auto result = git_nochdir("--version", nullptr);
	return (std::string)misc::trimmed(resultStdString(result));
}

bool Git::init()
{
	bool ok = false;
	Dir cwd = Dir::current();
	std::string dir = workingDir();
	if (Dir::setCurrent((QS)dir)) {
		std::string gitdir = dir / ".git";
		if (!FileInfo(gitdir).isDir()) {
			git_nochdir("init", nullptr);
			if (FileInfo(gitdir).isDir()) {
				ok = true;
			}
		}
		Dir::setCurrent(cwd.path());
	}
	return ok;
}

GitHash Git::rev_parse(const std::string &name)
{
	std::string cmd = fmt("rev-parse %s")(name);
	AbstractGitSession::Option opt;
	opt.use_cache = true;
	auto result = exec_git(cmd, opt);
	if (result) {
		return GitHash(misc::trimmed(resultStdString(result)));
	}
	return {};
}

QList<GitTag> Git::tags()
{
	QList<GitTag> list;
	auto result = git("show-ref");
	QStringList lines = misc::splitLines(resultQString(result));
	for (QString const &line : lines) {
		QStringList l = misc::splitWords(line);
		if (l.size() >= 2) {
			if (GitHash::isValidID(l[0]) && l[1].startsWith("refs/tags/")) {
				GitTag t;
				t.name = (QS)l[1].mid(10);
				t.id = GitHash(l[0]);
				list.push_back(t);
			}
		}
	}
	return list;
}

bool Git::tag(std::string const &name, GitHash const &id)
{
	std::string cmd = "tag \"%s\" %s";
	cmd = fmt(cmd)(name)(id.toString());
	return (bool)git(cmd);
}

bool Git::delete_tag(std::string const &name, bool remote)
{
	std::string cmd = "tag --delete \"%s\"";
	cmd = fmt(cmd)(name);
	git(cmd);

	if (remote) {
		std::string cmd = "push --delete origin \"%s\"";
		cmd = fmt(cmd)(name);
		git(cmd);
	}

	return true;
}

std::string Git::diff(std::string const &old_id, std::string const &new_id)
{
	std::string cmd = "diff --full-index -a %s %s";
	cmd = fmt(cmd)(old_id)(new_id);
	auto result = git(cmd);
	return resultStdString(result);
}

std::string Git::diff_file(std::string const &old_path, std::string const &new_path)
{
	std::string cmd = "diff --full-index -a -- %s %s";
	cmd = fmt(cmd)(old_path)(new_path);
	auto result = git(cmd);
	return resultStdString(result);
}

std::vector<std::string> Git::diff_name_only_head()
{
	QString cmd = "diff --name-only HEAD";
	auto result = git(cmd.toStdString());
	std::string str = resultStdString(result);
	return misc::splitLines(str, false);
}

std::string Git::diff_full_index_head_file(std::string const &file)
{
	std::string cmd = "diff --full-index HEAD -- " + file;
	auto result = git(cmd);
	return resultStdString(result);
}

template <typename T> void remove_empty(std::vector<T> *v)
{
	auto end = std::remove_if(v->begin(), v->end(), [](T const &x){
		return x.empty();
	});
	v->erase(end, v->end());
}

QList<GitDiffRaw> Git::diff_raw(GitHash const &old_id, GitHash const &new_id)
{
	QList<GitDiffRaw> list;
	std::string cmd = fmt("diff --raw --abbrev=%s %s %s")
			(GIT_ID_LENGTH)
			(old_id.toString())
			(new_id.toString());
	auto result = git(cmd);
	std::string text = resultStdString(result);
	std::vector<std::string_view> lines = misc::splitLinesV(text, false);
	remove_empty(&lines);
	for (std::string_view const &line : lines) { // raw format: e.g. ":100644 100644 08bc10d... 18f0501... M  src/MainWindow.cpp"
		GitDiffRaw item;
		auto colon = line.find(':');
		int tab = line.find('\t');
		if (colon != std::string_view::npos && tab != std::string_view::npos && colon < tab) {
			std::vector<std::string_view> header = misc::splitWords(line.substr(colon + 1, tab - colon - 1)); // コロンとタブの間の文字列を空白で分割
			if (header.size() >= 5) {
				std::vector<std::string_view> files = misc::split(line.substr(tab + 1), '\t'); // タブより後ろはファイルパス
				remove_empty(&files);
				if (!files.empty()) {
					item.a.id = (std::string)header[2];
					item.b.id = (std::string)header[3];
					item.a.mode = (std::string)header[0];
					item.b.mode = (std::string)header[1];
					item.state = (std::string)header[4];
					// item.files = files;
					for (std::string_view const &file : files) {
						item.files.push_back(gitTrimPath((std::string)file));
					}
					list.push_back(item);
				}
			}
		}
	}
	return list;
}

std::string Git::diff_to_file(std::string const &old_id, std::string const &path)
{
	std::string cmd = "diff --full-index -a %s -- \"%s\"";
	cmd = fmt(cmd)(old_id)(path);
	auto result = git(cmd);
	return resultStdString(result);
}

std::string Git::getDefaultBranch()
{
	AbstractGitSession::Option opt;
	opt.chdir = false;
	auto result = exec_git("config --global init.defaultBranch", opt);
	return (std::string)misc::trimmed(resultStdString(result));
}

void Git::setDefaultBranch(std::string const &branchname)
{
	AbstractGitSession::Option opt;
	opt.chdir = false;
	std::string cmd = "config --global init.defaultBranch \"%s\"";
	cmd = fmt(cmd)(branchname);
	exec_git(cmd, opt);
}

void Git::unsetDefaultBranch()
{
	AbstractGitSession::Option opt;
	opt.chdir = false;
	exec_git(QString("config --global --unset init.defaultBranch").toStdString(), opt);
}

std::string Git::getCurrentBranchName()
{
	if (isValidWorkingCopy()) {
		auto result = git("rev-parse --abbrev-ref HEAD");
		std::string s{misc::trimmed(resultStdString(result))};
		if (!s.empty() && !misc::starts_with(s, "fatal:") && s.find(' ') == std::string::npos) {
			return s;
		}
	}
	return {};
}

std::vector<std::string> Git::getUntrackedFiles()
{
	std::vector<std::string> files;
	std::vector<GitFileStatus> stats = status_s();
	for (GitFileStatus const &s : stats) {
		if (s.code() == GitFileStatus::Code::Untracked) {
			files.push_back(s.path1());
		}
	}
	return files;
}

void Git::parseAheadBehind(QString const &s, GitBranch *b)
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

QList<GitBranch> Git::branches()
{
	PROFILE;

	struct BranchItem {
		GitBranch branch;
		std::string alternate_name;
	};
	QList<BranchItem> branches;
	auto result = git(fmt("branch -vv -a --abbrev=%s")(GIT_ID_LENGTH));
	std::string s = resultStdString(result);

	std::vector<std::string> lines = misc::splitLines(s, false);
	for (std::string const &line : lines) {
		if (line.size() > 2) {
			std::string name;
			std::string text = line.substr(2);
			int pos = 0;
			if (misc::starts_with(text, '(')) {
				int paren = 1;
				for (int i = 1; i < text.size(); i++) {
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
					name = text.substr(1, pos - 1);
					pos++;
				}
			} else {
				while (pos < text.size() && !isspace((unsigned char)text[pos])) {
					pos++;
				}
				name = text.substr(0, pos);
			}
			if (!name.empty()) {
				while (pos < text.size() && isspace((unsigned char)text[pos])) {
					pos++;
				}
				text = text.substr(pos);

				BranchItem item;

				if (misc::starts_with(name, "HEAD detached at ")) {
					item.branch.flags |= GitBranch::HeadDetachedAt;
					name = name.substr(17);
				}

				if (misc::starts_with(name, "HEAD detached from ")) {
					item.branch.flags |= GitBranch::HeadDetachedFrom;
					name = name.substr(19);
				}

				if (misc::starts_with(name, "remotes/")) {
					name = name.substr(8);
					auto i = name.find('/');
					if (i != std::string::npos && i > 0) {
						item.branch.remote = name.substr(0, i);
						name = name.substr(i + 1);
					}
				}

				item.branch.name = name;

				if (misc::starts_with(text, "-> ")) {
					item.alternate_name = text.substr(3);
				} else {
					auto i = text.find(' ');
					if (i == GIT_ID_LENGTH) {
						item.branch.id = GitHash(text.substr(0, GIT_ID_LENGTH));
					}
					while (i < text.size() && isspace((unsigned char)text[i])) {
						i++;
					}
					text = text.substr(i);
					parseAheadBehind((QS)text, &item.branch);
				}

				if (misc::starts_with(line, '*')) {
					item.branch.flags |= GitBranch::Current;
				}

				branches.push_back(item);
			}
		}
	}

	QList<GitBranch> ret;

	for (int i = 0; i < branches.size(); i++) {
		BranchItem *b = &branches[i];
		if (b->alternate_name.empty()) {
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

static QDateTime parseDateTime(char const *s)
{
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
	return {};
}

std::optional<GitCommitItem> Git::parseCommitItem(std::string const &line)
{
	auto i = line.find("##");
	if (i != std::string::npos && i > 0) {
		GitCommitItem item;
		std::string sign_fp; // 署名フィンガープリント
		item.message = QS(line.substr(i + 2));
		std::string s{line.substr(0, i)};
		std::vector<std::string_view> atts = misc::split(s, '#');
		for (std::string_view const &s : atts) {
			auto j = s.find(':');
			if (j != std::string_view::npos && j > 0) {
				auto key = s.substr(0, j);
				auto val = s.substr(j + 1);
				if (key == "id") {
					item.commit_id = GitHash(val);
				} else if (key == "gpg") { // %G? 署名検証結果
					item.sign.verify = val[0];
				} else if (key == "key") { // %GF 署名フィンガープリント
					sign_fp = (std::string)val;
				} else if (key == "trust") {
					item.sign.trust = (QS)val;
				} else if (key == "parent") {
					QStringList list;
					auto vec = misc::splitWords(val);
					for (std::string_view const &v : vec) {
						if (!v.empty()) {
							list.push_back((QS)v);
						}
					}
					item.setParents(list);
				} else if (key == "author") {
					item.author = (QS)val;
				} else if (key == "mail") {
					item.email = (QS)val;
				} else if (key == "date") {
					item.commit_date = parseDateTime(std::string(val).c_str());
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

GitCommitItemList Git::log_file(std::string const &path, int maxcount)
{
	PROFILE;

	GitCommitItemList items;
	std::vector<GitCommitItem> list;

	std::string cmd = "log --pretty=format:\"id:%H#parent:%P#author:%an#mail:%ae#date:%ci##%s\"";
	cmd += fmt(" --all -%d -- %s")(maxcount)(path);
	auto result = git_nolog(cmd, nullptr);

	if (result && result->exit_code() == 0) {
		std::string text{misc::trimmed(resultStdString(result))};
		std::vector<std::string> lines = misc::splitLines(text, false);
		for (std::string const &line : lines) {
			auto item = parseCommitItem(line);
			if (item) {
				list.push_back(*item);
			}
		}
	}

	items.setList(std::move(list));
	return items;
}

GitCommitItemList Git::log_all(GitHash const &id, int maxcount)
{
	return log_file(id.toString(), maxcount);
}

std::vector<GitHash> Git::rev_list_all(GitHash const &id, int maxcount)
{
	std::vector<GitHash> items;

	QString cmd = "rev-list --all -%1 %2";
	cmd = cmd.arg(maxcount).arg(QString::fromStdString(id.toString()));
	auto result = git_nolog(cmd.toStdString(), nullptr);

	if (result && result->exit_code() == 0) {
		auto &out = result->output();
		std::string_view v(out.data(), out.size());
		std::vector<std::string_view> lines = misc::splitLinesV(v, false);
		for (std::string_view const &line : lines) {
			GitHash hash(line);
			if (hash) {
				items.push_back(hash);
			}
		}
	}
	return items;
}

GitCommitItemList Git::log(int maxcount)
{
	return log_all({}, maxcount);
}

QDateTime Git::repositoryLastModifiedTime()
{
	if (isValidWorkingCopy()) {
		auto result = git("log --format=%at --all -1");
		std::string s(resultStdString(result));
		QDateTime dt = QDateTime::fromSecsSinceEpoch(misc::toi<uint64_t>(s));
		return dt;
	}
	return {};
}

std::optional<std::vector<GitFileItem>> Git::ls(const std::string &path)
{
	return session_->ls(path.c_str());
}

std::optional<std::vector<char>> Git::readfile(const std::string &path)
{
	return session_->readfile(path.c_str());
}

/**
 * @brief Git::log_signature
 * @param id コミットID
 * @return
 *
 * コミットに関連する署名情報を取得する
 */
std::optional<GitCommitItem> Git::log_signature(GitHash const &id)
{
	PROFILE;

	std::optional<GitCommitItem> ret;

	QString cmd = "log -1 --show-signature --pretty=format:\"id:%H#gpg:%G?#key:%GF#sub:%GP#trust:%GT##%s\" %1";
	cmd = cmd.arg(QString::fromStdString(id.toString()));
	auto result = git_nolog(cmd.toStdString(), nullptr);
	if (result && result->exit_code() == 0) {
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
		QString text = resultQString(result).trimmed();
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
				auto item = parseCommitItem(line.toStdString());
				if (item) {
					item->sign.text = gpgtext;
					ret = item;
				}
			}
		}
	}

	return ret;
}

std::optional<GitCommitItem> Git::parseCommit(std::vector<char> const &ba)
{
	std::vector<std::string_view> lines = misc::splitLinesV(ba, false);

	while (!lines.empty() && lines[lines.size() - 1].empty()) {
		lines.pop_back();
	}

	auto QSTR = [](std::string_view const &s){
		return QString(QString::fromUtf8(s.data(), s.size()));
	};

	GitCommitItem out;

	bool gpgsig = false;
	bool message = false;
	for (size_t i = 0; i < lines.size(); i++) {
		std::string_view const &line = lines[i];
		if (line.empty()) {
			i++;
			for (; i < lines.size(); i++) {
				std::string_view const &line = lines[i];
				if (!out.message.isEmpty()) {
					out.message.append('\n');
				}
				out.message.append(QSTR(line));
			}
			break;
		}
		if (gpgsig) {
			if (line[0] == ' ') {
				std::string_view s = line.substr(1);
				out.gpgsig += QSTR(s) + '\n';
				if (s == "-----END PGP SIGNATURE-----") {
					gpgsig = false;
				}
			}
			continue;
		}
		if (line.empty()) {
			message = true;
			continue;
		}
		if (message) {
			if (!out.message.isEmpty()) {
				out.message += '\n';
			}
			out.message += QSTR(line);
		}

		auto Starts = [&](char const *key, std::string_view *out){
			for (size_t i = 0; i < line.size(); i++) {
				if (line[i] == ' ' && key[i] == 0) {
					*out = line.substr(i + 1);
					return true;
				}
				if (line[i] != key[i]) break;
			}
			return false;
		};

		std::string_view val;
		if (Starts("tree", &val)) {
			out.tree = GitHash(val);
		} else if (Starts("parent", &val)) {
			out.parent_ids.push_back(GitHash(val));
		} else if (Starts("author", &val)) {
			std::vector<std::string_view> arr = misc::splitWords(val);
			int n = arr.size();
			if (n >= 4) {
				out.commit_date = QDateTime::fromSecsSinceEpoch(misc::toi<long long>(arr[n - 2]));
				out.email = QSTR(arr[n - 3]);
				if (out.email.startsWith('<') && out.email.endsWith('>')) {
					int m = out.email.size();
					out.email = out.email.mid(1, m - 2);
				}
				for (int i = 0; i < n - 3; i++) {
					if (!out.author.isEmpty()) {
						out.author += ' ';
					}
					out.author += QSTR(arr[i]);
				}
			}
		} else if (Starts("commiter", &val)) {
			// nop
		} else if (Starts("gpgsig", &val)) {
			if (val == "-----BEGIN PGP SIGNATURE-----") {
				out.has_gpgsig = true;
				out.gpgsig.append(QSTR(val));
				gpgsig = true;
			}
		}
	}

	if (!out.tree.isValid()) return std::nullopt;

	return out;
}

std::optional<GitCommitItem> Git::queryCommitItem(GitHash const &id)
{
	if (objectType(id) == "commit") {
		auto ba = cat_file(id);
		if (ba) {
			auto ret = parseCommit(*ba);
			if (ret) {
				ret->commit_id = id;
				return ret;
			}
		}
	}
	return std::nullopt;
}

GitCloneData Git::preclone(QString const &url, QString const &path)
{
	GitCloneData d;
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
		FileInfo info((QS)path);
		d.basedir = (QS)info.dir().path();
		d.subdir = (QS)info.fileName();
	}
	return d;
}

bool Git::clone(GitCloneData const &data, AbstractPtyProcess *pty)
{
	QString clone_to = data.basedir / data.subdir;
	gitinfo().working_repo_dir = misc::normalizePathSeparator(clone_to).toStdString();

	std::optional<GitResult> var;
	Dir cwd = Dir::current();

	auto DoIt = [&](){
		QString cmd = "clone --recurse-submodules --progress -j%1 \"%2\" \"%3\"";
		cmd = cmd.arg(std::thread::hardware_concurrency()).arg(data.url).arg(data.subdir);
		var = git_nochdir(cmd.toStdString(), pty);
	};

	if (pty) {
		pty->setChangeDir(data.basedir);
		DoIt();
	} else {
		if (Dir::setCurrent(data.basedir.toStdString())) {
			DoIt();
			Dir::setCurrent(cwd.path());
		}
	}

	return (bool)var;
}

std::string Git::submoduleURL(std::string const &path)
{
	std::string cmd = fmt("config --file .gitmodules submodule.%s.url")(encodeQuotedText(path));
	auto result = git(cmd);
	if (result) {
		return std::string{misc::trimmed(resultStdString(result))};
	}
	return {};
}

QList<GitSubmoduleItem> Git::submodules()
{
	QList<GitSubmoduleItem> mods;

	auto result = git("submodule");
	QString text = resultQString(result);
	QStringList lines = misc::splitLines(text);
	for (QString const &line : lines) {
		QString text = line;
		ushort c = text.utf16()[0];
		if (c == ' ' || c == '+' || c == '-') {
			text = text.mid(1);
		}
		QStringList words = misc::splitWords(text);
		if (words.size() >= 2) {
			GitSubmoduleItem submod;
			submod.id = GitHash(words[0]);
			submod.path = words[1].toStdString();
			if (GitHash::isValidID(submod.id)) {
				if (words.size() >= 3) {
					submod.refs = words[2].toStdString();
					if (misc::starts_with(submod.refs, '(') && misc::ends_with(submod.refs, ')')) {
						submod.refs = submod.refs.substr(1, submod.refs.size() - 2);
					}
				}
				submod.url = submoduleURL(submod.path);
				mods.push_back(submod);
			}
		}
	}
	return mods;
}

bool Git::submodule_add(const GitCloneData &data, bool force, AbstractPtyProcess *pty)
{
	bool ok = false;

	QString cmd = "submodule add";
	if (force) {
		cmd += " -f";
	}
	cmd += " \"%1\" \"%2\"";
	cmd = cmd.arg(data.url).arg(data.subdir);
	AbstractGitSession::Option opt;
	opt.errout = true;
	opt.pty = pty;
	ok = (bool)exec_git(cmd.toStdString(), opt);

	return ok;
}

bool Git::submodule_update(GitSubmoduleUpdateData const &data, AbstractPtyProcess *pty)
{
	QString cmd = "submodule update";
	if (data.init) {
		cmd += " --init";
	}
	if (data.recursive) {
		cmd += " --recursive";
	}
	AbstractGitSession::Option opt;
	opt.errout = true;
	opt.pty = pty;
	return (bool)exec_git(cmd.toStdString(), opt);
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

std::string Git::encodeQuotedText(std::string const &str)
{
	std::string out = "\"";
	for (char c : str) {
		if (c == '\"') { // triple quotes
			out += "\"\"\"";
		} else {
			out += c;
		}
	}
	out += "\"";
	return out;
}

bool Git::commit_(std::string const &msg, bool amend, bool sign, AbstractPtyProcess *pty)
{
	std::string cmd = "commit";
	if (amend) {
		cmd += " --amend";
	}
	if (sign) {
		cmd += " -S";
	}

	std::string text = (std::string)misc::trimmed(msg);
	if (text.empty()) {
		text = "no message";
	}
	text = encodeQuotedText(text);
	cmd += fmt(" -m %s")(text);

	AbstractGitSession::Option opt;
	opt.pty = pty;
	return (bool)exec_git(cmd, opt);
}

bool Git::commit(std::string const &text, bool sign, AbstractPtyProcess *pty)
{
	return commit_(text, false, sign, pty);
}

bool Git::commit_amend_m(std::string const &text, bool sign, AbstractPtyProcess *pty)
{
	return commit_(text, true, sign, pty);
}

bool Git::revert(GitHash const &id)
{
	QString cmd = "revert %1";
	cmd = cmd.arg(QString::fromStdString(id.toString()));
	return (bool)git(cmd.toStdString());
}

bool Git::push_u(bool set_upstream, const std::string &remote, const std::string &branch, bool force, AbstractPtyProcess *pty)
{
	if (remote.find('\"') != std::string::npos || branch.find('\"') != std::string::npos) {
		return false;
	}
	std::string cmd = "push";
	if (force) {
		cmd += " --force";
	}
	if (set_upstream && !remote.empty() && !branch.empty()) {
		cmd += fmt(" -u \"%1\" \"%2\"")(remote)(branch);
	}
	AbstractGitSession::Option opt;
	opt.pty = pty;
	return (bool)exec_git(cmd, opt);
}

bool Git::push_tags(AbstractPtyProcess *pty)
{
	QString cmd = "push --tags";
	AbstractGitSession::Option opt;
	opt.pty = pty;
	return (bool)exec_git(cmd.toStdString(), opt);
}

std::vector<GitFileStatus> Git::status_s_()
{
	std::vector<GitFileStatus> files;
	AbstractGitSession::Option opt;
	opt.use_cache = true;
	auto result = exec_git("status -s -u --porcelain", opt);
	if (result) {
		QString text = resultQString(result);
		QStringList lines = misc::splitLines(text);
		for (QString const &line : lines) {
			if (line.size() > 3) {
				GitFileStatus s(line);
				if (s.code() != GitFileStatus::Code::Unknown) {
					files.push_back(s);
				}
			}
		}
	}
	return files;
}

std::vector<GitFileStatus> Git::status_s()
{
	return status_s_();
}

std::string Git::objectType(GitHash const &id)
{
	if (GitHash::isValidID(id)) {
		auto result = git(("cat-file -t " + id.toString()));
		return (std::string)misc::trimmed(resultStdString(result));
	}
	return {};
}

std::optional<std::vector<char>> Git::cat_file(GitHash const &id)
{
	if (GitHash::isValidID(id)) {
		auto result = git(("cat-file -p " + id.toString()));
		return toByteArray(result);
	}
	return std::nullopt;
}

std::string Git::queryEntireCommitMessage(GitHash const &id)
{
	std::string ret;
	auto file = cat_file(id);
	if (file) {
		std::string message{file->data(), file->size()};
		std::vector<std::string_view> lines = misc::splitLinesV(message, false);
		bool header = true;
		for (std::string_view const &line : lines) {
			if (header) {
				if (line.empty()) {
					header = false;
				}
			} else {
				if (!ret.empty()) {
					ret += '\n';
				}
				ret += (std::string)line;
			}
		}
	}
	return ret;
}

void Git::resetFile(std::string const &path)
{
	git("checkout -- \"" + path + "\"");
}

void Git::resetAllFiles()
{
	git("reset --hard HEAD");
}

void Git::rm(std::string const &path, bool rm_real_file)
{
	git("rm " + path);

	if (rm_real_file) {
		session_->remove(path);
	}
}

void Git::add_A()
{
	git("add -A");
}

void Git::stage(std::string const &path)
{
	git("add " + path);
}

bool Git::stage(const std::vector<std::string> &paths, AbstractPtyProcess *pty)
{
	if (paths.empty()) return false;

	std::string cmd = "add";
	for (std::string const &path : paths) {
		cmd += ' ';
		cmd += '\"';
		cmd += path;
		cmd += '\"';
	}

	AbstractGitSession::Option opt;
	opt.pty = pty;
	return (bool)exec_git(cmd, opt);
}

void Git::unstage(std::string const &path)
{
	std::string cmd = "reset HEAD \"%s\"";
	cmd = fmt(cmd)(path);
	git(cmd);
}

void Git::unstage(const std::vector<std::string> &paths)
{
	if (paths.empty()) return;
	std::string cmd = "reset HEAD";
	for (std::string const &path : paths) {
		cmd += " \"";
		cmd += path;
		cmd += '\"';
	}
	git(cmd);
}

bool Git::unstage_all()
{
	return (bool)git("reset HEAD");
}

bool Git::pull(AbstractPtyProcess *pty)
{
	AbstractGitSession::Option opt;
	opt.pty = pty;
	return (bool)exec_git("pull", opt);
}

bool Git::fetch(AbstractPtyProcess *pty, bool prune)
{
	QString cmd = "fetch --tags -f -j%1";
	cmd = cmd.arg(std::thread::hardware_concurrency());
	if (prune) {
		cmd += " --prune";
	}
	AbstractGitSession::Option opt;
	opt.pty = pty;
	return (bool)exec_git(cmd.toStdString(), opt);
}

std::vector<std::string> Git::make_branch_list_(std::optional<GitResult> const &result)
{
	std::vector<std::string> list;
	std::vector<std::string> l = misc::splitLines(resultStdString(result), false);
	for (std::string const &s : l) {
		if (misc::starts_with(s, "* ")) list.push_back(s.substr(2));
		if (misc::starts_with(s, "  ")) list.push_back(s.substr(2));
	}
	return list;
}

void Git::createBranch(std::string const &name)
{
	git("branch " + name);
}

void Git::checkoutBranch(std::string const &name)
{
	git("checkout " + name);
}

void Git::cherrypick(std::string const &name)
{
	git("cherry-pick " + name);
}

std::string Git::getCherryPicking() const
{
	QString dir = QString::fromStdString(workingDir());
	QString path = dir / ".git/CHERRY_PICK_HEAD";
	QFile file(path);
	if (file.open(QFile::ReadOnly)) {
		QByteArray ba = file.readLine();
		std::string line{misc::trimmed(QBA(ba).sv())};
		if (GitHash::isValidID(line)) {
			return line;
		}
	}
	return {};
}

std::string Git::getMessage(std::string const &id)
{
	auto result = git("show --no-patch --pretty=%s " + id);
	return (std::string)misc::trimmed(resultStdString(result));
}

void Git::mergeBranch(std::string const &name, GitMergeFastForward ff, bool squash)
{
	std::string cmd = "merge ";
	switch (ff) {
	case GitMergeFastForward::No:
		cmd += "--no-ff ";
		break;
	case GitMergeFastForward::Only:
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

bool Git::deleteBranch(const std::string &name)
{
	return (bool)git(fmt("branch -D \"%s\"")(name));
}

bool Git::checkout(std::string const &branch_name, std::string const &id) // oops! `switch` is C's keyword
{
	// use `switch` instead of `checkout`
	std::string cmd;
	if (id.empty()) {
		cmd = fmt("switch %s")(branch_name);
	} else {
		cmd = fmt("switch -c %s %s")(branch_name)(id);
	}
	return (bool)git(cmd);
}

bool Git::checkout_detach(std::string const &id)
{
	return (bool)git("checkout --detach " + id);
}

void Git::rebaseBranch(std::string const &name)
{
	git("rebase " + name);
}

void Git::rebase_abort()
{
	git("rebase --abort");
}

std::vector<std::string> Git::getRemotes()
{
	std::vector<std::string> ret;
	auto result = git("remote");
	std::vector<std::string> lines = misc::splitLines(resultStdString(result), false);
	for (std::string const &line: lines) {
		if (!line.empty()) {
			ret.push_back(line);
		}
	}
	return ret;
}

GitUser Git::getUser(GitSource purpose)
{
	PROFILE;
	GitUser user;
	bool global = purpose == GitSource::Global;
	bool local = purpose == GitSource::Local;
	std::string arg1;
	if (global) arg1 = "--global";
	if (local) arg1 = "--local";
	AbstractGitSession::Option opt;
	opt.use_cache = true;
	opt.chdir = !global;
	{
#if 1
		auto user_name = std::async(std::launch::async, [&](){
			return exec_git(fmt("config %s user.name")(arg1), opt);
		});
		auto user_email = std::async(std::launch::async, [&](){
			return exec_git(fmt("config %s user.email")(arg1), opt);
		});
		user.name = misc::trimmed(resultStdString(user_name.get()));
		user.email = misc::trimmed(resultStdString(user_email.get()));
#else
		auto user_name = exec_git(fmt("config %s user.name")(arg1), opt);
		auto user_email = exec_git(fmt("config %s user.email")(arg1), opt);
		user.name = misc::trimmed(resultStdString(user_name));
		user.email = misc::trimmed(resultStdString(user_email));
#endif
	}
	return user;
}

void Git::setUser(const GitUser &user, bool global)
{
	bool unset = !user;
	std::string config = global ? "--global" : "--local";
	if (unset) {
		config += " --unset";
	}
	AbstractGitSession::Option opt;
	opt.chdir = !global;
	exec_git(fmt("config %s user.name %s")(config)(encodeQuotedText(user.name)), opt);
	exec_git(fmt("config %s user.email %s")(config)(encodeQuotedText(user.email)), opt);
}

bool Git::reset_head1()
{
	return (bool)git("reset HEAD~1");
}

bool Git::reset_hard()
{
	return (bool)git("reset --hard");
}

bool Git::clean_df()
{
	return (bool)git("clean -df");
}

bool Git::stash()
{
	return (bool)git("stash");
}

bool Git::stash_apply()
{
	return (bool)git("stash apply");
}

bool Git::stash_drop()
{
	return (bool)git("stash drop");
}

bool Git::rm_cached(std::string const &file)
{
	std::string cmd = fmt("rm --cached \"%s\"")(file);
	return (bool)git(cmd);
}

void Git::remote_v(std::vector<GitRemote> *out)
{
	out->clear();
	auto result = git("remote -v");
	QStringList lines = misc::splitLines(resultQString(result));
	for (QString const &line : lines) {
		int i = line.indexOf('\t');
		int j = line.indexOf(" (");
		if (i > 0 && i < j) {
			GitRemote r;
			r.name = line.mid(0, i).toStdString();
			r.ssh_key = gitinfo().ssh_key_override;
			QString url = line.mid(i + 1, j - i - 1);
			QString type = line.mid(j + 1);
			if (type.startsWith('(') && type.endsWith(')')) {
				type = type.mid(1, type.size() - 2);
				if (type == "fetch") {
					r.url_fetch = url.toStdString();
				} else if (type == "push") {
					r.url_push = url.toStdString();
				}
			}
			out->push_back(r);
		}
	}
	std::sort(out->begin(), out->end(), [](GitRemote const &a, GitRemote const &b){
		return a.name < b.name;
	});
	size_t i = out->size();
	if (i > 1) {
		i--;
		while (i > 0) {
			i--;
			if ((*out)[i].name == (*out)[i + 1].name) {
				if ((*out)[i].url_fetch.empty()) {
					(*out)[i].url_fetch = (*out)[i + 1].url_fetch;
				}
				if ((*out)[i].url_push.empty()) {
					(*out)[i].url_push = (*out)[i + 1].url_push;
				}
				out->erase(out->begin() + i + 1);
			}
		}
	}
}

void Git::setRemoteURL(GitRemote const &remote)
{
	QString cmd = "remote set-url %1 %2";
	cmd = cmd.arg(encodeQuotedText(QString::fromStdString(remote.name))).arg(encodeQuotedText(QString::fromStdString(remote.url_fetch)));
	git(cmd.toStdString());
}

void Git::addRemoteURL(GitRemote const &remote)
{
	QString cmd = "remote add \"%1\" \"%2\"";
	cmd = cmd.arg(encodeQuotedText(QString::fromStdString(remote.name))).arg(encodeQuotedText(QString::fromStdString(remote.url_fetch)));
	gitinfo().ssh_key_override = remote.ssh_key;
	git(cmd.toStdString());
}

void Git::removeRemote(std::string const &name)
{
	std::string cmd = "remote remove ";
	cmd += encodeQuotedText(name);
	git(cmd);
}

bool Git::reflog(ReflogItemList *out, int maxcount)
{
	out->clear();
	QString cmd = "reflog --no-abbrev --raw -n %1";
	cmd = cmd.arg(maxcount);
	auto result = git(cmd.toStdString());
	if (!result) return false;
	std::vector<char> ba = toByteArray(result);
	if (!ba.empty()) {
		GitReflogItem item;
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
							GitReflogItem::File file;
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
						item = GitReflogItem();
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



GitFileStatus::Code GitFileStatus::parseFileStatusCode(char x, char y)
{
	if (x == ' ' && y == 'A') return GitFileStatus::Code::AddedToIndex;
	if (x == ' ' && (y == 'M' || y == 'D')) return GitFileStatus::Code::NotUpdated;
	if (x == 'M' && (y == 'M' || y == 'D' || y == ' ')) return GitFileStatus::Code::UpdatedInIndex;
	if (x == 'A' && (y == 'M' || y == 'D' || y == ' ')) return GitFileStatus::Code::AddedToIndex;
	if (x == 'D' && (y == 'M' || y == ' ')) return GitFileStatus::Code::DeletedFromIndex;
	if (x == 'R' && (y == 'M' || y == 'D' || y == ' ')) return GitFileStatus::Code::RenamedInIndex;
	if (x == 'C' && (y == 'M' || y == 'D' || y == ' ')) return GitFileStatus::Code::CopiedInIndex;
	if (x == 'C' && (y == 'M' || y == 'D' || y == ' ')) return GitFileStatus::Code::CopiedInIndex;
	if (x == 'D' && y == 'D') return GitFileStatus::Code::Unmerged_BothDeleted;
	if (x == 'A' && y == 'U') return GitFileStatus::Code::Unmerged_AddedByUs;
	if (x == 'U' && y == 'D') return GitFileStatus::Code::Unmerged_DeletedByThem;
	if (x == 'U' && y == 'A') return GitFileStatus::Code::Unmerged_AddedByThem;
	if (x == 'D' && y == 'U') return GitFileStatus::Code::Unmerged_DeletedByUs;
	if (x == 'A' && y == 'A') return GitFileStatus::Code::Unmerged_BothAdded;
	if (x == 'U' && y == 'U') return GitFileStatus::Code::Unmerged_BothModified;
	if (x == '?' && y == '?') return GitFileStatus::Code::Untracked;
	if (x == '!' && y == '!') return GitFileStatus::Code::Ignored;
	return GitFileStatus::Code::Unknown;
}

void GitFileStatus::parse(QString const &text)
{
	data = Data();
	if (text.size() > 3) {
		ushort const *p = text.utf16();
		if (p[2] == ' ') {
			data.code_x = p[0];
			data.code_y = p[1];

			int i = text.indexOf(" -> ", 3);
			if (i > 3) {
				data.rawpath1 = text.mid(3, i - 3).toStdString();
				data.rawpath2 = text.mid(i + 4).toStdString();
				data.path1 = gitTrimPath(data.rawpath1);
				data.path2 = gitTrimPath(data.rawpath2);
			} else {
				data.rawpath1 = text.mid(3).toStdString();
				data.path1 = gitTrimPath(data.rawpath1);
				data.path2 = {};
			}

			data.code = parseFileStatusCode(data.code_x, data.code_y);
		}
	}
}

std::vector<char> Git::blame(std::string const &path)
{
	std::string cmd = "blame --porcelain --abbrev=40 \"%s\"";
	cmd = fmt(cmd)(path);
	auto result = git(cmd);
	if (result) {
		return toByteArray(result);
	}
	return {};
}

QList<Git::RemoteInfo> Git::ls_remote()
{
	QList<RemoteInfo> list;
	std::string cmd = "ls-remote";
	auto result = git(cmd);
	if (result) {
		std::vector<std::string> lines = misc::splitLines(resultStdString(result), false);
		for (std::string const &line : lines) {
			std::vector<std::string_view> words = misc::splitWords(line);
			RemoteInfo info;
			info.commit_id = std::string{words[0]};
			if (words.size() == 2 && GitHash::isValidID(info.commit_id)) {
				info.name = std::string{words[1]};
				list.push_back(info);
			}
		}
	}
	return list;
}

std::string Git::signingKey(GitSource purpose)
{
	std::string arg1;
	if (purpose == GitSource::Global) arg1 = "--global";
	if (purpose == GitSource::Local) arg1 = "--local";
	std::string cmd = "config %s user.signingkey";
	cmd = fmt(cmd)(arg1);
	AbstractGitSession::Option opt;
	opt.chdir = purpose != GitSource::Global;
	auto result = exec_git(cmd, opt);
	if (result) {
		return (std::string)misc::trimmed(resultStdString(result));
	}
	return {};
}

bool Git::setSigningKey(std::string const &id, bool global)
{
	for (auto i : id) {
		if (!QChar(i).isLetterOrNumber()) return false;
	}

	std::string cmd = "config %s %s user.signingkey %s";
	cmd = fmt(cmd)(global ? "--global" : "--local")(id.empty() ? "--unset" : "")(id);
	AbstractGitSession::Option opt;
	opt.chdir = !global;
	return (bool)exec_git(cmd, opt);
}

GitSignPolicy Git::signPolicy(GitSource source)
{
	std::string arg1;
	if (source == GitSource::Global) arg1 = "--global";
	if (source == GitSource::Local)  arg1 = "--local";
	std::string cmd = fmt("config %s commit.gpgsign")(arg1);
	AbstractGitSession::Option opt;
	opt.chdir = source != GitSource::Global;
	auto result = exec_git(cmd, opt);
	if (result) {
		std::string t{misc::trimmed(resultStdString(result))};
		if (t == "false") return GitSignPolicy::False;
		if (t == "true")  return GitSignPolicy::True;
	}
	return GitSignPolicy::Unset;
}

bool Git::setSignPolicy(GitSource source, GitSignPolicy policy)
{
	std::string arg1;
	if (source == GitSource::Global) arg1 = "--global";
	if (source == GitSource::Local)  arg1 = "--local";
	std::string arg2;
	std::string arg3;
	if (policy == GitSignPolicy::False) {
		arg3 = "false";
	} else if (policy == GitSignPolicy::True) {
		arg3 = "true";
	} else {
		arg2 = "--unset";
	}
	std::string cmd = fmt("config %s %s commit.gpgsign %s")(arg1)(arg2)(arg3);
	AbstractGitSession::Option opt;
	opt.chdir = source != GitSource::Global;
	return (bool)exec_git(cmd, opt);
}

bool Git::configGpgProgram(std::string const &path, bool global)
{
	std::string cmd = "config ";
	if (global) {
		cmd += "--global ";
	}
	if (path.empty()) {
		cmd += "--unset ";
	}
	cmd += "gpg.program ";
	if (!path.empty()) {
		cmd += fmt("\"%s\"")(path);
	}
	return (bool)git_nochdir(cmd, nullptr);
}

// GitDiff

void GitDiff::makeForSingleFile(GitDiff *diff, QString const &id_a, QString const &id_b, QString const &path, QString const &mode)
{
	diff->diff = QString("diff --git a/%1 b/%2").arg(path).arg(path);
	diff->index = QString("index %1..%2 %3").arg(id_a).arg(id_b).arg(0);
	diff->blob.a_id_or_path = id_a;
	diff->blob.b_id_or_path = id_b;
	diff->path = path;
	diff->mode = mode;
	diff->type = GitDiff::Type::Create;
}

//

void parseDiff(std::string_view const &s, GitDiff const *info, GitDiff *out)
{
	std::vector<std::string_view> lines = misc::splitLinesV(s, false);

	out->diff = QString("diff --git ") + ("a/" + info->path) + ' ' + ("b/" + info->path);
	out->index = QString("index ") + info->blob.a_id_or_path + ".." + info->blob.b_id_or_path + ' ' + info->mode;
	out->path = info->path;
	out->blob = info->blob;

	bool atat = false;
	for (std::string_view const &v : lines) {
		std::string line = std::string{v};
		int c = line[0] & 0xff;
		if (c == '@') {
			if (strncmp(line.c_str(), "@@ ", 3) == 0) {
				out->hunks.push_back(GitHunk());
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


void parseGitSubModules(const QByteArray &ba, QList<GitSubmoduleItem> *out)
{
	*out = {};
	QStringList lines = misc::splitLines(QString::fromUtf8(ba));
	GitSubmoduleItem submod;
	auto Push = [&](){
		if (!submod.name.empty()) {
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
				submod.name = line.mid(i, j - i).toStdString();
			}
		} else {
			int i = line.indexOf('=');
			if (i > 0) {
				QString key = line.mid(0, i).trimmed();
				QString val = line.mid(i + 1).trimmed();
				if (key == "path") {
					submod.path = val.toStdString();
				} else if (key == "url") {
					submod.url = val.toStdString();
				}
			}
		}
	}
	Push();
}

std::shared_ptr<AbstractGitSession> GitContext::connect() const
{
#if 1
	GitBasicSession::Commands cmds;
	cmds.git_command = git_command;
	cmds.ssh_command = ssh_command;
	return std::make_shared<GitBasicSession>(cmds);
#else
#ifdef UNSAFE_ENABLED
	auto ret = std::make_shared<GitRemoteSshSession>();
	std::shared_ptr<SshConnection> ssh = std::make_shared<SshConnection>();
	std::string host = "192.168.0.80";
	int port = 22;
	bool passwd = false;
	std::string uid;
	std::string pwd;
	ssh->connect(host, port, passwd, uid, pwd);
	ret->connect(ssh, "/usr/bin/git");
	return ret;
#endif
#endif
}
