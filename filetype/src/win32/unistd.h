
// minimul unistd.h for windows

#ifndef WIN32_UNISTD_H
#define WIN32_UNISTD_H    1

#pragma comment(lib, "shlwapi") // for PathRemoveFileSpec function

#define R_OK    4
#define W_OK    2
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
typedef signed __int64 ssize_t;
typedef unsigned int mode_t;

extern char *optarg;
extern int optind;

/* Block device */
#if !defined(S_IFBLK)
#   define S_IFBLK 0
#endif

/* Pipe */
#if !defined(S_IFIFO)
#   define S_IFIFO _S_IFIFO
#endif

#include <io.h>

static inline int pipe(int fd) { return -1; }

#endif
