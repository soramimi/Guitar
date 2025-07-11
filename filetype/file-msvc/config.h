#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <locale.h>
typedef _locale_t locale_t;

typedef signed __int64 ssize_t;
typedef unsigned int mode_t;
#include "unistd.h"

#include <sys/stat.h>
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISFIFO(m) (0)

/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* Define in built-in ELF support is used */
#define BUILTIN_ELF 1

/* Enable bzlib compression support */
// #define BZLIBSUPPORT 1

/* Define for ELF core file support */
#define ELFCORE 1

/* Define to 1 if you have the 'asctime_r' function. */
#define HAVE_ASCTIME_R 1

/* Define to 1 if you have the 'asprintf' function. */
#define HAVE_ASPRINTF 1

/* Define to 1 if you have the <byteswap.h> header file. */
// #define HAVE_BYTESWAP_H 1

/* Define to 1 if you have the <bzlib.h> header file. */
// #define HAVE_BZLIB_H 1

/* Define to 1 if you have the 'ctime_r' function. */
// #define HAVE_CTIME_R 1

/* HAVE_DAYLIGHT */
#define HAVE_DAYLIGHT 1

/* Define to 1 if you have the declaration of 'daylight', and to 0 if you
   don't. */
#define HAVE_DECL_DAYLIGHT 1

/* Define to 1 if you have the declaration of 'tzname', and to 0 if you don't.
   */
#define HAVE_DECL_TZNAME 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the 'dprintf' function. */
#define HAVE_DPRINTF 1

/* Define to 1 if you have the <err.h> header file. */
#define HAVE_ERR_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the 'fmtcheck' function. */
/* #undef HAVE_FMTCHECK */

/* Define to 1 if you have the 'fork' function. */
// #define HAVE_FORK 1

/* Define to 1 if you have the 'freelocale' function. */
#define HAVE_FREELOCALE 1

/* Define to 1 if fseeko (and ftello) are declared in stdio.h. */
#define HAVE_FSEEKO 1

/* Define to 1 if you have the 'getline' function. */
// #define HAVE_GETLINE 1

/* Define to 1 if you have the <getopt.h> header file. */
#define HAVE_GETOPT_H 1

/* Define to 1 if you have the 'getopt_long' function. */
// #define HAVE_GETOPT_LONG 1

/* Define to 1 if you have the 'getpagesize' function. */
#define HAVE_GETPAGESIZE 1

/* Define to 1 if you have the 'gmtime_r' function. */
#define HAVE_GMTIME_R 1

/* Define to 1 if the system has the type 'intptr_t'. */
#define HAVE_INTPTR_T 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the 'bz2' library (-lbz2). */
#define HAVE_LIBBZ2 1

/* Define to 1 if you have the 'gnurx' library (-lgnurx). */
/* #undef HAVE_LIBGNURX */

/* Define to 1 if you have the 'lrzip' library (-llrzip). */
/* #undef HAVE_LIBLRZIP */

/* Define to 1 if you have the 'lz' library (-llz). */
#define HAVE_LIBLZ 1

/* Define to 1 if you have the 'lzma' library (-llzma). */
#define HAVE_LIBLZMA 1

/* Define to 1 if you have the 'seccomp' library (-lseccomp). */
// #define HAVE_LIBSECCOMP 1

/* Define to 1 if you have the 'z' library (-lz). */
#define HAVE_LIBZ 1

/* Define to 1 if you have the 'zstd' library (-lzstd). */
#define HAVE_LIBZSTD 1

/* Define to 1 if you have the 'localtime_r' function. */
#define HAVE_LOCALTIME_R 1

/* Define to 1 if you have the <Lrzip.h> header file. */
/* #undef HAVE_LRZIP_H */

/* Define to 1 if you have the <lzlib.h> header file. */
// #define HAVE_LZLIB_H 1

/* Define to 1 if you have the <lzma.h> header file. */
// #define HAVE_LZMA_H 1

/* Define to 1 if mbrtowc and mbstate_t are properly declared. */
#define HAVE_MBRTOWC 1

