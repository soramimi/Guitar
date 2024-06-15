TARGET = filetype
TEMPLATE = lib
CONFIG += staticlib console c++11
CONFIG -= app_bundle
CONFIG -= qt

DESTDIR = $$PWD/_bin

INCLUDEPATH += misc
INCLUDEPATH += file/src
INCLUDEPATH += pcre2/src
# win32:INCLUDEPATH += dirent/include
# win32:QMAKE_CFLAGS += /FI unistd.h
# win32:LIBS += -lshlwapi

DEFINES += "HAVE_CONFIG_H=1" "_SSIZE_T_DEFINED=1" "PCRE2_CODE_UNIT_WIDTH=8"
!win32:DEFINES += "HAVE_MKSTEMP=1"

win32:HEADERS += dirent.h

SOURCES += \
	file/src/apprentice.c \
	file/src/apptype.c \
	file/src/ascmagic.c \
	file/src/asctime_r.c \
	file/src/asprintf.c \
	file/src/buffer.c \
	file/src/cdf.c \
	file/src/cdf_time.c \
	file/src/compress.c \
	file/src/ctime_r.c \
	file/src/der.c \
	file/src/dprintf.c \
	file/src/encoding.c \
	file/src/fmtcheck.c \
	file/src/fsmagic.c \
	file/src/funcs.c \
	file/src/getline.c \
	file/src/gmtime_r.c \
	file/src/is_csv.c \
	file/src/is_json.c \
	file/src/is_simh.c \
	file/src/is_tar.c \
	file/src/localtime_r.c \
	file/src/magic.c \
	file/src/pread.c \
	file/src/print.c \
	file/src/readcdf.c \
	file/src/readelf.c \
	file/src/seccomp.c \
	file/src/softmagic.c \
	file/src/strcasestr.c \
	file/src/strlcat.c \
	file/src/strlcpy.c \
	file/src/vasprintf.c \
	pcre2/src/pcre2_auto_possess.c \
	pcre2/src/pcre2_chartables.c \
	pcre2/src/pcre2_compile.c \
	pcre2/src/pcre2_config.c \
	pcre2/src/pcre2_context.c \
	pcre2/src/pcre2_convert.c \
	pcre2/src/pcre2_dfa_match.c \
	pcre2/src/pcre2_error.c \
	pcre2/src/pcre2_find_bracket.c \
	pcre2/src/pcre2_fuzzsupport.c \
	pcre2/src/pcre2_jit_compile.c \
	pcre2/src/pcre2_jit_match.c \
	pcre2/src/pcre2_jit_misc.c \
	pcre2/src/pcre2_maketables.c \
	pcre2/src/pcre2_match.c \
	pcre2/src/pcre2_match_data.c \
	pcre2/src/pcre2_newline.c \
	pcre2/src/pcre2_ord2utf.c \
	pcre2/src/pcre2_pattern_info.c \
	pcre2/src/pcre2_printint.c \
	pcre2/src/pcre2_serialize.c \
	pcre2/src/pcre2_string_utils.c \
	pcre2/src/pcre2_study.c \
	pcre2/src/pcre2_substitute.c \
	pcre2/src/pcre2_substring.c \
	pcre2/src/pcre2_tables.c \
	pcre2/src/pcre2_ucd.c \
	pcre2/src/pcre2_valid_utf.c \
	pcre2/src/pcre2_xclass.c \
	pcre2/src/pcre2posix.c

HEADERS += \
	file/src/cdf.h \
	file/src/der.h \
	file/src/elfclass.h \
	file/src/file.h \
	file/src/file_opts.h \
	file/src/magic.h \
	file/src/magic.h \
	file/src/patchlevel.h \
	file/src/readelf.h \
	file/src/tar.h \
	misc/config.h \
	misc/file_config.h \
	misc/pcre2.h \
	misc/pcre2_config.h \
	misc/unistd.h \
	pcre2/src/pcre2_internal.h \
	pcre2/src/pcre2_intmodedep.h \
	pcre2/src/pcre2_ucp.h \
	pcre2/src/pcre2posix.h \
	pcre2/src/regex.h


