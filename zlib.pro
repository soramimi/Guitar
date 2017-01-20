TEMPLATE = lib
CONFIG += console staticlib
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    ../zlib-1.2.11/gzclose.c \
    ../zlib-1.2.11/uncompr.c \
    ../zlib-1.2.11/compress.c \
    ../zlib-1.2.11/adler32.c \
    ../zlib-1.2.11/crc32.c \
    ../zlib-1.2.11/gzread.c \
    ../zlib-1.2.11/infback.c \
    ../zlib-1.2.11/inflate.c \
    ../zlib-1.2.11/trees.c \
    ../zlib-1.2.11/zutil.c \
    ../zlib-1.2.11/inffast.c \
    ../zlib-1.2.11/inftrees.c \
    ../zlib-1.2.11/gzlib.c \
    ../zlib-1.2.11/gzwrite.c \
    ../zlib-1.2.11/deflate.c

HEADERS += \
    ../zlib-1.2.11/trees.h \
    ../zlib-1.2.11/inftrees.h \
    ../zlib-1.2.11/inffast.h \
    ../zlib-1.2.11/inffixed.h \
    ../zlib-1.2.11/crc32.h \
    ../zlib-1.2.11/inflate.h \
    ../zlib-1.2.11/gzguts.h \
    ../zlib-1.2.11/zutil.h \
    ../zlib-1.2.11/deflate.h \
    ../zlib-1.2.11/zconf.h \
    ../zlib-1.2.11/zlib.h

