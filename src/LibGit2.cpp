#include "LibGit2.h"

#if USE_LIBGIT2

#include <QDebug>
#include "common/misc.h"


std::string LibGit2::get_error_message(int err)
{
	switch (err) {
	case GIT_OK:                   return "No error";
	case GIT_ERROR:                return "Generic error";
	case GIT_ENOTFOUND:            return "Requested object could not be found";
	case GIT_EEXISTS:              return "Object exists preventing operation";
	case GIT_EAMBIGUOUS:           return "More than one object matches";
	case GIT_EBUFS:                return "Output buffer too short to hold data";
	case GIT_EUSER:                return "EUSER";
	case GIT_EBAREREPO:            return "Operation not allowed on bare repository";
	case GIT_EUNBORNBRANCH:        return "HEAD refers to branch with no commits";
	case GIT_EUNMERGED:            return "Merge in progress prevented operation";
	case GIT_ENONFASTFORWARD:      return "Reference was not fast-forwardable";
	case GIT_EINVALIDSPEC:         return "Name/ref spec was not in a valid format";
	case GIT_ECONFLICT:            return "Checkout conflicts prevented operation";
	case GIT_ELOCKED:              return "Lock file prevented operation";
	case GIT_EMODIFIED:            return "Reference value does not match expected";
	case GIT_EAUTH:                return "Authentication error";
	case GIT_ECERTIFICATE:         return "Server certificate is invalid";
	case GIT_EAPPLIED:             return "Patch/merge has already been applied";
	case GIT_EPEEL:                return "The requested peel operation is not possible";
	case GIT_EEOF:                 return "Unexpected EOF";
	case GIT_EINVALID:             return "Invalid operation or input";
	case GIT_EUNCOMMITTED:         return "Uncommitted changes in index prevented operation";
	case GIT_EDIRECTORY:           return "The operation is not valid for a directory";
	case GIT_EMERGECONFLICT:       return "A merge conflict exists and cannot continue";
	case GIT_PASSTHROUGH:          return "Internal only";
	case GIT_ITEROVER:             return "Signals end of iteration with iterator";
	}
	char tmp[100];
	sprintf(tmp, "Undefined (%d)", err);
	return tmp;
}

void LibGit2::init()
{
	git_libgit2_init();
}

void LibGit2::shutdown()
{
	git_libgit2_shutdown();

}

LibGit2::Repository LibGit2::openRepository(const std::string &dir)
{
	Repository r;
	r.open(dir.c_str());
	return r;
}

LibGit2::UserInfo LibGit2::get_global_user_info()
{
	UserInfo user;
	git_config *cfg = nullptr;
	if (git_config_open_default(&cfg) == 0) {
		git_config_entry *e;
		e = nullptr;
		if (git_config_get_entry(&e, cfg, "user.name") == 0 && e) {
			user.name = e->value;
			git_config_entry_free(e);
		}
		e = nullptr;
		if (git_config_get_entry(&e, cfg, "user.email") == 0 && e) {
			user.email = e->value;
			git_config_entry_free(e);
		}
		git_config_free(cfg);
	}
	return user;
}

// GitOID

GitOID::GitOID()
{
}

GitOID::GitOID(const git_oid &r)
	: sharedptr(new git_oid(r))
{
}

GitOID::GitOID(const std::string &s)
{
	git_oid *p = new git_oid;
	if (git_oid_fromstr(p, s.c_str()) == 0) {
		sharedptr = std::shared_ptr<git_oid>(p);
	} else {
		delete p;
	}
}

std::string GitOID::to_string() const
{
	git_oid const *id = ptr();
	if (id) {
		const int len = GIT_OID_RAWSZ * 2;
		char tmp[len + 1];
		git_oid_tostr(tmp, sizeof(tmp), id);
		return std::string(tmp, len);
	}
	return std::string();
}

// Repository

std::string LibGit2::Repository::process_diff(git_diff *diff)
{
	std::vector<char> vec;
	vec.reserve(65536);
	git_diff_print(diff, GIT_DIFF_FORMAT_PATCH, diff_line_handler, &vec);
	git_diff_free(diff);
	if (!vec.empty()) {
		char const *begin = &vec[0];
		char const *end = begin + vec.size();
		return std::string(begin, end);
	}
	return std::string();
}

