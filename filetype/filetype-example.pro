DESTDIR = $$PWD/_bin

TARGET = filetype-example
TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

DEFINES += ONIG_STATIC=1

gcc:QMAKE_CFLAGS += -include stdint.h
gcc:QMAKE_CXXFLAGS += -include stdint.h

INCLUDEPATH += $$PWD/file/src

msvc:INCLUDEPATH += $$PWD/file-msvc
# msvc:LIBS += $$PWD/_build/x86_64/Release/filetype.lib
msvc:LIBS += $$PWD/lib/filetype.lib
!msvc:LIBS += $$PWD/lib/libfiletype.a

!msvc:DEFINES += HAVE_STRCASESTR=1

SOURCES += main.cpp

HEADERS +=

