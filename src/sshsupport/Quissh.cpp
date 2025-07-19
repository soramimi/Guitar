#ifdef UNSAFE_ENABLED
#include "Quissh.h"
#include "../common/joinpath.h"
#include <fcntl.h>
#include <sys/stat.h>

#include <libssh/libssh.h>
#include <libssh/sftp.h>

#ifdef _MSC_VER
#include <io.h>
#else
#include <unistd.h>
#endif

// パスの安全性をチェックする関数群
namespace {

// 危険なパスパターンをチェック
bool is_dangerous_patterns(std::string_view const &part)
{
	static char const *special_names[] = {
		"CON", "PRN", "AUX", "NUL",  // Windows予約名
		"COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
		"LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
		nullptr
	};

	size_t n = part.size();
	if (n > 0) {
		if (std::isspace((unsigned char)part[0]) || std::isspace((unsigned char)part[n - 1])) {
			return true; // 前後に空白がある場合は危険
		}

		// NULL文字、制御文字、危険な文字をチェック
		for (size_t i = 0; i < n; i++) {
			int c = (unsigned char)part[i];
			if (c == '\0' || c < 32 || c == 127) {
				return true;
			}
			if (c == '<' || c == '>' || c == '|' || c == '"' || c == '*' || c == '?') {
				return true;
			}
		}

		// 特別な名前をチェック
		for (char const **p = special_names; *p; p++) {
			if (part == *p) return true;
		}
	}

	return false;
}

}

// パスの安全性を検証する関数
std::string normalize_path(char const *path)
{
	if (!path || !*path) return {}; // 空のパスは無効;

	bool absolute_path = false;

	std::vector<std::string_view> parts;
	{
		char const *begin = path;
		char const *end = begin + strlen(path);
		char const *left = begin;
		char const *right = begin;
		while (1) {
			int c = 0;
			if (right < end) {
				c = (unsigned char)*right;
			}
			if (c == '/' || c == '\\' || c == 0) {
				std::string_view v(left, right - left);
				if (v == "..") {
					if (parts.empty()) return {}; // 親ディレクトリへの移動は許可しない
					parts.pop_back();
				} else if (v.empty()) {
					if (parts.empty()) {
						absolute_path = true;
					}
				} else {
					if (is_dangerous_patterns(v)) return {}; // 危険なパターンを含む
					parts.push_back(v);
				}
				if (c == 0) break;
				right++;
				left = right;
			} else {
				right++;
			}
		}
	}

	std::string newpath;
	for (size_t i = 0; i < parts.size(); ++i) {
		if (i > 0 || absolute_path) {
			newpath += '/';
		}
		newpath.append(parts[i]);
	}
	
	// 長すぎるパスは拒否
	if (newpath.size() > 4096) {
		return {};
	}
	
	return newpath;
}

bool is_safe_path(char const *path)
{
	std::string path2 = normalize_path(path);
	return !path2.empty(); // 正規化されたパスが空でない場合は安全
}

// パスを安全に正規化する関数
std::string safe_normalize_path(char const *path)
{
	if (!is_safe_path(path)) {
		return {};  // 危険なパスは空文字列を返す
	}
	return normalize_path(path);
}

// コマンドの安全性をチェックする関数群
namespace {
// 危険なコマンド文字をチェック
bool has_dangerous_command_chars(char const *command)
{
	// コマンドインジェクションで使用される危険な文字
	static char const dangerous_chars[] = ";&|`$><\\";

	for (size_t i = 0; command[i]; i++) {
		int c = (unsigned char)command[i];
		if (c == ' ' || c == '\t') continue; // 空白文字は無視
		if (c >= 0 && c < 0x20) return true; // 制御文字は危険
		if (strchr(dangerous_chars, c)) return true; // 危険な文字が含まれている
	}
	return false;
}

// 危険なコマンドパターンをチェック
bool has_dangerous_command_patterns(char const *command)
{
	static const char *dangerous_patterns[] = {
		"&&",     // AND演算子
		"||",     // OR演算子
		">>",     // 追記リダイレクト
		"<<",     // ヒアドキュメント
		"$((",    // 算術展開
		"$(",     // コマンド置換
		"${",     // 変数展開
		"2>",     // エラーリダイレクト
		"&>",     // 全リダイレクト
		"|&",     // パイプとエラー
		nullptr
	};

	for (size_t i = 0; dangerous_patterns[i]; i++) {
		if (strstr(command, dangerous_patterns[i])) {
			return true;
		}
	}
	return false;
}

} // namespace


