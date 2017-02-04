unix:TARGET = z
win32:TARGET = libz
TEMPLATE = lib
CONFIG += console staticlib
CONFIG -= app_bundle
CONFIG -= qt

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

