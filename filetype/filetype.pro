TARGET = filetype
TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

DESTDIR = $$PWD/_bin

INCLUDEPATH += $$PWD/misc

# win32:LIBS += $$PWD/_build/x86_64/Release/filetype.lib
win32:LIBS += $$PWD/_bin/filetype.lib
!win32:LIBS += $$PWD/_bin/libfiletype.a

!win32:DEFINES += HAVE_STRCASESTR=1

SOURCES += main.cpp \
	src/FileType.cpp

HEADERS += \
	src/FileType.h

