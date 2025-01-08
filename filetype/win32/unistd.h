#ifndef WIN32_DUMMY_UNISTD_H // dummy file for windows

#define	STDIN_FILENO	0
#define	STDOUT_FILENO	1
#define	STDERR_FILENO	2


#define S_IFBLK 0060000
#define S_IFIFO 0010000


#ifdef __cplusplus
extern "C" {
#endif

int pipe(int *fds);

#ifdef __cplusplus
}
#endif





#define F_OK 0
#define R_OK 2
#define W_OK 4
//#define X_OK

#endif // WIN32_DUMMY_UNISTD_H
