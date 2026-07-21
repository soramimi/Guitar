DESTDIR = $$PWD/_bin
TARGET = chat
CONFIG += console c++20
QT += core

gcc:QMAKE_CXXFLAGS += -std=c++17 -Wall -Wextra -Werror=return-type -Werror=trigraphs -Wno-switch -Wno-reorder -Wno-unused-parameter -Wno-unused-parameter

!win32:DEFINES += HAVE_STRCASESTR
win32:QMAKE_CXXFLAGS += /FI $$PWD/../../subprojects/FileTypePlugin/file-msvc/unistd.h

INCLUDEPATH += $$PWD/../../
INCLUDEPATH += $$PWD/../../src
INCLUDEPATH += $$PWD/../../src/common
INCLUDEPATH += $$PWD/../../src/process/src
INCLUDEPATH += $$PWD/../../subprojects/FileTypePlugin
win32:INCLUDEPATH += $$PWD/../../subprojects/FileTypePlugin/file-msvc

DEFINES += NO_LOGGER
DEFINES += NO_TRACELOG
DEFINES += NOMINMAX

win32:INCLUDEPATH += C:/vcpkg/installed/x64-windows/include
win32:LIBS += -LC:/vcpkg/installed/x64-windows/lib
!win32:DEFINES += HAVE_STRCASESTR

# win32:LIBS += -lws2_32

win32 {
    LIBS += -lzlib -lzstd -lole32 -lshell32 -lws2_32
}

!win32 {
    LIBS += -lz -lzstd
}

!win32:LIBS += -lcurl
win32:LIBS += -llibcurl

FILETYPEPLUGIN = $$PWD/../../subprojects/FileTypePlugin

SOURCES += $$FILETYPEPLUGIN/lib/magic_mgc_zst.c \
	../../src/FileTypeDetector.cpp \
	../../subprojects/FileTypePlugin/src/FileType.cpp \
	../../subprojects/FileTypePlugin/src/zs.cpp

win32:CONFIG(release, debug|release):LIBS += $$FILETYPEPLUGIN/lib/file.lib $$FILETYPEPLUGIN/lib/oniguruma.lib
win32:CONFIG(debug, debug|release):LIBS += $$FILETYPEPLUGIN/lib/filed.lib $$FILETYPEPLUGIN/lib/onigurumad.lib
!win32:CONFIG(release, debug|release):LIBS += $$FILETYPEPLUGIN/lib/libfile.a $$FILETYPEPLUGIN/lib/liboniguruma.a
!win32:CONFIG(debug, debug|release):LIBS += $$FILETYPEPLUGIN/lib/libfiled.a $$FILETYPEPLUGIN/lib/libonigurumad.a

SOURCES += \
../../src/ai/AiApiBridge.cpp \
../../src/ai/CommitMessageGenerator.cpp \
../../src/ai/GenerativeAI.cpp \
../../src/common/misc.cpp \
../../src/common/q/Dir.cpp \
../../src/common/q/FileInfo.cpp \
../../src/common/realpath.cpp \
../../src/common/unicode_conversion.cpp \
../../src/common/urlencode.cpp \
../../src/inet/curlclient.cpp \
../../src/inet/inetclient.cpp \
../../src/inet/inetresolver.cpp \
../../src/process/src/AbstractProcess.cpp \
../../src/process/src/ProcessHelper.cpp \
../../subprojects/FileTypePlugin/src/FileTypeWrapper.cpp \
../../subprojects/FileTypePlugin/src/gzip.cpp \
    ../common/ConfigParser.cpp \
    ../common/LineReader.cpp \
    ../common/rwfile.cpp \
    main.cpp

HEADERS +=  \
../../src/FileTypeDetector.h \
../../src/ai/AiApiBridge.h \
../../src/ai/CommitMessageGenerator.h \
../../src/ai/GenerativeAI.h \
../../src/common/misc.h \
../../src/common/q/Dir.h \
../../src/common/q/FileInfo.h \
../../src/common/realpath.h \
../../src/common/unicode_conversion.h \
../../src/common/urlencode.h \
../../src/inet/curlclient.h \
../../src/inet/inetclient.h \
../../src/inet/inetresolver.h \
../../src/process/src/AbstractProcess.h \
../../src/process/src/BasicProcessPosix.h \
../../src/process/src/ProcessHelper.h \
../../subprojects/FileTypePlugin/src/FileType.h \
../../subprojects/FileTypePlugin/src/FileTypeWrapper.h \
../../subprojects/FileTypePlugin/src/gzip.h \
../../subprojects/FileTypePlugin/src/zs.h \
    ../common/ConfigParser.h \
    ../common/LineReader.h \
    ../common/rwfile.h \
    main.h

win32 {
SOURCES += \
../../src/process/src/ProcessWin.cpp \
../../src/process/src/BasicProcessWin.cpp \
../../src/common/wstring.cpp
HEADERS +=  \
../../src/process/src/ProcessWin.h \
../../src/process/src/BasicProcessWin.h \
../../src/common/wstring.h

}

!win32 {
SOURCES += \
../../src/process/src/BasicProcessPosix.cpp
}

