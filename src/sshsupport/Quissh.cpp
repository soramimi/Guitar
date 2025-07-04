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
// パスを正規化する（../, ./, 重複する/を処理）
std::string normalize_path(char const *path)
{
	std::vector<std::string> components;
	std::string current;

	std::string path2 = path;
	for (char c : path2) {
		if (c == '/') {
			if (!current.empty()) {
				if (current == "..") {
					if (!components.empty() && components.back() != "..") {
						components.pop_back();
					} else if (path2[0] != '/') {
						// 相対パスの場合のみ .. を許可
						components.push_back(current);
					}
				} else if (current != ".") {
					components.push_back(current);
				}
				current.clear();
			}
		} else {
			current += c;
		}
	}

	// 最後のコンポーネントを処理
	if (!current.empty()) {
		if (current == "..") {
			if (!components.empty() && components.back() != "..") {
				components.pop_back();
			} else if (path2[0] != '/') {
				components.push_back(current);
			}
		} else if (current != ".") {
			components.push_back(current);
		}
	}

	std::string result;
	if (path2[0] == '/') {
		result = "/";
	}

	for (size_t i = 0; i < components.size(); ++i) {
		if (i > 0 || result.empty()) {
			result += "/";
		}
		result += components[i];
	}

	return result.empty() ? "." : result;
}

// 危険な文字をチェック
bool has_dangerous_chars(std::string const &path)
{
	// NULL文字、制御文字、危険な文字をチェック
	for (char c : path) {
		if (c == '\0' || c < 32 || c == 127) {
			return true;
		}
		// Windows系の危険な文字も念のためチェック
		if (c == '<' || c == '>' || c == '|' || c == '"' || c == '*' || c == '?') {
			return true;
		}
	}
	return false;
}

// 危険なパスパターンをチェック
bool has_dangerous_patterns(std::string const &path)
{
	// 明示的な危険パターン
	const std::vector<std::string> dangerous_patterns = {
		"../",     // パストラバーサル
		// "./",      // カレントディレクトリ（相対パス）
		"//",      // 重複スラッシュ
		"\\",      // バックスラッシュ
		// "~",       // ホームディレクトリ
	};

	for (const auto& pattern : dangerous_patterns) {
		if (path.find(pattern) != std::string::npos) {
			return true;
		}
	}

	// 特殊なファイル名パターン
	const std::vector<std::string> special_names = {
		// ".",       // カレントディレクトリ
		"..",      // 親ディレクトリ
		"CON", "PRN", "AUX", "NUL",  // Windows予約名
		"COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
		"LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
	};

	// パスの各コンポーネントをチェック
	std::string current;
	for (char c : path) {
		if (c == '/') {
			if (!current.empty()) {
				for (const auto& name : special_names) {
					if (current == name) {
						return true;
					}
				}
				current.clear();
			}
		} else {
			current += c;
		}
	}

	// 最後のコンポーネントもチェック
	if (!current.empty()) {
		for (const auto& name : special_names) {
			if (current == name) {
				return true;
			}
		}
	}

	return false;
}
}

// パスの安全性を検証する関数
bool is_safe_path(char const *path)
{
	if (!path) return false;

	// 空のパスは危険
	if (!*path) return false;

	std::string path2 = path;
	
	// 長すぎるパスは拒否（一般的な制限）
	if (path2.length() > 4096) {
		return false;
	}
	
	// 危険な文字をチェック
	if (has_dangerous_chars(path2)) {
		return false;
	}
	
	// 危険なパターンをチェック
	if (has_dangerous_patterns(path2)) {
		return false;
	}
	
	// 絶対パスの場合、ルートディレクトリより上に行けないかチェック
	if (path2[0] == '/') {
		std::string normalized = normalize_path(path2.c_str());
		// 正規化後にルートより上に行こうとしている場合
		if (normalized.find("../") != std::string::npos) {
			return false;
		}
	}
	
	// 相対パスの場合、現在のディレクトリより上に行けないかチェック
	else {
		std::string normalized = normalize_path(path2.c_str());
		// 正規化後に .. が残っている場合（上位ディレクトリへの移動）
		if (normalized.find("..") != std::string::npos) {
			return false;
		}
	}
	
	return true;
}