int LibGit2::Repository::status_cb_(const char *path, unsigned int status_flags, void *payload)
{
	Git::FileStatusList *files = (Git::FileStatusList *)payload;
	if (status_flags & GIT_STATUS_IGNORED) {
		// nop
	} else {
		char tmp[100];
		sprintf(tmp, "%08x", status_flags);
		qDebug() << tmp << path;

		Git::FileStatus stat;
		stat.data.path1 = path;
		if (status_flags & GIT_STATUS_WT_NEW) {
			stat.data.code_x = '?';
			stat.data.code_y = '?';
			stat.data.code = Git::FileStatusCode::Untracked;
		} else if (status_flags & GIT_STATUS_WT_MODIFIED) {
			stat.data.code_x = ' ';
			stat.data.code_y = 'M';
			stat.data.code = Git::FileStatusCode::NotUpdated;
		} else if (status_flags & GIT_STATUS_WT_DELETED) {
			stat.data.code_x = ' ';
			stat.data.code_y = 'D';
			stat.data.code = Git::FileStatusCode::NotUpdated;
		}
		if (stat.data.code != Git::FileStatusCode::Unknown) {
			files->push_back(stat);
		}
	}
	return 0;
}

bool LibGit2::Repository::open(const std::string &dir)
{
	std::shared_ptr<git_repository *> t;
	git_repository *p;
	if (git_repository_open(&p, dir.c_str()) == 0) {
		t = std::shared_ptr<git_repository *>(new git_repository *(p), custom_deleter);
	}
	sharedptr = t;
	return sharedptr.get();
}

LibGit2::Commit LibGit2::Repository::revparse_head()
{
	git_commit *p = nullptr;
	if (git_repository *r = ptr()) {
		git_object *o = nullptr;
		if (git_revparse_single(&o, r, "HEAD") == 0) {
			if (git_object_peel((git_object **)&p, o, GIT_OBJ_COMMIT) != 0) {
				p = nullptr;
			}
			git_object_free(o);
		}
	}
	return p ? Commit(p) : Commit();
}

LibGit2::Commit LibGit2::Repository::lookup_commit(git_oid const *oid) const
{
	if (git_repository *repo = const_cast<Repository *>(this)->ptr()) {
		git_commit *comm;
		if (git_commit_lookup(&comm, repo, oid) == 0) {
			return Commit(comm);
		}
	}
	return Commit();
}

LibGit2::Commit LibGit2::Repository::lookup_commit(const GitOID &oid) const
{
	return lookup_commit(oid.ptr());
}



void LibGit2::Repository::test()
{

}



QString toQString(std::string const &s)
{
	return QString::fromStdString(s);
}

static void makeGitCommitItem(LibGit2::Commit const &commit, Git::CommitItem *out)
{
	out->commit_id = QString::fromStdString(commit.get_commit_id().to_string());
	out->author = toQString(commit.get_committer_name());
	out->message = toQString(commit.get_message());
	out->commit_date = QDateTime::fromTime_t(commit.get_time());
	size_t n = commit.get_parent_count();
	for (size_t i = 0; i < n; i++) {
		LibGit2::Commit parent = commit.get_parent(i);
		QString id = toQString(parent.get_commit_id().to_string());
		out->parent_ids.push_back(id);
	}
}

git_commit *LibGit2::Repository::revparse_single_to_commit(std::string const &spec) const
{
	git_commit *commit = nullptr;
	{
		git_object *obj = nullptr;
		if (git_revparse_single(&obj, ptr(), spec.c_str()) == 0) {
			git_commit *t = nullptr;
			if (git_object_peel((git_object **)&t, obj, GIT_OBJ_COMMIT) == 0) {
				commit = t;
			}
			git_object_free(obj);
		}
	}
	return commit;
}

git_tree *LibGit2::Repository::revparse_single_to_tree(std::string const &spec) const
{
	git_tree *tree = nullptr;
	{
		git_object *obj = nullptr;
		if (git_revparse_single(&obj, ptr(), spec.c_str()) == 0) {
			git_tree *t = nullptr;
			if (git_object_peel((git_object **)&t, obj, GIT_OBJ_TREE) == 0) {
				tree = t;
			}
			git_object_free(obj);
		}
	}
	return tree;
}

int LibGit2::Repository::diff_line_handler(const git_diff_delta * /*delta*/, const git_diff_hunk * /*hunk*/, const git_diff_line *line, void *payload)
{
	std::vector<char> *vec = (std::vector<char> *)payload;
	char const *begin = line->content;
	char const *end = begin + line->content_len;
	switch (line->origin) {
	case GIT_DIFF_LINE_CONTEXT:  vec->push_back(' '); break;
	case GIT_DIFF_LINE_ADDITION: vec->push_back('+'); break;
	case GIT_DIFF_LINE_DELETION: vec->push_back('-'); break;
	}
	vec->insert(vec->end(), begin, end);
	return 0;
}



