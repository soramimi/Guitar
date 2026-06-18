QMAKE_PROJECT_DEPTH = 0

DESTDIR = $$PWD/../_bin
# DESTDIR = $$PWD/lib

TARGET = filetype
# CONFIG(debug,debug|release):TARGET = filetyped
# CONFIG(release,debug|release):TARGET = filetype

TEMPLATE = lib
CONFIG += staticlib console c++11
CONFIG -= app_bundle
CONFIG -= qt

DEFINES += ONIG_STATIC=1 HAVE_CONFIG_H=1

msvc:DEFINES += DLLEXPORT=__declspec(dllexport)

msvc:INCLUDEPATH += file-msvc
gcc:INCLUDEPATH += file-gcc
INCLUDEPATH += file

msvc:QMAKE_CXXFLAGS += /FI $$PWD/file-msvc/unistd.h

msvc:INCLUDEPATH += C:/vcpkg/installed/x64-windows/include
msvc:LIBS += -LC:/vcpkg/installed/x64-windows/lib

macx:QMAKE_CFLAGS += -include xlocale.h
macx:QMAKE_CXXFLAGS += -include xlocale.h

msvc:CONFIG(release, debug|release):LIBS += $$PWD/lib/file.lib $$PWD/lib/oniguruma.lib
msvc:CONFIG(debug, debug|release):LIBS += $$PWD/lib/filed.lib $$PWD/lib/onigurumad.lib
!msvc:CONFIG(release, debug|release):LIBS += $$PWD/lib/libfile.a $$PWD/lib/liboniguruma.a
!msvc:CONFIG(debug, debug|release):LIBS += $$PWD/lib/libfiled.a $$PWD/lib/libonigurumad.a

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

SOURCES += \
	lib/magic_mgc_gz.c \
	src/AbstractSimpleIO.cpp \
	src/FileType.cpp \
	src/gzip.cpp \
	src/zs.cpp

HEADERS += \
	src/AbstractSimpleIO.h \
	src/FileType.h \
	src/gzip.h \
	src/zs.h

# SOURCES += lib/magic_mgc_gz.c
SOURCES += lib/magic_mgc_zst.c

