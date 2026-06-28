
#ifdef EXPERIMENTAL_FILETYPEPLUGIN

#include "MyExperimentalLibrary.h"

int api_test(int a, int b)
{
	return a + b;
}

void api_release()
{
}

namespace filetypeplugin {

struct Hoge::Private {
	int value = 0;
};

Hoge *Hoge_new()
{
	Hoge *p = new Hoge();
	p->m = new Hoge::Private;
	p->m->value = 42;
	return p;
}

void Hoge_delete(Hoge *p)
{
	delete p->m;
	delete p;
}

int Hoge_value(Hoge *self)
{
	return self->m->value;
}

static FILETYPE_API_TABLE api_table = {
	.release = api_release,
	.test = api_test,
	.Hoge_new = Hoge_new,
	.Hoge_delete = Hoge_delete,
	.Hoge_value = Hoge_value,	
};
} // namespace filetypeplugin
using namespace filetypeplugin;

extern "C" FILETYPE_API_TABLE *init_5b803ec8_416b_ee53_7fd2_fc9aec7decf8(size_t size)
{
	if (size == sizeof(FILETYPE_API_TABLE)) {
		return &api_table;
	}
	return nullptr;
}


#endif