// コマンドを安全にサニタイズする関数
std::string sanitize_command(char const *command, std::set<std::string> const &allowed_commands)
{
	if (!command || !*command) return {}; // 空のコマンドは無効

	size_t len = strlen(command);

	// 長すぎるコマンドは拒否
	if (len > 1000) {
		return {};
	}

	// 危険な文字をチェック
	if (has_dangerous_command_chars(command)) {
		return {};
	}

	// 危険なパターンをチェック
	if (has_dangerous_command_patterns(command)) {
		return {};
	}

	// コマンドと引数を分割
	size_t i = 0;
	while (i < len && std::isspace((unsigned char)command[i])) {
		i++;
	}
	size_t j = i;
	while (j < len && !std::isspace((unsigned char)command[j])) {
		j++;
	}
	std::string cmd(command + i, j - i);
	std::string arg(command + j, len - j);

	std::string cmd2 = cmd;
	if (cmd2.size() > 2) {
		if (cmd2[0] == '\"' && cmd2[cmd2.size() - 1] == '\"') {
			cmd2 = cmd2.substr(1, cmd2.size() - 2); // 引用符を削除
		}
	}
	if (cmd2.empty()) return {};  // 無効なコマンド

	// ホワイトリストに基づいてコマンドをチェック
	if (allowed_commands.find(cmd2) == allowed_commands.end()) {
		return {};
	}

	return cmd + arg;
}

std::string to_string(ssh_string s)
{
	if (s) {
		void *p = ssh_string_data(s);
		size_t n = ssh_string_len(s);
		return { (char const *)p, n };
	}
	return {};
}

std::string to_string(char const *s)
{
	if (s) {
		return s;
	}
	return {};
}

struct Quissh::Auth {

	ssh_session session = nullptr;

	Auth(ssh_session session);

	int operator()(PasswdAuth &auth);

	int operator()(PubkeyAuth &auth);

	int auth(AuthVar &auth);
};

struct Quissh::SftpSimpleCommand {
	Quissh *that;
	std::string name_;
	SftpSimpleCommand(Quissh *that, std::string name);
	int operator()(MKDIR &cmd);
	int operator()(RMDIR &cmd);
	int operator()(UNLINK &cmd);
	bool visit(SftpCmd &cmd);
};

struct Quissh::Private {
	std::string error;
	bool connected = false;
	bool sftp_connected = false;
	ssh_session session = nullptr;
	ssh_channel channel = nullptr;
	ssh_scp scp = nullptr;
	sftp_session sftp = nullptr;
	std::set<std::string> allowed_commands;
};

Quissh::Quissh()
	: m(new Private)
{
}

Quissh::~Quissh()
{
	close();
	delete m;
}

bool Quissh::channel_open()
{
	if (!m->session) {
		fprintf(stderr, "SSH session is not initialized.\n");
		return false;
	}
	if (!m->channel) {
		m->channel = ssh_channel_new(m->session);
		if (m->channel == NULL) {
			fprintf(stderr, "Failed to create SSH channel\n");
			return false;
		}
		int rc = ssh_channel_open_session(m->channel);
		if (rc != SSH_OK) {
			fprintf(stderr, "Failed to open SSH channel: %s\n", ssh_get_error(m->session));
			return false;
		}
	}
	return true;
}

void Quissh::channel_close()
{
	if (m->channel) {
		ssh_channel_close(m->channel);
		m->channel = nullptr;
	}
}