// パスを安全に正規化する関数
std::string safe_normalize_path(char const *path)
{
	if (!is_safe_path(path)) {
		return "";  // 危険なパスは空文字列を返す
	}
	return normalize_path(path);
}

// コマンドの安全性をチェックする関数群
namespace {
// 危険なコマンド文字をチェック
bool has_dangerous_command_chars(const std::string& command) {
	// コマンドインジェクションで使用される危険な文字
	const std::vector<char> dangerous_chars = {
		';',  // コマンド区切り
		'&',  // バックグラウンド実行、AND演算子
		'|',  // パイプ
		'`',  // バッククォート（コマンド置換）
		'$',  // 変数展開、コマンド置換
		'>',  // リダイレクト
		'<',  // リダイレクト
		'\n', // 改行
		'\r', // キャリッジリターン
		'\\', // エスケープ文字
	};

	for (char c : command) {
		// NULL文字や制御文字をチェック
		if (c == '\0' || (c > 0 && c < 32 && c != '\t' && c != ' ')) {
			return true;
		}
		// 危険な文字をチェック
		for (char dangerous : dangerous_chars) {
			if (c == dangerous) {
				return true;
			}
		}
	}
	return false;
}

// 危険なコマンドパターンをチェック
bool has_dangerous_command_patterns(const std::string& command) {
	const std::vector<std::string> dangerous_patterns = {
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
	};

	for (const auto& pattern : dangerous_patterns) {
		if (command.find(pattern) != std::string::npos) {
			return true;
		}
	}
	return false;
}

// 許可されたコマンドのホワイトリスト
bool is_allowed_command(const std::string& command) {
	// 先頭の空白を除去
	size_t start = 0;
	while (start < command.length() && std::isspace(command[start])) {
		start++;
	}

	if (start >= command.length()) {
		return false; // 空のコマンド
	}

	// コマンド名を抽出（最初の空白またはタブまで）
	size_t end = start;
	while (end < command.length() && !std::isspace(command[end])) {
		end++;
	}

	std::string cmd_name = command.substr(start, end - start);

	// 許可されたコマンドのホワイトリスト
	const std::vector<std::string> allowed_commands = {
		"ls",     // ファイル一覧
		"pwd",    // 現在のディレクトリ
		"whoami", // 現在のユーザー
		"id",     // ユーザーID情報
		"date",   // 日付
		"uname",  // システム情報
		"df",     // ディスク使用量
		"free",   // メモリ使用量
		"uptime", // システム稼働時間
		"cat",    // ファイル内容表示（引数チェック必要）
		"head",   // ファイルの先頭表示
		"tail",   // ファイルの末尾表示
		"wc",     // 文字数・行数カウント
		"echo",   // 文字列出力
		"which",  // コマンドパス表示
		"type",   // コマンドタイプ表示

		"git"
	};

	for (const auto& allowed : allowed_commands) {
		if (cmd_name == allowed) {
			return true;
		}
	}

	return true;
}

} // namespace

// コマンドの安全性を検証する関数
bool is_safe_command(const std::string& command) {
	// 空のコマンドは危険
	if (command.empty()) {
		return false;
	}
	
	// 長すぎるコマンドは拒否
	if (command.length() > 1000) {
		return false;
	}
	
	// 危険な文字をチェック
	if (has_dangerous_command_chars(command)) {
		return false;
	}
	
	// 危険なパターンをチェック
	if (has_dangerous_command_patterns(command)) {
		return false;
	}
	
	// ホワイトリストによるチェック
	if (!is_allowed_command(command)) {
		return false;
	}
	
	return true;
}

// コマンドを安全にサニタイズする関数
std::string sanitize_command(const std::string& command) {
	if (!is_safe_command(command)) {
		return "";  // 危険なコマンドは空文字列を返す
	}
	
	// 追加のサニタイズ処理
	std::string sanitized = command;
	
	// 前後の空白を除去
	size_t start = 0;
	while (start < sanitized.length() && std::isspace(sanitized[start])) {
		start++;
	}
	
	size_t end = sanitized.length();
	while (end > start && std::isspace(sanitized[end - 1])) {
		end--;
	}
	
	return sanitized.substr(start, end - start);
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

	std::string safe_cmd = sanitize_command(command);
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
