
#include "GitDiff.h"
#include <fmt.h>

void GitDiff::makeForSingleFile(GitDiff *diff, const std::string &id_a, const std::string &id_b, const std::string &path, const std::string &mode)
{
	diff->diff = fmt("diff --git a/%s b/%s").arg(path)(path);
	diff->index = fmt("index %s..%s %d")(id_a)(id_b)(0);
	diff->blob.a_id_or_path = id_a;
	diff->blob.b_id_or_path = id_b;
	diff->path = path;
	diff->mode = mode;
	diff->type = GitDiff::Type::Create;
}
