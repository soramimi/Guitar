#ifndef QUISSH_H
#define QUISSH_H
#ifdef UNSAFE_ENABLED

#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <stdint.h>
#include <set>

class Quissh {
	friend class DIR;
public:
	struct PasswdAuth {
		std::string uid;
		std::string pwd;
	};

	struct PubkeyAuth {
	};

	using AuthVar = std::variant<PasswdAuth, PubkeyAuth>;

	struct Auth;

	struct FileAttribute {
		std::string name;
		std::string longname;
		uint32_t flags = 0;
		uint8_t type = 5;//SSH_FILEXFER_TYPE_UNKNOWN;
		uint64_t size = 0;
		uint32_t uid = 0;
		uint32_t gid = 0;
		std::string owner;
		std::string group;
		uint32_t permissions = 0;
		uint64_t atime64 = 0;
		uint32_t atime = 0;
		uint32_t atime_nseconds = 0;
		uint64_t createtime = 0;
		uint32_t createtime_nseconds = 0;
		uint64_t mtime64 = 0;
		uint32_t mtime = 0;
		uint32_t mtime_nseconds = 0;
		std::string acl;
		uint32_t extended_count = 0;
		std::string extended_type;
		std::string extended_data;

		bool exists() const;
		bool isfile() const;
		bool isdir() const;
		bool islink() const;
	};
private:
	struct Private;
	Private *m;

	struct UNLINK {
	};
	struct MKDIR {
	};
	struct RMDIR {
	};
	typedef std::variant<UNLINK, MKDIR, RMDIR> SftpCmd;
	struct SftpSimpleCommand;

	void close_scp();
	void clear_error();
public:
	Quissh();
	~Quissh();
	Quissh(Quissh const &) = delete;
	Quissh &operator=(Quissh const &) = delete;
	Quissh(Quissh &&) = delete;
	Quissh &operator=(Quissh &&) = delete;

	bool open(const char *host, int port, AuthVar authdata);
	void close();

	bool is_connected() const;
	bool is_sftp_connected() const;

	void add_allowed_command(const std::string &command);

	bool exec(char const *cmd, std::function<bool (const char *, int)> writer);

	class CHANNEL {
	private:
		Quissh &ssh_;
	public:
		CHANNEL(Quissh &ssh)
			: ssh_(ssh)
		{
		}
		~CHANNEL()
		{
			close();
		}
		bool open()
		{
			return ssh_.channel_open();
		}
		void close()
		{
			ssh_.channel_close();
		}
		bool exec(char const *command, std::function<bool(char const *, int)> writer);
	};
private:
	bool sftp_unlink(const char *name);
	bool sftp_mkdir(const char *name);
	bool sftp_rmdir(const char *name);

	bool scp_push_file(char const *path, std::function<int (char *ptr, int len)> reader, size_t size);
	bool scp_pull_file(std::function<bool (char const *ptr, int len)> writer);

	bool sftp_open();
	bool sftp_close();

	bool sftp_push_file(char const *path, std::function<int (char *ptr, int len)> reader);
	bool sftp_pull_file(char const *remote_path, std::function<int (char *ptr, int len)> writer);

	std::optional<std::vector<FileAttribute>> sftp_ls(char const *path);
	FileAttribute sftp_stat(const std::string &path);

	bool channel_open();
	void channel_close();
public:
	bool push_file(char const *path, std::function<int (char *ptr, int len)> reader);
	bool pull_file(char const *remote_path, std::function<int (char const *ptr, int len)> writer);
	std::optional<struct stat> stat(std::string const &path);

	class SFTP {
	private:
		Quissh &ssh_;
	public:
		SFTP(Quissh &ssh)
			: ssh_(ssh)
		{
		}
		~SFTP()
		{
			close();
		}
		bool open()
		{
			return ssh_.sftp_open();
		}
		void close()
		{
			ssh_.sftp_close();
		}
		bool is_connected() const;
		bool unlink(char const *name)
		{
			return ssh_.sftp_unlink(name);
		}
		bool mkdir(char const *name)
		{
			return ssh_.sftp_mkdir(name);
		}
		bool rmdir(char const *name)
		{
			return ssh_.sftp_rmdir(name);
		}
		std::optional<std::vector<FileAttribute>> ls(char const *path)
		{
			return ssh_.sftp_ls(path);
		}
		bool push(char const *path, std::function<int (char *, int)> reader)
		{
			return ssh_.sftp_push_file(path, reader);
		}
		FileAttribute stat(char const *path)
		{
			return ssh_.sftp_stat(path);
		}
		bool push(char const *local_path, char const *remote_path);
		bool pull(char const *remote_path, std::function<int (char const *ptr, int len)> writer)
		{
			return ssh_.sftp_pull_file(remote_path, writer);
		}
		bool pull(char const *remote_path, char const *local_path);
	};

	class DIR {
	private:
		struct Private;
		Private *m;
		Quissh &quissh_;
		DIR(DIR const &) = delete;
		DIR &operator=(DIR const &) = delete;
		DIR(DIR &&) = delete;
		DIR &operator=(DIR &&) = delete;
	public:
		DIR(Quissh &quissh);
		~DIR();
		bool opendir(char const *path);
		void closedir();
		std::optional<FileAttribute> readdir();
	};

};

#endif
#endif // QUISSH_H
