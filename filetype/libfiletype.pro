DESTDIR = $$PWD/lib
CONFIG(debug,debug|release):TARGET = filetyped
CONFIG(release,debug|release):TARGET = filetype

TEMPLATE = lib
CONFIG += staticlib console c++11
CONFIG -= app_bundle
CONFIG -= qt

DEFINES += ONIG_STATIC=1 HAVE_CONFIG_H=1

msvc:INCLUDEPATH += file-msvc
gcc:INCLUDEPATH += file-gcc
INCLUDEPATH += file

macx:QMAKE_CFLAGS += -include xlocale.h
macx:QMAKE_CXXFLAGS += -include xlocale.h

SOURCES += \
	src/FileType.cpp

HEADERS += \
	src/FileType.h


