
// minimul unistd.h

#ifndef _UNISTD_H
#define _UNISTD_H    1

#ifdef _WIN32

#pragma comment(lib, "shlwapi") // for PathRemoveFileSpec function

#define R_OK    4
#define W_OK    2
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
//typedef signed __int64 ssize_t;
typedef unsigned int mode_t;

/* Block device */
#if !defined(S_IFBLK)
#   define S_IFBLK 0
#endif

/* Pipe */
#if !defined(S_IFIFO)
#   define S_IFIFO _S_IFIFO
#endif

static inline int pipe(int fd) { return -1; }

#else // !_WIN32
// #include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STDIN_FILENO    0       /* Standard input.  */
#define STDOUT_FILENO   1       /* Standard output.  */
#define STDERR_FILENO   2       /* Standard error output.  */
extern ssize_t read (int __fd, void *__buf, size_t __nbytes);
extern ssize_t write (int __fd, const void *__buf, size_t __n);
extern int close (int __fd);

#ifdef __cplusplus
}
#endif

#endif /* unistd.h  */

#endif
