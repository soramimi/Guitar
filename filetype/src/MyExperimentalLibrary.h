#ifndef MYEXPERIMENTALLIBRARY_H
#define MYEXPERIMENTALLIBRARY_H
#ifdef EXPERIMENTAL_FILETYPEPLUGIN

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
#ifdef Q_OS_WIN
	typedef FARPROC fnptr_t;
	struct {
		HMODULE hLib = nullptr;
	} d;
#else
	struct {
		void *hLib = nullptr;
		FILETYPE_API_TABLE *filetype_api = nullptr;
	} d;
#endif
	FILETYPE_API_TABLE *(*filetypeplugin_init)(size_t n) = nullptr;
	struct {
		FILETYPE_API_TABLE *(*init)() = nullptr;
		void (*dtor)(Hoge *self) = nullptr;
		Hoge (*hoge)() = nullptr;
		int (*value)(Hoge *self) = nullptr;
	} fn;
	bool _open()
	{
		bool ok = true;
		std::string libname = "filetypeplugin";
#ifdef Q_OS_WIN
		{
			d.hLib = LoadLibraryA((libname + ".dll").c_str());
			if (!d.hLib) {
				logprintf(LOG_DEFAULT, "Failed to load filetype.dll: %d", GetLastError());
				return false;
			}
			for (auto &e : table) {
				e.ptr = GetProcAddress(d.hLib, e.name);
				if (!e.ptr) {
					logprintf(LOG_DEFAULT, "Failed to find symbol %s in filetype.dll: %d", e.name, GetLastError());
					return false;
				}
			}
		}
#else
		{
			std::string soname = "lib" + libname + ".so";
			d.hLib = dlopen(soname.c_str(), RTLD_NOW | RTLD_GLOBAL);
			if (!d.hLib) {
				fprintf(stderr, "Failed to load %s: %s", soname.c_str(), dlerror());
				return false;
			}
			(void *&)filetypeplugin_init = dlsym(d.hLib, "init_5b803ec8_416b_ee53_7fd2_fc9aec7decf8");
			if (filetypeplugin_init) {
				d.filetype_api = filetypeplugin_init(sizeof(FILETYPE_API_TABLE));
				if (d.filetype_api) {
					int n = d.filetype_api->test(12, 34);
					fprintf(stderr, "filetype.dll: test(12, 34) = %d\n", n);
					Hoge *hoge = d.filetype_api->Hoge_new();
					int value = d.filetype_api->Hoge_value(hoge);
					fprintf(stderr, "filetype.dll: Hoge_value() = %d\n", value);
					d.filetype_api->Hoge_delete(hoge);
					d.filetype_api->release();
					ok = true;
				}
			}
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
		fn = {};
	}
};

#endif
#endif // MYEXPERIMENTALLIBRARY_H
