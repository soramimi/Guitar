QMAKE_PROJECT_DEPTH = 0

DESTDIR = $$PWD/lib
CONFIG(debug,debug|release):TARGET = filed
CONFIG(release,debug|release):TARGET = file

TEMPLATE = lib
CONFIG += staticlib console c++11
CONFIG -= app_bundle
CONFIG -= qt

DEFINES += ONIG_STATIC=1 HAVE_CONFIG_H=1

msvc:INCLUDEPATH += file-msvc
msvc:INCLUDEPATH += file-msvc/dirent/include
msvc:INCLUDEPATH += file-msvc
msvc:QMAKE_CFLAGS += /FI file-msvc/config.h /FI inttypes.h

msvc:INCLUDEPATH += C:/vcpkg/installed/x64-windows/include

gcc:INCLUDEPATH += file-gcc
gcc:QMAKE_CFLAGS += -include config.h -include stdint.h
gcc:QMAKE_CXXFLAGS += -include config.h -include stdint.h
macx:QMAKE_CFLAGS += -include xlocale.h

INCLUDEPATH += file

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
	file/src/print.c \
	file/src/readcdf.c \
	file/src/readelf.c \
	file/src/seccomp.c \
	file/src/softmagic.c \
	file/src/strcasestr.c \
	file/src/vasprintf.c

HEADERS += \
	file-msvc/config.h \
	file-msvc/magic.h \
	file/src/cdf.h \
	file/src/der.h \
	file/src/elfclass.h \
	file/src/file.h \
	file/src/file_opts.h \
	file/src/patchlevel.h \
	file/src/readelf.h \
	file/src/tar.h

msvc:SOURCES += file/src/pread.c file/src/strlcat.c file/src/strlcpy.c
gcc:!macx:SOURCES += file/src/pread.c file/src/strlcat.c file/src/strlcpy.c

