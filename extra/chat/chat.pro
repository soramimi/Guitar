DESTDIR = $$PWD/_bin
TARGET = chat
CONFIG += console c++17
QT += core

gcc:QMAKE_CXXFLAGS += -std=c++17 -Wall -Wextra -Werror=return-type -Werror=trigraphs -Wno-switch -Wno-reorder -Wno-unused-parameter -Wno-unused-parameter

INCLUDEPATH += $$PWD/../
INCLUDEPATH += $$PWD/../../src
INCLUDEPATH += $$PWD/../../filetype/src
INCLUDEPATH += $$PWD/../../src/common

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

SOURCES += \
    ../../src/AbstractProcess.cpp \
    ../../src/FileTypeDetector.cpp \
    ../../src/GenerativeAI.cpp \
    ../../src/Logger.cpp \
    ../../src/MyProcess.cpp \
    ../../src/common/misc.cpp \
    ../../src/common/q/Dir.cpp \
    ../../src/common/q/FileInfo.cpp \
    ../../src/common/realpath.cpp \
    ../../src/common/unicode_conversion.cpp \
    ../../src/common/urlencode.cpp \
    ../../src/curlclient.cpp \
    ../../src/inetclient.cpp \
    ../../src/process/MyProcess2.cpp \
    ../../src/AiApiBridge.cpp \
    ../common/ConfigParser.cpp \
    ../common/LineReader.cpp \
    ../common/rwfile.cpp \
    main.cpp

HEADERS +=  \
    ../../src/AbstractProcess.h \
    ../../src/FileTypeDetector.h \
    ../../src/GenerativeAI.h \
    ../../src/Logger.h \
    ../../src/MyProcess.h \
    ../../src/common/base64.h \
    ../../src/common/joinpath.h \
    ../../src/common/misc.h \
    ../../src/common/q/Dir.h \
    ../../src/common/q/FileInfo.h \
    ../../src/common/realpath.h \
    ../../src/common/unicode_conversion.h \
    ../../src/common/urlencode.h \
    ../../src/curlclient.h \
    ../../src/inetclient.h \
    ../../src/process/MyProcess2.h \
    ../../src/AiApiBridge.h \
    ../common/ConfigParser.h \
    ../common/LineReader.h \
    ../common/rwfile.h \
    main.h

msvc {
SOURCES += \
../../src/common/wstring.cpp \
	../../src/process/ProcessWin.cpp \
../../src/win32/Win32Process.cpp
HEADERS +=  \
../../src/common/wstring.h \
	../../src/process/ProcessWin.h \
../../src/win32/Win32Process.h

}

!msvc {
SOURCES += \
../../src/process/ProcessPosix.cpp \
../../src/unix/UnixProcess.cpp
HEADERS +=  \
../../src/process/ProcessPosix.h \
../../src/unix/UnixProcess.h
}

