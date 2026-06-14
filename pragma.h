
#ifdef __GNUC__
#pragma GCC diagnostic error   "-Wreturn-type"
#pragma GCC diagnostic error   "-Wtrigraphs"
#pragma GCC diagnostic error   "-Wuninitialized-const-reference"
#pragma GCC diagnostic ignored "-Wswitch"
#pragma GCC diagnostic ignored "-Wreorder"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-braces"
#endif

#ifdef _MSC_VER
#pragma warning(error: 4715) // not all control paths return a value
#pragma warning(error: 4837) // trigraphs are incompatible with C99
#pragma warning(disable: 4061) // enumerator in switch of enum is not explicitly handled by a case label
#pragma warning(disable: 4100) // unreferenced formal parameter
#pragma warning(disable: 4996) // function or variable may be unsafe
#endif