std::string LibGit2::Repository::diff_head_to_workdir()
{
	std::string text;
	std::string from = "HEAD";
	git_tree *tree = revparse_single_to_tree(from);
	if (tree) {
		git_diff *diff;
		git_diff_options opt;
		git_diff_init_options(&opt, GIT_DIFF_OPTIONS_VERSION);
		opt.id_abbrev = GIT_OID_RAWSZ * 2;
		if (git_diff_tree_to_workdir(&diff, this->ptr(), tree, &opt) == 0) {
			text = process_diff(diff);
		}
		git_tree_free(tree);
	}
	return text;
}

std::string LibGit2::Repository::diff2(const std::string &older, const std::string &newer)
{
	std::string text;
	git_tree *old_tree = revparse_single_to_tree(older);
	git_tree *new_tree = revparse_single_to_tree(newer);
	if (old_tree && new_tree) {
		git_diff *diff;
		git_diff_options opt;
		git_diff_init_options(&opt, GIT_DIFF_OPTIONS_VERSION);
		opt.id_abbrev = GIT_OID_RAWSZ * 2;
		if (git_diff_tree_to_tree(&diff, this->ptr(), old_tree, new_tree, &opt) == 0) {
			text = process_diff(diff);
		}
	}
	if (old_tree) git_tree_free(old_tree);
	if (new_tree) git_tree_free(new_tree);
	return text;
}

Git::CommitItemList LibGit2::Repository::log(size_t maxcount)
{
	Git::CommitItemList list;

	git_revwalk *walk;
	git_revwalk_new(&walk, this->ptr());
	git_revwalk_sorting(walk, GIT_SORT_TIME);
	git_revwalk_push_glob(walk, "refs/*");

	git_oid oid;
	while (list.size() < maxcount && git_revwalk_next(&oid, walk) == 0) {
		Commit commit = lookup_commit(&oid);
		if (!commit.ptr()) break;
		Git::CommitItem item;
		makeGitCommitItem(commit, &item);
		list.push_back(item);
	}

	git_revwalk_free(walk);

	return list;
}

QByteArray LibGit2::Repository::cat_file(const std::string &id)
{
	QByteArray ba;
	git_blob *blob = nullptr;
	git_object *obj;
	if (git_revparse_single(&obj, this->ptr(), id.c_str()) == 0) {
		git_object *p = nullptr;
		if (git_object_peel(&p, obj, GIT_OBJ_ANY) == 0 && p) {
			blob = (git_blob *)p;
		}
		git_object_free(obj);
	}
	if (blob) {
		git_off_t n = git_blob_rawsize(blob);
		char const *p = (char const *)git_blob_rawcontent(blob);
		if (p && n > 0) {
			ba = QByteArray(p, n);
		}
		git_blob_free(blob);
	}
	return ba;
}

QList<Git::Branch> LibGit2::Repository::branches()
{
	QList<Git::Branch> branches;
	std::string refs_heads = "refs/heads/";
	std::string refs_remotes = "refs/remotes/";
	git_branch_iterator *it;
	if (git_branch_iterator_new(&it, this->ptr(), GIT_BRANCH_ALL) == 0) {
		git_reference *ref;
		git_branch_t t;
		while (git_branch_next(&ref, &t, it) == 0) {
			std::string name = git_reference_name(ref);
			GitOID oid;
			git_reference_name_to_id(oid.ptr(), this->ptr(), name.c_str());
			if (misc::starts_with(name, refs_heads)) {
				name = misc::mid(name, refs_heads.size());
			} else if (misc::starts_with(name, refs_remotes)) {
				name = misc::mid(name, refs_remotes.size());
			}
			Git::Branch b;
			b.name = QString::fromStdString(name);
			b.id = QString::fromStdString(oid.to_string());
			branches.push_back(b);
		}
		git_branch_iterator_free(it);
	}
	return branches;
}

Git::FileStatusList LibGit2::Repository::status()
{
	Git::FileStatusList files;
	git_status_foreach(this->ptr(), status_cb_, &files);
	return files;
}

bool LibGit2::Repository::commit(const std::string &message)
{
	bool ok = false;

	git_repository *repo = this->ptr();

	git_commit *parent_commit = revparse_single_to_commit("HEAD");

	if (parent_commit) {
		git_signature *sign;
		if (git_signature_default(&sign, repo) == 0 && sign) {
			git_index *repo_id;
			if (git_repository_index(&repo_id, repo) == 0 && repo_id) {
				git_index_read(repo_id, 1);
				git_oid oid_idx_tree;
				if (git_index_write_tree(&oid_idx_tree, repo_id) == 0) {
					git_tree *tree = nullptr;
					if (git_tree_lookup(&tree, repo, &oid_idx_tree) == 0 && tree) {
						git_oid new_commit_id;
						if (git_commit_create(&new_commit_id, repo, "HEAD", sign, sign, nullptr, message.c_str(), tree, 1, (const git_commit **)&parent_commit) == 0) {
							ok = true;
						}
						git_tree_free(tree);
					}
				}
				git_index_free(repo_id);
			}
			git_signature_free(sign);
		}
		git_commit_free(parent_commit);
	}
	return ok;
}

