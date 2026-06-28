
QT = core

CONFIG(release,debug|release):TARGET = filetypeplugin
CONFIG(debug,debug|release):TARGET = filetypeplugind
TEMPLATE = lib
CONFIG += plugin

DESTDIR = $$PWD/../../lib

gcc:DEFINES += HAVE_STRCASESTR

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
	src/gzip.cpp \
	src/zs.cpp

HEADERS += \
	src/AbstractSimpleIO.h \
	src/FileTypeWrapper.h \
	src/gzip.h \
	src/zs.h

# SOURCES += lib/magic_mgc_gz.c
SOURCES += lib/magic_mgc_zst.c

