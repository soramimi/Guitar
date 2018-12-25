unix:CONFIG(debug,debug|release):TARGET = zd
unix:CONFIG(release,debug|release):TARGET = z
win32:CONFIG(debug,debug|release):TARGET = libzd
win32:CONFIG(release,debug|release):TARGET = libz

TEMPLATE = lib
CONFIG += console staticlib
CONFIG -= app_bundle
CONFIG -= qt

DESTDIR = $$PWD/_bin

SOURCES += \
    ../zlib/gzclose.c \
    ../zlib/uncompr.c \
    ../zlib/compress.c \
    ../zlib/adler32.c \
    ../zlib/crc32.c \
    ../zlib/gzread.c \
    ../zlib/infback.c \
    ../zlib/inflate.c \
    ../zlib/trees.c \
    ../zlib/zutil.c \
    ../zlib/inffast.c \
    ../zlib/inftrees.c \
    ../zlib/gzlib.c \
    ../zlib/gzwrite.c \
    ../zlib/deflate.c

HEADERS += \
    ../zlib/trees.h \
    ../zlib/inftrees.h \
    ../zlib/inffast.h \
    ../zlib/inffixed.h \
    ../zlib/crc32.h \
    ../zlib/inflate.h \
    ../zlib/gzguts.h \
    ../zlib/zutil.h \
    ../zlib/deflate.h \
    ../zlib/zconf.h \
    ../zlib/zlib.h

