DESTDIR = $$PWD/_bin
TARGET = chat
CONFIG += console c++17
QT += core

gcc:QMAKE_CXXFLAGS += -std=c++17 -Wall -Wextra -Werror=return-type -Werror=trigraphs -Wno-switch -Wno-reorder -Wno-unused-parameter -Wno-unused-parameter

INCLUDEPATH += $$PWD/../
INCLUDEPATH += $$PWD/../../src
INCLUDEPATH += $$PWD/../../filetype/src
INCLUDEPATH += $$PWD/$$SRC/common

DEFINES += NO_LOGGER
DEFINES += NO_TRACELOG

msvc:INCLUDEPATH += C:/vcpkg/installed/x64-windows/include
msvc:LIBS += -LC:/vcpkg/installed/x64-windows/lib

# msvc:LIBS += -lws2_32

msvc {
    LIBS += -lzlib -lole32 -lshell32
}

!msvc {
    LIBS += -lz
}

!msvc:LIBS += -lcurl
msvc:LIBS += -llibcurl

include($$PWD/../../libfiletype.pri)

SRC = $$PWD/../../src

SOURCES += \
    $$SRC/AbstractProcess.cpp \
    $$SRC/FileTypeDetector.cpp \
    $$SRC/ai/GenerativeAI.cpp \
    $$SRC/Logger.cpp \
    $$SRC/MyProcess.cpp \
    $$SRC/common/misc.cpp \
    $$SRC/common/q/Dir.cpp \
    $$SRC/common/q/FileInfo.cpp \
    $$SRC/common/realpath.cpp \
    $$SRC/common/unicode_conversion.cpp \
    $$SRC/common/urlencode.cpp \
    $$SRC/inet/curlclient.cpp \
    $$SRC/inet/inetclient.cpp \
    $$SRC/process/MyProcess2.cpp \
    $$SRC/AiApiBridge.cpp \
    ../common/ConfigParser.cpp \
    ../common/LineReader.cpp \
    ../common/rwfile.cpp \
    main.cpp

HEADERS +=  \
    $$SRC/AbstractProcess.h \
    $$SRC/FileTypeDetector.h \
    $$SRC/ai/GenerativeAI.h \
    $$SRC/Logger.h \
    $$SRC/MyProcess.h \
    $$SRC/common/base64.h \
    $$SRC/common/joinpath.h \
    $$SRC/common/misc.h \
    $$SRC/common/q/Dir.h \
    $$SRC/common/q/FileInfo.h \
    $$SRC/common/realpath.h \
    $$SRC/common/unicode_conversion.h \
    $$SRC/common/urlencode.h \
    $$SRC/inet/curlclient.h \
    $$SRC/inet/inetclient.h \
    $$SRC/process/MyProcess2.h \
    $$SRC/AiApiBridge.h \
    ../common/ConfigParser.h \
    ../common/LineReader.h \
    ../common/rwfile.h \
    main.h

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

