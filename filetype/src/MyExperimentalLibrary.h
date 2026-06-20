#ifndef MYEXPERIMENTALLIBRARY_H
#define MYEXPERIMENTALLIBRARY_H
#ifdef EXPERIMENTAL_FILETYPEPLUGIN

#include <string>

#ifdef _WIN32
#include <windows.h>
#ifndef DLLEXPORT
#define DLLEXPORT __declspec(dllimport)
#endif
#else
#include <dlfcn.h>
#include <string>
#define DLLEXPORT
#endif

namespace filetypeplugin {

struct Hoge {
	struct Private;
	Private *m;
};

struct FILETYPE_API_TABLE {
	void (*release)() = nullptr;
	int (*test)(int a, int b) = nullptr;
	filetypeplugin::Hoge *(*Hoge_new)() = nullptr;
	void (*Hoge_delete)(filetypeplugin::Hoge *p) = nullptr;
	int (*Hoge_value)(filetypeplugin::Hoge *self) = nullptr;
};
}

extern "C" DLLEXPORT filetypeplugin::FILETYPE_API_TABLE *init_5b803ec8_416b_ee53_7fd2_fc9aec7decf8(size_t size);

class MyExperimentalLibrary {
private:
	using FILETYPE_API_TABLE = filetypeplugin::FILETYPE_API_TABLE;
	using Hoge = filetypeplugin::Hoge;
	struct {
#ifdef _WIN32
		HMODULE hLib = nullptr;
#else
		void *hLib = nullptr;
#endif
		FILETYPE_API_TABLE *filetype_api = nullptr;
		FILETYPE_API_TABLE *(*filetypeplugin_init)(size_t n) = nullptr;
	} d;
	bool _init()
	{
		if (d.filetypeplugin_init) {
			d.filetype_api = d.filetypeplugin_init(sizeof(FILETYPE_API_TABLE));
			if (d.filetype_api) {
				int n = d.filetype_api->test(12, 34);
				fprintf(stderr, "filetype.dll: test(12, 34) = %d\n", n);
				Hoge *hoge = d.filetype_api->Hoge_new();
				int value = d.filetype_api->Hoge_value(hoge);
				fprintf(stderr, "filetype.dll: Hoge_value() = %d\n", value);
				d.filetype_api->Hoge_delete(hoge);
				d.filetype_api->release();
				return true;
			}
		}
		return false;
	}
	bool _open()
	{
		bool ok = true;
		std::string libname = "filetypeplugin";
#ifdef _WIN32
		{
			d.hLib = LoadLibraryA((libname + ".dll").c_str());
			if (!d.hLib) {
				fprintf(stderr, "Failed to load filetype.dll: %d", GetLastError());
				return false;
			}
			(void *&)d.filetypeplugin_init = GetProcAddress(d.hLib, "init_5b803ec8_416b_ee53_7fd2_fc9aec7decf8");
			ok = _init();
		}
#else
		{
			std::string soname = "lib" + libname + ".so";
			d.hLib = dlopen(soname.c_str(), RTLD_NOW | RTLD_GLOBAL);
			if (!d.hLib) {
				fprintf(stderr, "Failed to load %s: %s", soname.c_str(), dlerror());
				return false;
			}
			(void *&)d.filetypeplugin_init = dlsym(d.hLib, "init_5b803ec8_416b_ee53_7fd2_fc9aec7decf8");
			ok = _init();
		}
#endif
		return ok;
	}
public:
	MyExperimentalLibrary() = default;
	~MyExperimentalLibrary()
	{
	}
	bool open()
	{
		if (_open()) {
			return true;
		} else {
			close();
			return false;
		}
	}
	void close()
	{
		d = {};
	}
};

#endif
#endif // MYEXPERIMENTALLIBRARY_H
