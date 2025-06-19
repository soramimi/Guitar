DESTDIR = $$PWD/lib

TARGET = filetype
TEMPLATE = lib
CONFIG += staticlib console c++11
CONFIG -= app_bundle
CONFIG -= qt

DEFINES += ONIG_STATIC=1

# INCLUDEPATH += misc
INCLUDEPATH += file
msvc:INCLUDEPATH += file-msvc
msvc:INCLUDEPATH += file-msvc/dirent/include
# win32:INCLUDEPATH += misc/win32
# win32:INCLUDEPATH += misc/win32/dirent/include
# win32:QMAKE_CFLAGS += /FI unistd.h
# win32:LIBS += -lshlwapi
msvc:DEFINES += HAVE_CONFIG_H=1
msvc:INCLUDEPATH += $$PWD/../../zlib
msvc:INCLUDEPATH += file-msvc
msvc:QMAKE_CFLAGS += /FI file-msvc/config.h /FI inttypes.h

gcc:QMAKE_CFLAGS += -include src/fileconfig.h -include stdint.h
gcc:QMAKE_CXXFLAGS += -include src/fileconfig.h -include stdint.h

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

msvc:SOURCES += file/src/strlcat.c file/src/strlcpy.c file/src/pread.c

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

    # src/fileconfig.h

HEADERS += \
	oniguruma/src/config.h \
	oniguruma/src/oniggnu.h \
	oniguruma/src/onigposix.h \
	oniguruma/src/oniguruma.h \
	oniguruma/src/regenc.h \
	oniguruma/src/regint.h \
	oniguruma/src/regparse.h

# oniguruma/src/st.h

SOURCES += \
	oniguruma/src/ascii.c \
	oniguruma/src/big5.c \
	oniguruma/src/cp1251.c \
	oniguruma/src/euc_jp.c \
	oniguruma/src/euc_jp_prop.c \
	oniguruma/src/euc_kr.c \
	oniguruma/src/euc_tw.c \
	oniguruma/src/gb18030.c \
	oniguruma/src/iso8859_1.c \
	oniguruma/src/iso8859_10.c \
	oniguruma/src/iso8859_11.c \
	oniguruma/src/iso8859_13.c \
	oniguruma/src/iso8859_14.c \
	oniguruma/src/iso8859_15.c \
	oniguruma/src/iso8859_16.c \
	oniguruma/src/iso8859_2.c \
	oniguruma/src/iso8859_3.c \
	oniguruma/src/iso8859_4.c \
	oniguruma/src/iso8859_5.c \
	oniguruma/src/iso8859_6.c \
	oniguruma/src/iso8859_7.c \
	oniguruma/src/iso8859_8.c \
	oniguruma/src/iso8859_9.c \
	oniguruma/src/koi8.c \
	oniguruma/src/koi8_r.c \
	oniguruma/src/onig_init.c \
	oniguruma/src/regcomp.c \
	oniguruma/src/regenc.c \
	oniguruma/src/regerror.c \
	oniguruma/src/regexec.c \
	oniguruma/src/regext.c \
	oniguruma/src/reggnu.c \
	oniguruma/src/regparse.c \
	oniguruma/src/regposerr.c \
	oniguruma/src/regposix.c \
	oniguruma/src/regsyntax.c \
	oniguruma/src/regtrav.c \
	oniguruma/src/regversion.c \
	oniguruma/src/sjis.c \
	oniguruma/src/sjis_prop.c \
	oniguruma/src/st.c \
	oniguruma/src/unicode.c \
	oniguruma/src/unicode_egcb_data.c \
	oniguruma/src/unicode_fold1_key.c \
	oniguruma/src/unicode_fold2_key.c \
	oniguruma/src/unicode_fold3_key.c \
	oniguruma/src/unicode_fold_data.c \
	oniguruma/src/unicode_property_data.c \
	oniguruma/src/unicode_property_data_posix.c \
	oniguruma/src/unicode_unfold_key.c \
	oniguruma/src/unicode_wb_data.c \
	oniguruma/src/utf16_be.c \
	oniguruma/src/utf16_le.c \
	oniguruma/src/utf32_be.c \
	oniguruma/src/utf32_le.c \
	oniguruma/src/utf8.c

SOURCES += \
	src/FileType.cpp

# HEADERS += \
# 	misc/config.h \
# 	misc/file_config.h \
# 	src/FileType.h


