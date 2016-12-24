#ifndef LIBGIT2_H
#define LIBGIT2_H

#include "Git.h"
#include <memory>

#if USE_LIBGIT2
#include <git2.h>

class LibGit2 {
public:
	static void init();
	static void shutdown();
public:
	class Repository;
	class Commit;
	struct UserInfo {
		std::string name;
		std::string email;
	};
public:
	static Repository openRepository(const std::string &dir);
	static UserInfo get_global_user_info();
	static std::string get_error_message(int err);
};

class GitOID {
private:
	std::shared_ptr<git_oid> sharedptr;
public:
	GitOID();
	GitOID(git_oid const &r);
	GitOID(std::string const &s);

	git_oid *ptr()
	{
		git_oid *p = sharedptr.get();
		if (!p) {
			p = new git_oid;
			memset(p, 0, sizeof(git_oid));
			sharedptr = std::shared_ptr<git_oid>(p);
		}
		return p;
	}
	git_oid const *ptr() const
	{
		return const_cast<GitOID *>(this)->ptr();
	}
	std::string to_string() const;
};

class LibGit2::Repository {
private:
	std::shared_ptr<git_repository *> sharedptr;
	static void custom_deleter(git_repository **p)
	{
		git_repository_free(*p);
	}
	static std::string process_diff(git_diff *diff);
	static int status_cb_(const char *path, unsigned int status_flags, void *payload);
	static int diff_line_handler(const git_diff_delta *delta, const git_diff_hunk *hunk, const git_diff_line *line, void *payload);
public:
	Repository()
	{
	}
	~Repository()
	{
	}
	bool open(std::string const &path);
	git_repository *ptr()
	{
		git_repository **p = sharedptr.get();
		return p ? *p : nullptr;
	}
	git_repository *ptr() const
	{
		return const_cast<Repository *>(this)->ptr();
	}
	Commit lookup_commit(const git_oid *oid) const;
	Commit lookup_commit(GitOID const &oid) const;
	Commit revparse_head();

	void test();
	git_commit *revparse_single_to_commit(const std::string &spec) const;
	git_tree *revparse_single_to_tree(const std::string &spec) const;

	std::string diff_head_to_workdir();
	std::string diff2(const std::string &older, const std::string &newer);
	Git::CommitItemList log(size_t maxcount);
	QByteArray cat_file(const std::string &id);
	QList<Git::Branch> branches();
	Git::FileStatusList status();
	bool commit(const std::string &message);
	bool push();
	bool fetch();
	void revert_a_file(const std::string &path);
};


class LibGit2::Commit {
	friend class Repository;
private:
	std::shared_ptr<git_commit *> sharedptr;
	static void custom_deleter(git_commit **p)
	{
		git_commit_free(*p);
	}
public:
	Commit(git_commit *p = nullptr)
	{
		if (p) sharedptr = std::shared_ptr<git_commit *>(new git_commit *(p), custom_deleter);
	}
	~Commit()
	{
	}
	git_commit *ptr()
	{
		git_commit **p = sharedptr.get();
		return p ? *p : nullptr;
	}
	git_commit const *ptr() const
	{
		return const_cast<Commit *>(this)->ptr();
	}
	GitOID get_commit_id() const;
	size_t get_parent_count() const;
	Commit get_parent(size_t i) const;
	std::string get_committer_name() const;
	std::string get_message() const;
	time_t get_time() const;
};

#endif // USE_LIBGIT2

#endif // LIBGIT2_H
