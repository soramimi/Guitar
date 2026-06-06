DESTDIR = $$PWD/../_bin
TARGET = tooluse
CONFIG += console c++17
QT += core

gcc:QMAKE_CXXFLAGS += -std=c++17 -Wall -Wextra -Werror=return-type -Werror=trigraphs -Wno-switch -Wno-reorder -Wno-unused-parameter -Wno-unused-parameter

BASEDIR = $$PWD/../..
SRCDIR = $$BASEDIR/../src
INCLUDEPATH += $$BASEDIR/
INCLUDEPATH += $$SRCDIR
INCLUDEPATH += $$SRCDIR/common
INCLUDEPATH += $$BASEDIR/../filetype/src

DEFINES += NO_LOGGER
DEFINES += NO_TRACELOG

msvc:INCLUDEPATH += C:/vcpkg/installed/x64-windows/include
msvc:LIBS += -LC:/vcpkg/installed/x64-windows/lib

# msvc:LIBS += -lws2_32

LIBS += -lssl -lcrypto

msvc {
    LIBS += -lzlib -lole32 -lshell32
}

!msvc {
    LIBS += -lz
}

!msvc:LIBS += -lcurl
msvc:LIBS += -llibcurl

include($$BASEDIR/../libfiletype.pri)

SRC = $$PWD/../../../src

SOURCES += \
	$$SRC/AbstractProcess.cpp \
	$$SRC/AiApiBridge.cpp \
	$$SRC/FileTypeDetector.cpp \
	$$SRC/Logger.cpp \
	$$SRC/MyProcess.cpp \
	$$SRC/ai/GenerativeAI.cpp \
	$$SRC/common/misc.cpp \
	$$SRC/common/q/Dir.cpp \
	$$SRC/common/q/FileInfo.cpp \
	$$SRC/common/realpath.cpp \
	$$SRC/common/unicode_conversion.cpp \
	$$SRC/common/urlencode.cpp \
	$$SRC/inet/curlclient.cpp \
	$$SRC/inet/inetclient.cpp \
	$$SRC/inet/inetresolver.cpp \
	$$SRC/inet/webclient.cpp \
	$$SRC/process/MyProcess2.cpp \
	../../common/ConfigParser.cpp \
	../../common/LineReader.cpp \
	../../common/rwfile.cpp \
	../src/main.cpp

HEADERS +=  \
	$$SRC/AbstractProcess.h \
	$$SRC/AiApiBridge.h \
	$$SRC/FileTypeDetector.h \
	$$SRC/Logger.h \
	$$SRC/MyProcess.h \
	$$SRC/ai/GenerativeAI.h \
	$$SRC/common/base64.h \
	$$SRC/common/joinpath.h \
	$$SRC/common/jstream.h \
	$$SRC/common/misc.h \
	$$SRC/common/q/Dir.h \
	$$SRC/common/q/FileInfo.h \
	$$SRC/common/realpath.h \
	$$SRC/common/unicode_conversion.h \
	$$SRC/common/urlencode.h \
	$$SRC/inet/curlclient.h \
	$$SRC/inet/inetclient.h \
	$$SRC/inet/inetresolver.h \
	$$SRC/inet/webclient.h \
	$$SRC/process/MyProcess2.h \
	../../common/ConfigParser.h \
	../../common/LineReader.h \
	../../common/rwfile.h \
	../src/main.h

msvc {
SOURCES += \
	$$SRC/common/wstring.cpp \
	$$SRC/process/ProcessWin.cpp \
	$$SRC/win32/Win32Process.cpp
HEADERS +=  \
	$$SRC/common/wstring.h \
	$$SRC/process/ProcessWin.h \
	$$SRC/win32/Win32Process.h
}

!msvc {
	SOURCES += \
		$$SRC/process/ProcessPosix.cpp \
		$$SRC/unix/UnixProcess.cpp
	HEADERS +=  \
		$$SRC/process/ProcessPosix.h \
		$$SRC/unix/UnixProcess.h
}

