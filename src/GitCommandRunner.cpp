#include "GitCommandRunner.h"
#include <string>

std::string GitCommandRunner::pty_message() const
{
	return pty_->getMessage();
}

void GitCommandRunner::operator ()(Git_clone const &item)
{
	result = git()->clone(item.clonedata_, pty());
}

void GitCommandRunner::operator ()(Git_fetch const &item)
{
	result = git()->fetch(pty(), item.prune);
}

void GitCommandRunner::operator ()(Git_fetch_tags_f const &item)
{
	result = git()->fetch_tags_f(pty());
}

void GitCommandRunner::operator ()(Git_stage const &item)
{
	result = git()->stage(item.paths, pty());
}

void GitCommandRunner::operator ()(Git_push const &item)
{
	result = git()->push_u(item.set_upstream_, item.remote_, item.branch_, item.force_, pty());
}

void GitCommandRunner::operator ()(Git_pull const &item)
{
	result = git()->pull(pty());
}

void GitCommandRunner::operator ()(Git_push_tags const &item)
{
	result = git()->push_tags(pty());
}

void GitCommandRunner::operator ()(Git_delete_tag const &item)
{
	result = git()->delete_tag(item.name_, item.remote_);
}

void GitCommandRunner::operator ()(Git_delete_tags const &item)
{
	result = false;
	for (QString const &name : item.tagnames) {
		if (git()->delete_tag(name, true)) {
			result = true;
		}
	}
}

void GitCommandRunner::operator ()(Git_add_tag const &item)
{
	result = git()->tag(item.name_, item.commit_id_);
}

void GitCommandRunner::operator ()(Git_submodule_add const &item)
{
	result = git()->submodule_add(item.data_, item.force_, pty());
}