bool Quissh::exec(char const *command, std::function<bool(char const *, int)> writer)
{
	// コマンドの安全性をチェック
	if (!command || !*command) {
		fprintf(stderr, "Command is not specified.\n");
		return false;
	}

	CHANNEL ch(*this);
	if (!ch.open()) return false;

	ch.exec(command, writer);
	ch.close();
	return true;
}

void Quissh::clear_error()
{
	m->error.clear();
}

bool Quissh::open(char const *host, int port, AuthVar authdata)
{
	if (!host || !*host) {
		fprintf(stderr, "Host is not specified.\n");
		return false;
	}

	if (port <= 0 || port > 65535) {
		fprintf(stderr, "Invalid port number: %d\n", port);
		return false;
	}

	int rc;

	close();

	m->session = ssh_new();
	if (!m->session) {
		fprintf(stderr, "Failed to create SSH session.\n");
		return false;
	}

	ssh_options_set(m->session, SSH_OPTIONS_HOST, host);
	ssh_options_set(m->session, SSH_OPTIONS_PORT, &port);

	rc = ssh_connect(m->session);
	if (rc != SSH_OK) {
		fprintf(stderr, "Connection failed: %s\n", ssh_get_error(m->session));
		return false;
	}

	rc = Auth { m->session }.auth(authdata);
	if (rc != SSH_AUTH_SUCCESS) {
		fprintf(stderr, "Authentication failed: %s\n", ssh_get_error(m->session));
		return false;
	}

	m->connected = true;
	return true;
}

void Quissh::close()
{
	m->connected = false;

	if (m->channel) {
		ssh_channel_close(m->channel);
		ssh_channel_free(m->channel);
		m->channel = nullptr;
	}
	if (m->scp) {
		ssh_scp_close(m->scp);
		ssh_scp_free(m->scp);
		m->scp = nullptr;
	}
	sftp_close();
	if (m->session) {
		ssh_disconnect(m->session);
		ssh_free(m->session);
		m->session = nullptr;
	}
}

bool Quissh::is_connected() const
{
	return m->session && m->connected;
}

bool Quissh::sftp_unlink(char const *name)
{
	// パスの安全性をチェック
	if (!is_safe_path(name)) {
		fprintf(stderr, "Unsafe path detected\n");
		return false;
	}

	SftpCmd cmd = UNLINK {};
	return SftpSimpleCommand { this, name }.visit(cmd);
}

bool Quissh::sftp_mkdir(char const *name)
{
	// パスの安全性をチェック
	if (!is_safe_path(name)) {
		fprintf(stderr, "Unsafe path detected\n");
		return false;
	}
	
	SftpCmd cmd = MKDIR {};
	return SftpSimpleCommand { this, name }.visit(cmd);
}

bool Quissh::sftp_rmdir(char const *name)
{
	// パスの安全性をチェック
	if (!is_safe_path(name)) {
		fprintf(stderr, "Unsafe path detected\n");
		return false;
	}
	
	SftpCmd cmd = RMDIR {};
	return SftpSimpleCommand { this, name }.visit(cmd);
}

bool Quissh::scp_push_file(char const *path, std::function<int(char *, int)> reader, size_t size) // scp is deprecated
{
	// パスの安全性をチェック
	if (!is_safe_path(path)) {
		fprintf(stderr, "Unsafe path detected\n");
		return false;
	}

	std::string dir;
	std::string name = path;
	{
		auto i = name.rfind('/');
		if (i != std::string::npos) {
			dir = name.substr(0, i);
			name = name.substr(i + 1);
		}
	}

	int rc;

	m->scp = ssh_scp_new(m->session, SSH_SCP_WRITE | SSH_SCP_RECURSIVE, dir.c_str());
	if (!m->scp) {
		fprintf(stderr, "Failed to create SCP session: %s\n", ssh_get_error(m->session));
		return false;
	}

	rc = ssh_scp_init(m->scp);
	if (rc != SSH_OK) {
		fprintf(stderr, "Failed to initialize SCP: %s\n", ssh_get_error(m->session));
		return false;
	}

	rc = ssh_scp_push_file(m->scp, name.c_str(), size, 0644);
	if (rc != SSH_OK) {
		fprintf(stderr, "SCP push failed: %s\n", ssh_get_error(m->session));
		return false;
	}

	while (1) {
		char tmp[1024];
		int nbytes = reader(tmp, sizeof(tmp));
		if (nbytes < 1) break;
		ssh_scp_write(m->scp, tmp, nbytes);
	}

	fprintf(stderr, "File transferred successfully.\n");
	return true;
}

