include(../../common.pri)

!win32:DESTDIR = $$PWD/lib
win32:DESTDIR = $$PWD/../../_bin
CONFIG(release,debug|release):TARGET = filetypeplugin
CONFIG(debug,debug|release):TARGET = filetypeplugind

TEMPLATE = lib
CONFIG += plugin
QT = core

gcc:DEFINES += HAVE_STRCASESTR
win32:QMAKE_CXXFLAGS += /FI $$PWD/file-msvc/unistd.h

win32:DEFINES += DLLEXPORT=__declspec(dllexport)
win32:INCLUDEPATH += C:/vcpkg/installed/x64-windows/include

win32:INCLUDEPATH += file-msvc
gcc:INCLUDEPATH += file-gcc
INCLUDEPATH += file

unix:LIBS += -ldl
win32:LIBS += -lole32 -loleaut32

# zlib

win32 {
	LIBS += -lzlib
	LIBS += -lzstd
}

!win32 {
	LIBS += -lz
	LIBS += -lzstd
}

#

win32:LIBS += -LC:/vcpkg/installed/x64-windows/lib

win32:CONFIG(release, debug|release):LIBS += $$PWD/lib/file.lib $$PWD/lib/oniguruma.lib
win32:CONFIG(debug, debug|release):LIBS += $$PWD/lib/filed.lib $$PWD/lib/onigurumad.lib
!win32:CONFIG(release, debug|release):LIBS += $$PWD/lib/libfile.a $$PWD/lib/liboniguruma.a
!win32:CONFIG(debug, debug|release):LIBS += $$PWD/lib/libfiled.a $$PWD/lib/libonigurumad.a

HEADERS += \
	src/FileType.h \
	src/FileTypeInterface.h \
	src/FileTypePlugin.h
SOURCES += \
	src/FileType.cpp \
	src/FileTypeInterface.cpp \
	src/FileTypePlugin.cpp

DISTFILES += \
	src/Makefile \
	filetypeplugin.json


SOURCES += \
	lib/magic_mgc_gz.c \
	src/AbstractSimpleIO.cpp \
	src/FileTypeWrapper.cpp \
	src/zs.cpp

HEADERS += \
	src/AbstractSimpleIO.h \
	src/FileTypeWrapper.h \
	src/zs.h

# SOURCES += lib/magic_mgc_gz.c
SOURCES += lib/magic_mgc_zst.c