/* Define to 1 if <wchar.h> declares mbstate_t. */
#define HAVE_MBSTATE_T 1

/* Define to 1 if you have the 'memmem' function. */
// #define HAVE_MEMMEM 1

/* Define to 1 if you have the <minix/config.h> header file. */
/* #undef HAVE_MINIX_CONFIG_H */

/* Define to 1 if you have the 'mkostemp' function. */
#define HAVE_MKOSTEMP 1

/* Define to 1 if you have the 'mkstemp' function. */
// #define HAVE_MKSTEMP 1

/* Define to 1 if you have a working 'mmap' system call. */
// #define HAVE_MMAP 1

/* Define to 1 if you have the 'newlocale' function. */
// #define HAVE_NEWLOCALE 1

/* Define to 1 if you have the 'pipe2' function. */
// #define HAVE_PIPE2 1

/* Define to 1 if you have the 'posix_spawnp' function. */
#define HAVE_POSIX_SPAWNP 1

/* Define to 1 if you have the 'pread' function. */
// #define HAVE_PREAD 1

/* Have sig_t type */
#define HAVE_SIG_T 1

/* Define to 1 if you have the <spawn.h> header file. */
// #define HAVE_SPAWN_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the 'strcasestr' function. */
#define HAVE_STRCASESTR 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the 'strlcat' function. */
// #define HAVE_STRLCAT 1

/* Define to 1 if you have the 'strlcpy' function. */
// #define HAVE_STRLCPY 1

/* Define to 1 if you have the 'strndup' function. */
// #define HAVE_STRNDUP 1

/* Define to 1 if you have the 'strtof' function. */
#define HAVE_STRTOF 1

/* HAVE_STRUCT_OPTION */
#define HAVE_STRUCT_OPTION 1

/* Define to 1 if 'st_rdev' is a member of 'struct stat'. */
// #define HAVE_STRUCT_STAT_ST_RDEV 1

/* Define to 1 if 'tm_gmtoff' is a member of 'struct tm'. */
// #define HAVE_STRUCT_TM_TM_GMTOFF 1

/* Define to 1 if 'tm_zone' is a member of 'struct tm'. */
// #define HAVE_STRUCT_TM_TM_ZONE 1

/* Define to 1 if you have the <sys/bswap.h> header file. */
/* #undef HAVE_SYS_BSWAP_H */

/* Define to 1 if you have the <sys/ioctl.h> header file. */
// #define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/mman.h> header file. */
#define HAVE_SYS_MMAN_H 1

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/sysmacros.h> header file. */
// #define HAVE_SYS_SYSMACROS_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
// #define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/utime.h> header file. */
#define HAVE_SYS_UTIME_H 1

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
// #define HAVE_SYS_WAIT_H 1

/* HAVE_TM_ISDST */
#define HAVE_TM_ISDST 1

/* HAVE_TM_ZONE */
#define HAVE_TM_ZONE 1

/* HAVE_TZNAME */
#define HAVE_TZNAME 1

/* Define to 1 if the system has the type 'uintptr_t'. */
#define HAVE_UINTPTR_T 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the 'uselocale' function. */
#define HAVE_USELOCALE 1

/* Define to 1 if you have the 'utime' function. */
#define HAVE_UTIME 1

/* Define to 1 if you have the 'utimes' function. */
// #define HAVE_UTIMES 1

/* Define to 1 if you have the <utime.h> header file. */
#define HAVE_UTIME_H 1

/* Define to 1 if you have the 'vasprintf' function. */
#define HAVE_VASPRINTF 1

/* Define to 1 if you have the 'vfork' function. */
#define HAVE_VFORK 1

/* Define to 1 if you have the <vfork.h> header file. */
/* #undef HAVE_VFORK_H */

/* Define to 1 or 0, depending whether the compiler supports simple visibility
   declarations. */
#define HAVE_VISIBILITY 0

/* Define to 1 if you have the <wchar.h> header file. */
#define HAVE_WCHAR_H 1

/* Define to 1 if you have the <wctype.h> header file. */
#define HAVE_WCTYPE_H 1

