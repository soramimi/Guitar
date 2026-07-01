
QT = core

CONFIG(release,debug|release):TARGET = filetypeplugin
CONFIG(debug,debug|release):TARGET = filetypeplugind
TEMPLATE = lib
CONFIG += plugin

DESTDIR = $$PWD/../../lib

gcc:DEFINES += HAVE_STRCASESTR
msvc:QMAKE_CXXFLAGS += /FI $$PWD/file-msvc/unistd.h

msvc:DEFINES += DLLEXPORT=__declspec(dllexport)
msvc:INCLUDEPATH += C:/vcpkg/installed/x64-windows/include

msvc:INCLUDEPATH += file-msvc
gcc:INCLUDEPATH += file-gcc
INCLUDEPATH += file

unix:LIBS += -ldl
win32:LIBS += -lole32 -loleaut32

# zlib

msvc {
	LIBS += -lzlib
	LIBS += -lzstd
}

!msvc {
	LIBS += -lz
	LIBS += -lzstd
}

#

msvc:LIBS += -LC:/vcpkg/installed/x64-windows/lib

msvc:CONFIG(release, debug|release):LIBS += $$PWD/lib/file.lib $$PWD/lib/oniguruma.lib
msvc:CONFIG(debug, debug|release):LIBS += $$PWD/lib/filed.lib $$PWD/lib/onigurumad.lib
!msvc:CONFIG(release, debug|release):LIBS += $$PWD/lib/libfile.a $$PWD/lib/liboniguruma.a
!msvc:CONFIG(debug, debug|release):LIBS += $$PWD/lib/libfiled.a $$PWD/lib/libonigurumad.a

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