int my_git_cred_acquire_cb(git_cred **cred, const char *url, const char *username_from_url, unsigned int allowed_types, void *payload)
{
	git_cred_userpass_plaintext_new(cred, "username", "password");
	return 0;
}

bool LibGit2::Repository::fetch()
{
	git_remote *remote = nullptr;
	if (git_remote_lookup(&remote, ptr(), "origin") == 0) {
		git_remote_callbacks callbacks;
		git_proxy_options proxyopts;
		git_strarray strarr;
		git_remote_init_callbacks(&callbacks, GIT_REMOTE_CALLBACKS_VERSION);
		callbacks.credentials = my_git_cred_acquire_cb;
		git_proxy_init_options(&proxyopts, GIT_PROXY_OPTIONS_VERSION);
		strarr.strings = nullptr;
		strarr.count = 0;
		int r = git_remote_connect(remote, GIT_DIRECTION_FETCH, &callbacks, &proxyopts, &strarr);
		if (r == 0) {
			qDebug();
		} else {
			qDebug() << QString::fromStdString(LibGit2::get_error_message(r));
		}
	}
	return false;
}

bool LibGit2::Repository::push() // not work. わからん。誰か直して。
{
	git_remote *remote = nullptr;
	if (git_remote_lookup(&remote, ptr(), "origin") == 0) {
		git_remote_callbacks callbacks;
		git_proxy_options proxyopts;
		git_strarray strarr;
		git_remote_init_callbacks(&callbacks, GIT_REMOTE_CALLBACKS_VERSION);
		callbacks.credentials = my_git_cred_acquire_cb;
		git_proxy_init_options(&proxyopts, GIT_PROXY_OPTIONS_VERSION);
		strarr.strings = nullptr;
		strarr.count = 0;
		int r;
		r = git_remote_connect(remote, GIT_DIRECTION_PUSH, &callbacks, &proxyopts, &strarr);
		if (r == 0) {
			{//if (git_remote_add_push(ptr(), "origin", "refs/heads/master:refs/remotes/origin/master") == 0) {
				git_push_options pushopts;
				git_push_init_options(&pushopts, GIT_PUSH_OPTIONS_VERSION);
				pushopts.callbacks = callbacks;
				pushopts.proxy_opts = proxyopts;
				r = git_remote_upload(remote, nullptr, &pushopts);
				qDebug() << QString::fromStdString(LibGit2::get_error_message(r));
				if (r == 0) {
					return true;
				}
			}
			git_remote_disconnect(remote);
		} else {
			qDebug() << QString::fromStdString(LibGit2::get_error_message(r));
		}
	}
	return false;
}

void LibGit2::Repository::revert_a_file(std::string const &path)
{
}


// Commit

GitOID LibGit2::Commit::get_commit_id() const
{
	if (git_commit const *comm = ptr()) {
		git_oid const *oid = git_commit_id(comm);
		if (oid) {
			return GitOID(*oid);
		}
	}
	return GitOID();
}

size_t LibGit2::Commit::get_parent_count() const
{
	if (git_commit const *comm = ptr()) {
		return git_commit_parentcount(comm);
	}
	return 0;
}

LibGit2::Commit LibGit2::Commit::get_parent(size_t i) const
{
	if (git_commit const *comm = ptr()) {
		git_commit *parent = nullptr;
		size_t n = get_parent_count();
		if (i < n && git_commit_parent(&parent, comm, i) == 0) {
			return Commit(parent);
		}
	}
	return Commit();
}

std::string LibGit2::Commit::get_committer_name() const
{
	if (git_commit const *comm = ptr()) {
		git_signature const *s = git_commit_committer(comm);
		if (s) return s->name;
	}
	return std::string();
}

std::string LibGit2::Commit::get_message() const
{
	char const *s = nullptr;
	if (git_commit const *comm = ptr()) {
		s = git_commit_message(comm);
	}
	return s ? std::string(s) : std::string();
}

time_t LibGit2::Commit::get_time() const
{
	if (git_commit const *comm = ptr()) {
		return git_commit_time(comm);
	}
	return 0;
}


#endif // USE_LIBGIT2