bool Quissh::sftp_open()
{
	if (!m->sftp) {
		m->sftp = sftp_new(m->session);
		if (!m->sftp) {
			fprintf(stderr, "Failed to create SFTP session: %s\n", ssh_get_error(m->session));
			return false;
		}

		int rc = sftp_init(m->sftp);
		if (rc != SSH_OK) {
			fprintf(stderr, "Failed to initialize SFTP: %s\n", ssh_get_error(m->session));
			return false;
		}
	}

	m->sftp_connected = true;

	return true;
}

bool Quissh::sftp_close()
{
	m->sftp_connected = false;
	if (!m->sftp) return false;
	sftp_free(m->sftp);
	m->sftp = nullptr;
	return true;
}

bool Quissh::is_sftp_connected() const
{
	return is_connected() && m->sftp && m->sftp_connected;
}

void Quissh::add_allowed_command(const std::string &command)
{
	m->allowed_commands.insert(command);
}

static Quissh::FileAttribute make_file_attribute(sftp_attributes const &st)
{
	if (!st) return {};

	Quissh::FileAttribute ret;
	ret.name = to_string(st->name);
	ret.longname = to_string(st->longname);
	ret.flags = st->flags;
	ret.type = st->type;
	ret.size = st->size;
	ret.uid = st->uid;
	ret.gid = st->gid;
	ret.owner = to_string(st->owner);
	ret.group = to_string(st->group);
	ret.permissions = st->permissions;
	ret.atime64 = st->atime64;
	ret.atime = st->atime;
	ret.atime_nseconds = st->atime_nseconds;
	ret.createtime = st->createtime;
	ret.createtime_nseconds = st->createtime_nseconds;
	ret.mtime64 = st->mtime64;
	ret.mtime = st->mtime;
	ret.mtime_nseconds = st->mtime_nseconds;
	ret.acl = to_string(st->acl);
	ret.extended_count = st->extended_count;
	ret.extended_type = to_string(st->extended_type);
	ret.extended_data = to_string(st->extended_data);
	return ret;
}

std::optional<std::vector<Quissh::FileAttribute>> Quissh::sftp_ls(char const *path)
{
	// パスの安全性をチェック
	if (!is_safe_path(path)) {
		fprintf(stderr, "Unsafe path detected\n");
		return std::nullopt;
	}

	std::vector<FileAttribute> ret;

	DIR d(*this);
	d.opendir(path);
	while (auto a = d.readdir()) {
		ret.push_back(*a);
	}
	d.closedir();

	return ret;
}

Quissh::FileAttribute Quissh::sftp_stat(std::string const &path)
{
	sftp_attributes st = ::sftp_stat(m->sftp, path.c_str());
	if (!st) return {};

	FileAttribute ret = make_file_attribute(st);
	sftp_attributes_free(st);
	return ret;
}

bool Quissh::sftp_push_file(char const *path, std::function<int(char *, int)> reader)
{
	// パスの安全性をチェック
	if (!is_safe_path(path)) {
		fprintf(stderr, "Unsafe path detected\n");
		return false;
	}

	if (!sftp_open()) return false;

	sftp_file file = ::sftp_open(m->sftp, path, O_WRONLY | O_CREAT, 0644);
	if (!file) {
		fprintf(stderr, "Failed to open file: %s\n", ssh_get_error(m->session));
		return false;
	}

	while (1) {
		char tmp[1024];
		int nbytes = reader(tmp, sizeof(tmp));
		if (nbytes < 1) break;
		sftp_write(file, tmp, nbytes);
	}

	::sftp_close(file);

	return true;
}

