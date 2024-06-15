TARGET = filetype
TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

DESTDIR = $$PWD/_bin

INCLUDEPATH += misc
INCLUDEPATH += file/src
INCLUDEPATH += pcre2/src
win32:INCLUDEPATH += dirent/include
win32:QMAKE_CFLAGS += /FI unistd.h
win32:LIBS += $$PWD/_build/x86_64/Release/filetype.lib
!win32:LIBS += $$PWD/_bin/libfiletype.a

DEFINES += "HAVE_CONFIG_H=1" "_SSIZE_T_DEFINED=1" "PCRE2_CODE_UNIT_WIDTH=8"

SOURCES += main.cpp
