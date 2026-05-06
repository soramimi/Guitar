DESTDIR = $$PWD/_bin
CONFIG += console c++17
QT += core

INCLUDEPATH += $$PWD/../
INCLUDEPATH += $$PWD/../../src
INCLUDEPATH += $$PWD/../../filetype/src
INCLUDEPATH += $$PWD/../../src/common

DEFINES += NO_LOGGER
DEFINES += NO_TRACELOG

msvc:INCLUDEPATH += C:/vcpkg/installed/x64-windows/include
msvc:LIBS += -LC:/vcpkg/installed/x64-windows/lib

msvc:LIBS += -lws2_32

msvc {
    LIBS += -lzlib
}

!msvc {
    LIBS += -lz
}

!msvc:LIBS += -lcurl
msvc:LIBS += -llibcurl

include($$PWD/../../libfiletype.pri)

SOURCES += \
    ../../src/AbstractProcess.cpp \
    ../../src/CommitMessageGenerator.cpp \
    ../../src/FileTypeDetector.cpp \
    ../../src/GenerativeAI.cpp \
    ../../src/Logger.cpp \
    ../../src/MemoryReader.cpp \
    ../../src/MyProcess.cpp \
    ../../src/common/misc.cpp \
    ../../src/common/q/Dir.cpp \
    ../../src/common/q/FileInfo.cpp \
    ../../src/common/urlencode.cpp \
    ../../src/curlclient.cpp \
    ../../src/inetclient.cpp \
    main.cpp \
    selectitem.cpp
HEADERS +=  \
    ../../src/AbstractProcess.h \
    ../../src/CommitMessageGenerator.h \
    ../../src/FileTypeDetector.h \
    ../../src/GenerativeAI.h \
    ../../src/Logger.h \
    ../../src/MemoryReader.h \
    ../../src/MyProcess.h \
    ../../src/common/base64.h \
    ../../src/common/joinpath.h \
    ../../src/common/misc.h \
    ../../src/common/q/Dir.h \
    ../../src/common/q/FileInfo.h \
    ../../src/common/urlencode.h \
    ../../src/curlclient.h \
    ../../src/inetclient.h \
    main.h \
    selectitem.h

msvc {
SOURCES += \
../../src/win32/Win32Process.cpp
HEADERS +=  \
../../src/win32/Win32Process.h

}

!msvc {
SOURCES += \
../../src/unix/UnixProcess.cpp
HEADERS +=  \
../../src/unix/UnixProcess.h

}