/* Define to 1 if you have the 'wcwidth' function. */
// #define HAVE_WCWIDTH 1

/* Define to 1 if 'fork' works. */
#define HAVE_WORKING_FORK 1

/* Define to 1 if 'vfork' works. */
#define HAVE_WORKING_VFORK 1

/* Define to 1 if you have the <xlocale.h> header file. */
/* #undef HAVE_XLOCALE_H */

/* Define to 1 if you have the <zlib.h> header file. */
#define HAVE_ZLIB_H 1

/* Define to 1 if you have the <zstd_errors.h> header file. */
#define HAVE_ZSTD_ERRORS_H 1

/* Define to 1 if you have the <zstd.h> header file. */
// #define HAVE_ZSTD_H 1

/* Enable lrziplib compression support */
/* #undef LRZIPLIBSUPPORT */

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Enable lzlib compression support */
// #define LZLIBSUPPORT 1

/* Define to 1 if 'major', 'minor', and 'makedev' are declared in <mkdev.h>.
   */
/* #undef MAJOR_IN_MKDEV */

/* Define to 1 if 'major', 'minor', and 'makedev' are declared in
   <sysmacros.h>. */
#define MAJOR_IN_SYSMACROS 1

/* Name of package */
#define PACKAGE "file"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "christos@astron.com"

/* Define to the full name of this package. */
#define PACKAGE_NAME "file"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "file 5.46"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "file"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "5.46"

/* Define to 1 if all of the C89 standard headers exist (not just the ones
   required in a freestanding environment). This macro is provided for
   backward compatibility; new code need not use it. */
#define STDC_HEADERS 1

/* Define to 1 if your <sys/time.h> declares 'struct tm'. */
/* #undef TM_IN_SYS_TIME */

/* Enable extensions on AIX, Interix, z/OS.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable general extensions on macOS.  */
#ifndef _DARWIN_C_SOURCE
# define _DARWIN_C_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable X/Open compliant socket functions that do not require linking
   with -lxnet on HP-UX 11.11.  */
#ifndef _HPUX_ALT_XOPEN_SOCKET_API
# define _HPUX_ALT_XOPEN_SOCKET_API 1
#endif
/* Identify the host operating system as Minix.
   This macro does not affect the system headers' behavior.
   A future release of Autoconf may stop defining this macro.  */
#ifndef _MINIX
/* # undef _MINIX */
#endif
/* Enable general extensions on NetBSD.
   Enable NetBSD compatibility extensions on Minix.  */
#ifndef _NETBSD_SOURCE
# define _NETBSD_SOURCE 1
#endif
/* Enable OpenBSD compatibility extensions on NetBSD.
   Oddly enough, this does nothing on OpenBSD.  */
#ifndef _OPENBSD_SOURCE
# define _OPENBSD_SOURCE 1
#endif
/* Define to 1 if needed for POSIX-compatible behavior.  */
#ifndef _POSIX_SOURCE
/* # undef _POSIX_SOURCE */
#endif
/* Define to 2 if needed for POSIX-compatible behavior.  */
#ifndef _POSIX_1_SOURCE
/* # undef _POSIX_1_SOURCE */
#endif
/* Enable POSIX-compatible threading on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-5:2014.  */
#ifndef __STDC_WANT_IEC_60559_ATTRIBS_EXT__
# define __STDC_WANT_IEC_60559_ATTRIBS_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-1:2014.  */
#ifndef __STDC_WANT_IEC_60559_BFP_EXT__
# define __STDC_WANT_IEC_60559_BFP_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-2:2015.  */
#ifndef __STDC_WANT_IEC_60559_DFP_EXT__
# define __STDC_WANT_IEC_60559_DFP_EXT__ 1
#endif
/* Enable extensions specified by C23 Annex F.  */
#ifndef __STDC_WANT_IEC_60559_EXT__
# define __STDC_WANT_IEC_60559_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-4:2015.  */
#ifndef __STDC_WANT_IEC_60559_FUNCS_EXT__
# define __STDC_WANT_IEC_60559_FUNCS_EXT__ 1
#endif
/* Enable extensions specified by C23 Annex H and ISO/IEC TS 18661-3:2015.  */
#ifndef __STDC_WANT_IEC_60559_TYPES_EXT__
# define __STDC_WANT_IEC_60559_TYPES_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TR 24731-2:2010.  */
#ifndef __STDC_WANT_LIB_EXT2__
# define __STDC_WANT_LIB_EXT2__ 1
#endif
/* Enable extensions specified by ISO/IEC 24747:2009.  */
#ifndef __STDC_WANT_MATH_SPEC_FUNCS__
# define __STDC_WANT_MATH_SPEC_FUNCS__ 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable X/Open extensions.  Define to 500 only if necessary
   to make mbstate_t available.  */