bool Quissh::scp_pull_file(std::function<bool(char const *, int)> writer) // scp is deprecated
{
	if (!sftp_open()) return false;

	std::string remote_file = "/tmp/example.txt";

	char buffer[1024];
	int nbytes;
	size_t total = 0;

	if (!m->scp) {
		m->scp = ssh_scp_new(m->session, SSH_SCP_READ, remote_file.c_str());
		if (m->scp == NULL) {
			fprintf(stderr, "Error initializing SCP session: %s\n", ssh_get_error(m->session));
			return false;
		}

		if (ssh_scp_init(m->scp) != SSH_OK) {
			fprintf(stderr, "Error initializing SCP: %s\n", ssh_get_error(m->session));
			return false;
		}
	}

	int rc;
	rc = ssh_scp_pull_request(m->scp);
	if (rc != SSH_SCP_REQUEST_NEWFILE) {
		fprintf(stderr, "Error requesting file: %s\n", ssh_get_error(m->session));
		return false;
	}
	auto size = ssh_scp_request_get_size(m->scp);
	// auto *filename = ssh_scp_request_get_filename(scp_);
	// auto mode = ssh_scp_request_get_permissions(scp_);

	if (ssh_scp_accept_request(m->scp) != SSH_OK) {
		fprintf(stderr, "Error accepting SCP request: %s\n", ssh_get_error(m->session));
		return false;
	}

	//
	total = 0;
	while (total < size) {
		nbytes = std::min(sizeof(buffer), size - total);
		nbytes = ssh_scp_read(m->scp, buffer, nbytes);
		if (nbytes < 0) {
			fprintf(stderr, "Error receiving file: %s\n", ssh_get_error(m->session)); // エラーメッセージを表示
			break;
		}

		if (writer(buffer, nbytes) < 1) break;
		total += nbytes;
	}

	return true;
}

bool Quissh::sftp_pull_file(char const *remote_path, std::function<int(char *, int)> writer)
{
	// リモートパスの安全性をチェック
	if (!is_safe_path(remote_path)) {
		fprintf(stderr, "Unsafe remote path detected\n");//, remote_path.c_str());
		return false;
	}

	if (!sftp_open()) return false;

	sftp_file file = ::sftp_open(m->sftp, remote_path, O_RDONLY, 0);
	if (!file) {
		fprintf(stderr, "Failed to open file: %s\n", ssh_get_error(m->session));
		return false;
	}

	char buffer[1024];
	int nbytes;
	while ((nbytes = sftp_read(file, buffer, sizeof(buffer))) > 0) {
		if (writer(buffer, nbytes) < 1) break;
	}

	::sftp_close(file);

	return true;
}

bool Quissh::push_file(const char *path, std::function<int(char *, int)> reader)
{
	return sftp_push_file(path, reader);
}

bool Quissh::pull_file(const char *remote_path, std::function<int(char const *, int)> writer)
{
	return sftp_pull_file(remote_path, writer);
}

std::optional<struct stat> Quissh::stat(std::string const &path)
{
	struct stat st = {};
	if (is_sftp_connected()) {
		sftp_attributes attr = ::sftp_stat(m->sftp, path.c_str());
		if (!attr) {
			fprintf(stderr, "Failed to stat file: %s\n", ssh_get_error(m->session));
			return st;
		}
		st.st_size = attr->size;
		st.st_mode = attr->permissions;
		sftp_attributes_free(attr);
		return st;
	}
	return std::nullopt;
}

Quissh::Auth::Auth(ssh_session session)
	: session(session)
{
}

int Quissh::Auth::operator()(PasswdAuth &auth)
{
	ssh_options_set(session, SSH_OPTIONS_USER, auth.uid.c_str());
	if (auth.pwd.empty()) {
		return ssh_userauth_none(session, NULL);
	} else {
		return ssh_userauth_password(session, NULL, auth.pwd.c_str());
	}
}

