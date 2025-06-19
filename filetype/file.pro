DESTDIR = $$PWD/_bin

TARGET = file
TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

gcc:QMAKE_CFLAGS += -include file/config.h

# INCLUDEPATH += misc
# INCLUDEPATH += file/src
# INCLUDEPATH += pcre2/src
win32:INCLUDEPATH += dirent/include
msvc:INCLUDEPATH += file-msvc
msvc:QMAKE_CFLAGS += /FI file-msvc/config.h /FI inttypes.h

# DEFINES += "HAVE_CONFIG_H=1" "_SSIZE_T_DEFINED=1" "PCRE2_CODE_UNIT_WIDTH=8"
win32:DEFINES += "_WIN32"

win32:HEADERS += dirent.h
win32:HEADERS += misc/unistd.h

gcc:LIBS += $$PWD/lib/libfiletype.a
msvc:LIBS += $$PWD/lib/filetype.lib

HEADERS += \
    file-msvc/getopt.h

SOURCES += file/src/file.c \
    file-msvc/getopt.c
