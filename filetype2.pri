
INCLUDEPATH += filetype2/misc
INCLUDEPATH += filetype2/file/src
INCLUDEPATH += filetype2/pcre2/src
win32:INCLUDEPATH += filetype2/win32
win32:INCLUDEPATH += filetype2/dirent/include

DEFINES += "HAVE_CONFIG_H=1" "_SSIZE_T_DEFINED=1" "PCRE2_CODE_UNIT_WIDTH=8"
!win32:DEFINES += "HAVE_MKSTEMP=1"

win32:HEADERS += filetype2/dirent/include/dirent.h

SOURCES += \
	filetype2/file/src/apprentice.c \
	filetype2/file/src/apptype.c \
	filetype2/file/src/ascmagic.c \
	filetype2/file/src/asctime_r.c \
	filetype2/file/src/asprintf.c \
	filetype2/file/src/buffer.c \
	filetype2/file/src/cdf.c \
	filetype2/file/src/cdf_time.c \
	filetype2/file/src/compress.c \
	filetype2/file/src/ctime_r.c \
	filetype2/file/src/der.c \
	filetype2/file/src/dprintf.c \
	filetype2/file/src/encoding.c \
	filetype2/file/src/fmtcheck.c \
	filetype2/file/src/fsmagic.c \
	filetype2/file/src/funcs.c \
	filetype2/file/src/getline.c \
	filetype2/file/src/gmtime_r.c \
	filetype2/file/src/is_csv.c \
	filetype2/file/src/is_json.c \
	filetype2/file/src/is_simh.c \
	filetype2/file/src/is_tar.c \
	filetype2/file/src/localtime_r.c \
	filetype2/file/src/magic.c \
	filetype2/file/src/pread.c \
	filetype2/file/src/print.c \
	filetype2/file/src/readcdf.c \
	filetype2/file/src/readelf.c \
	filetype2/file/src/seccomp.c \
	filetype2/file/src/softmagic.c \
	filetype2/file/src/strcasestr.c \
	filetype2/file/src/strlcat.c \
	filetype2/file/src/strlcpy.c \
	filetype2/file/src/vasprintf.c \
	filetype2/pcre2/src/pcre2_auto_possess.c \
	filetype2/pcre2/src/pcre2_chartables.c \
	filetype2/pcre2/src/pcre2_compile.c \
	filetype2/pcre2/src/pcre2_config.c \
	filetype2/pcre2/src/pcre2_context.c \
	filetype2/pcre2/src/pcre2_convert.c \
	filetype2/pcre2/src/pcre2_dfa_match.c \
	filetype2/pcre2/src/pcre2_error.c \
	filetype2/pcre2/src/pcre2_find_bracket.c \
	filetype2/pcre2/src/pcre2_fuzzsupport.c \
	filetype2/pcre2/src/pcre2_jit_compile.c \
	filetype2/pcre2/src/pcre2_jit_match.c \
	filetype2/pcre2/src/pcre2_jit_misc.c \
	filetype2/pcre2/src/pcre2_maketables.c \
	filetype2/pcre2/src/pcre2_match.c \
	filetype2/pcre2/src/pcre2_match_data.c \
	filetype2/pcre2/src/pcre2_newline.c \
	filetype2/pcre2/src/pcre2_ord2utf.c \
	filetype2/pcre2/src/pcre2_pattern_info.c \
	filetype2/pcre2/src/pcre2_printint.c \
	filetype2/pcre2/src/pcre2_serialize.c \
	filetype2/pcre2/src/pcre2_string_utils.c \
	filetype2/pcre2/src/pcre2_study.c \
	filetype2/pcre2/src/pcre2_substitute.c \
	filetype2/pcre2/src/pcre2_substring.c \
	filetype2/pcre2/src/pcre2_tables.c \
	filetype2/pcre2/src/pcre2_ucd.c \
	filetype2/pcre2/src/pcre2_valid_utf.c \
	filetype2/pcre2/src/pcre2_xclass.c \
	filetype2/pcre2/src/pcre2posix.c

HEADERS += \
	filetype2/file/src/cdf.h \
	filetype2/file/src/der.h \
	filetype2/file/src/elfclass.h \
	filetype2/file/src/file.h \
	filetype2/file/src/file_opts.h \
	filetype2/file/src/magic.h \
	filetype2/file/src/magic.h \
	filetype2/file/src/patchlevel.h \
	filetype2/file/src/readelf.h \
	filetype2/file/src/tar.h \
	filetype2/misc/config.h \
	filetype2/misc/file_config.h \
	filetype2/misc/pcre2.h \
	filetype2/misc/pcre2_config.h \
	filetype2/misc/unistd.h \
	filetype2/pcre2/src/pcre2_internal.h \
	filetype2/pcre2/src/pcre2_intmodedep.h \
	filetype2/pcre2/src/pcre2_ucp.h \
	filetype2/pcre2/src/pcre2posix.h \
	filetype2/pcre2/src/regex.h