int Quissh::Auth::operator()(PubkeyAuth &auth)
{
	return ssh_userauth_publickey_auto(session, NULL, NULL);
}

int Quissh::Auth::auth(AuthVar &auth)
{
	return std::visit(*this, auth);
}

Quissh::SftpSimpleCommand::SftpSimpleCommand(Quissh *that, std::string name)
	: that(that)
	, name_(name)
{
}

int Quissh::SftpSimpleCommand::operator()(UNLINK &cmd)
{
	return ::sftp_unlink(that->m->sftp, name_.c_str());
}

int Quissh::SftpSimpleCommand::operator()(MKDIR &cmd)
{
	return ::sftp_mkdir(that->m->sftp, name_.c_str(), 0755);
}

int Quissh::SftpSimpleCommand::operator()(RMDIR &cmd)
{
	return ::sftp_rmdir(that->m->sftp, name_.c_str());
}

bool Quissh::SftpSimpleCommand::visit(SftpCmd &cmd)
{
	if (!that->is_sftp_connected()) {
		fprintf(stderr, "SFTP is not connected.\n");
		return false;
	}
	int rc = std::visit(*this, cmd);
	return rc == SSH_OK;
}

bool Quissh::SFTP::is_connected() const
{
	return ssh_.is_connected() && ssh_.is_sftp_connected();
}

bool Quissh::SFTP::push(char const *local_path, char const *remote_path)
{
	// ローカルパスとリモートパスの安全性をチェック
	if (!is_safe_path(local_path)) {
		ssh_.m->error = "Unsafe local path detected"; // + local_path;
		return false;
	}
	if (!is_safe_path(remote_path)) {
		ssh_.m->error = "Unsafe remote path detected"; // + remote_path;
		return false;
	}

	std::string remote_path2 = remote_path;

	ssh_.clear_error();

	auto Do = [&]() -> bool {
		std::string basename;
		{
			std::string local_path2(local_path);
			auto it = local_path2.rfind('/');
			basename = it == std::string::npos ? local_path2 : local_path2.substr(it + 1);
		}
		{
			auto Split = [&](std::string const &remote_path) {
				std::vector<std::string> parts;
				char const *begin = remote_path.c_str();
				char const *end = begin + remote_path.size();
				char const *left = begin;
				char const *right = begin;
				while (1) {
					int c = 0;
					if (right < end) {
						c = (unsigned char)*right;
					}
					if (c == '/' || c == 0) {
						parts.emplace_back(left, right - left);
						if (c == 0) break;
						right++;
						left = right;
					} else {
						right++;
					}
				}
				return parts;
			};

			std::vector<std::string> parts = Split(remote_path2);
			if (parts.empty()) {
				ssh_.m->error = "Invalid remote path";
				// ssh_.m->error += remote_path;
				return false;
			}
			if (!parts.back().empty()) {
				basename = parts.back();
			}
			parts.pop_back();

			remote_path2 = {};
			for (size_t i = 0; i < parts.size(); i++) {
				remote_path2 = std::string(remote_path2) / parts[i];
				auto atts = stat(remote_path2.c_str());
				if (atts.isdir()) continue;
				if (!ssh_.sftp_mkdir(remote_path2.c_str())) {
					ssh_.m->error = "Failed to create remote directory";
					// ssh_.m->error += remote_path;
					return false;
				}
			}
			remote_path2 = remote_path2 / basename;
		}

		struct stat st = {};
		if (::stat(local_path, &st) == 0) {
			int fd = ::open(local_path, O_RDONLY);
			if (fd < 0) {
				ssh_.m->error = "Failed to open local file";
				// ssh_.m->error += local_path;
				return false;
			}
			auto Reader = [&](char *ptr, int len) {
				int n = ::read(fd, ptr, len);
				return n;
			};
			bool r = push(remote_path2.c_str(), Reader);
			::close(fd);
			return r;
		} else {
			ssh_.m->error = "Failed to stat local file";
			// ssh_.m->error += local_path;
			return false;
		}
	};

	bool r = Do();
	if (!r) {
		fprintf(stderr, "%s\n", ssh_.m->error.c_str());
	}
	return r;
}