#ifndef _XOPEN_SOURCE
/* # undef _XOPEN_SOURCE */
#endif


/* Version number of package */
#define VERSION "5.46"

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Enable xzlib compression support */
// #define XZLIBSUPPORT 1

/* Enable zlib compression support */
#define ZLIBSUPPORT 1

/* Enable zstdlib compression support */
// #define ZSTDLIBSUPPORT 1

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Define to 1 if necessary to make fseeko visible. */
/* #undef _LARGEFILE_SOURCE */

/* Define to 1 on platforms where this makes off_t a 64-bit type. */
/* #undef _LARGE_FILES */

/* Number of bits in time_t, on hosts where this is settable. */
/* #undef _TIME_BITS */

/* Define for Solaris 2.5.1 so the uint32_t typedef from <sys/synch.h>,
   <pthread.h>, or <semaphore.h> is not used. If the typedef were allowed, the
   #define below would cause a syntax error. */
/* #undef _UINT32_T */

/* Define for Solaris 2.5.1 so the uint64_t typedef from <sys/synch.h>,
   <pthread.h>, or <semaphore.h> is not used. If the typedef were allowed, the
   #define below would cause a syntax error. */
/* #undef _UINT64_T */

/* Define for Solaris 2.5.1 so the uint8_t typedef from <sys/synch.h>,
   <pthread.h>, or <semaphore.h> is not used. If the typedef were allowed, the
   #define below would cause a syntax error. */
/* #undef _UINT8_T */

/* Define to 1 on platforms where this makes time_t a 64-bit type. */
/* #undef __MINGW_USE_VC2005_COMPAT */

/* Define to the type of a signed integer type of width exactly 32 bits if
   such a type exists and the standard includes do not define it. */
/* #undef int32_t */

/* Define to the type of a signed integer type of width exactly 64 bits if
   such a type exists and the standard includes do not define it. */
/* #undef int64_t */

/* Define to the type of a signed integer type wide enough to hold a pointer,
   if such a type exists, and if the system does not define it. */
/* #undef intptr_t */

/* Define to a type if <wchar.h> does not define. */
/* #undef mbstate_t */

/* Define to 'long int' if <sys/types.h> does not define. */
/* #undef off_t */

/* Define as a signed integer type capable of holding a process identifier. */
/* #undef pid_t */

/* Define as 'unsigned int' if <stddef.h> doesn't define. */
/* #undef size_t */

/* Define to the type of an unsigned integer type of width exactly 16 bits if
   such a type exists and the standard includes do not define it. */
/* #undef uint16_t */

/* Define to the type of an unsigned integer type of width exactly 32 bits if
   such a type exists and the standard includes do not define it. */
/* #undef uint32_t */

/* Define to the type of an unsigned integer type of width exactly 64 bits if
   such a type exists and the standard includes do not define it. */
/* #undef uint64_t */

/* Define to the type of an unsigned integer type of width exactly 8 bits if
   such a type exists and the standard includes do not define it. */
/* #undef uint8_t */

/* Define to the type of an unsigned integer type wide enough to hold a
   pointer, if such a type exists, and if the system does not define it. */
/* #undef uintptr_t */

/* Define as 'fork' if 'vfork' does not work. */
/* #undef vfork */


#endif // CONFIG_H
