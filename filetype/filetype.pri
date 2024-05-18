
INCLUDEPATH += filetype/misc
INCLUDEPATH += filetype/src
INCLUDEPATH += filetype/pcre2/src
win32:INCLUDEPATH += filetype/win32

win32:DEFINES += "HAVE_CONFIG_H=1" "PCRE2_CODE_UNIT_WIDTH=8"
!win32:DEFINES += "HAVE_CONFIG_H=1" "_SSIZE_T_DEFINED=1" "PCRE2_CODE_UNIT_WIDTH=8"

SOURCES += \
	$$PWD/file/src/is_simh.c \
	filetype/file/src/apprentice.c \
	filetype/file/src/apptype.c \
	filetype/file/src/ascmagic.c \
	filetype/file/src/asctime_r.c \
	filetype/file/src/asprintf.c \
	filetype/file/src/buffer.c \
	filetype/file/src/cdf.c \
	filetype/file/src/cdf_time.c \
	filetype/file/src/compress.c \
	filetype/file/src/ctime_r.c \
	filetype/file/src/der.c \
	filetype/file/src/dprintf.c \
	filetype/file/src/encoding.c \
	filetype/file/src/fmtcheck.c \
	filetype/file/src/fsmagic.c \
	filetype/file/src/funcs.c \
	filetype/file/src/getline.c \
	filetype/file/src/gmtime_r.c \
	filetype/file/src/is_csv.c \
	filetype/file/src/is_json.c \
	filetype/file/src/is_tar.c \
	filetype/file/src/localtime_r.c \
	filetype/file/src/magic.c \
	filetype/file/src/pread.c \
	filetype/file/src/print.c \
	filetype/file/src/readcdf.c \
	filetype/file/src/readelf.c \
	filetype/file/src/seccomp.c \
	filetype/file/src/softmagic.c \
	filetype/file/src/strcasestr.c \
	filetype/file/src/strlcat.c \
	filetype/file/src/strlcpy.c \
	filetype/file/src/vasprintf.c \
	filetype/pcre2/src/pcre2_auto_possess.c \
	filetype/pcre2/src/pcre2_chartables.c \
	filetype/pcre2/src/pcre2_compile.c \
	filetype/pcre2/src/pcre2_config.c \
	filetype/pcre2/src/pcre2_context.c \
	filetype/pcre2/src/pcre2_convert.c \
	filetype/pcre2/src/pcre2_dfa_match.c \
	filetype/pcre2/src/pcre2_error.c \
	filetype/pcre2/src/pcre2_find_bracket.c \
	filetype/pcre2/src/pcre2_jit_compile.c \
	filetype/pcre2/src/pcre2_jit_match.c \
	filetype/pcre2/src/pcre2_jit_misc.c \
	filetype/pcre2/src/pcre2_maketables.c \
	filetype/pcre2/src/pcre2_match.c \
	filetype/pcre2/src/pcre2_match_data.c \
	filetype/pcre2/src/pcre2_newline.c \
	filetype/pcre2/src/pcre2_ord2utf.c \
	filetype/pcre2/src/pcre2_pattern_info.c \
	filetype/pcre2/src/pcre2_printint.c \
	filetype/pcre2/src/pcre2_serialize.c \
	filetype/pcre2/src/pcre2_string_utils.c \
	filetype/pcre2/src/pcre2_study.c \
	filetype/pcre2/src/pcre2_substitute.c \
	filetype/pcre2/src/pcre2_substring.c \
	filetype/pcre2/src/pcre2_tables.c \
	filetype/pcre2/src/pcre2_ucd.c \
	filetype/pcre2/src/pcre2_valid_utf.c \
	filetype/pcre2/src/pcre2_xclass.c \
	filetype/pcre2/src/pcre2posix.c

HEADERS += \
	$$PWD/win32/unistd.h \
	filetype/file/src/cdf.h \
	filetype/file/src/der.h \
	filetype/file/src/elfclass.h \
	filetype/file/src/file.h \
	filetype/file/src/file_opts.h \
	filetype/file/src/magic.h \
	filetype/file/src/magic.h \
	filetype/file/src/patchlevel.h \
	filetype/file/src/readelf.h \
	filetype/file/src/tar.h \
	filetype/misc/config.h \
	filetype/misc/file_config.h \
	filetype/misc/my_unistd.h \
	filetype/misc/pcre2.h \
	filetype/misc/pcre2_config.h \
	filetype/pcre2/src/pcre2_internal.h \
	filetype/pcre2/src/pcre2_intmodedep.h \
	filetype/pcre2/src/pcre2_ucp.h \
	filetype/pcre2/src/pcre2posix.h \
	filetype/pcre2/src/regex.h

SOURCES += filetype/filetype.cpp

win32:HEADERS += filetype/dirent/include/dirent.h