bool Quissh::SFTP::pull(char const *remote_path, char const *local_path)
{
	// リモートパスとローカルパスの安全性をチェック
	if (!is_safe_path(remote_path)) {
		ssh_.m->error = "Unsafe remote path detected"; // + remote_path;
		return false;
	}
	if (!is_safe_path(local_path)) {
		ssh_.m->error = "Unsafe local path detected"; // + local_path;
		return false;
	}

	ssh_.clear_error();

	auto Do = [&]() -> bool {
		std::string dir;
		std::string name = local_path;
		{
			auto i = name.rfind('/');
			if (i != std::string::npos) {
				dir = name.substr(0, i);
				name = name.substr(i + 1);
			}
		}

		bool ret = false;

		int fd = ::open(local_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fd != -1) {
			auto Writer = [&](char const *ptr, int len) {
				int n = ::write(fd, ptr, len);
				return n == len;
			};
			ret = ssh_.sftp_pull_file(remote_path, Writer);
			::close(fd);
		}

		return ret;
	};

	bool r = Do();
	if (!r) {
		fprintf(stderr, "%s\n", ssh_.m->error.c_str());
	}
	return r;

}

bool Quissh::FileAttribute::exists() const
{
	return type != SSH_FILEXFER_TYPE_UNKNOWN;
}

bool Quissh::FileAttribute::isfile() const
{
	return (type == SSH_FILEXFER_TYPE_REGULAR || type == SSH_FILEXFER_TYPE_SYMLINK);
}

bool Quissh::FileAttribute::isdir() const
{
	return type == SSH_FILEXFER_TYPE_DIRECTORY;
}

bool Quissh::FileAttribute::islink() const
{
	return type == SSH_FILEXFER_TYPE_SYMLINK;
}

//

struct Quissh::DIR::Private {
	sftp_dir dir = nullptr;
};

Quissh::DIR::DIR(Quissh &quissh)
	: m(new Private)
	, quissh_(quissh)
{
}

Quissh::DIR::~DIR()
{
	closedir();
	delete m;
}

bool Quissh::DIR::opendir(const char *path)
{
	// パスの安全性をチェック
	if (!path || !is_safe_path(path)) {
		fprintf(stderr, "Unsafe path detected\n");
		return false;
	}
	
	m->dir = sftp_opendir(quissh_.m->sftp, path);
	return (bool)m->dir;
}

void Quissh::DIR::closedir()
{
	if (m->dir) {
		int rc = sftp_closedir(m->dir);
		if (rc != SSH_OK) {
			fprintf(stderr, "Can't close directory: %s\n", ssh_get_error(quissh_.m->session));
		}
		m->dir = nullptr;
	}
}

std::optional<Quissh::FileAttribute> Quissh::DIR::readdir()
{
	if (quissh_.m->sftp && m->dir) {
		FileAttribute ret;
		sftp_attributes a = sftp_readdir(quissh_.m->sftp, m->dir);
		if (!a) return std::nullopt;
		ret = make_file_attribute(a);
		sftp_attributes_free(a);
		return ret;
	}
	return std::nullopt;
}

//

bool Quissh::CHANNEL::exec(const char *command, std::function<bool (const char *, int)> writer)
{
	int rc;

	std::string safe_cmd = sanitize_command(command, ssh_.m->allowed_commands);
	if (safe_cmd.empty()) {
		fprintf(stderr, "Unsafe command detected.\n");
		return false;
	}

	rc = ssh_channel_request_exec(ssh_.m->channel, safe_cmd.c_str());
	if (rc != SSH_OK) {
		fprintf(stderr, "Failed to execute command: %s\n", ssh_get_error(ssh_.m->session));
		return false;
	}

	char buffer[1024];
	int nbytes;

	while ((nbytes = ssh_channel_read(ssh_.m->channel, buffer, sizeof(buffer), 0)) > 0) {
		writer(buffer, nbytes);
	}

	return true;
}

//
#endif
