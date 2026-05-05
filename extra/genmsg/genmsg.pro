DESTDIR = $$PWD/_bin
CONFIG += c++17
QT += core
INCLUDEPATH += $$PWD/../
INCLUDEPATH += $$PWD/../../src
INCLUDEPATH += $$PWD/../../filetype/src
# INCLUDEPATH += $$PWD/../../src/texteditor
INCLUDEPATH += $$PWD/../../src/common

DEFINES += NO_LOGGER
DEFINES += NO_TRACELOG
DEFINES += USE_LIBCURL

msvc:INCLUDEPATH += C:/vcpkg/installed/x64-windows/include
msvc:LIBS += -LC:/vcpkg/installed/x64-windows/lib

msvc {
	LIBS += -lzlib
}

!msvc {
	LIBS += -lz
}

!msvc:LIBS += -lcurl
msvc:LIBS += -llibcurl

# !msvc:LIBS += -lssl -lcrypto
# msvc:LIBS += -llibcrypto -llibssl

msvc:CONFIG(release, debug|release):LIBS += $$PWD/../../filetype/lib/filetype.lib $$PWD/../../filetype/lib/file.lib $$PWD/../../filetype/lib/oniguruma.lib
msvc:CONFIG(debug, debug|release):LIBS += $$PWD/../../filetype/lib/filetyped.lib $$PWD/../../filetype/lib/filed.lib $$PWD/../../filetype/lib/onigurumad.lib
!msvc:CONFIG(release, debug|release):LIBS += $$PWD/../../filetype/lib/libfiletype.a $$PWD/../../filetype/lib/libfile.a $$PWD/../../filetype/lib/liboniguruma.a
!msvc:CONFIG(debug, debug|release):LIBS += $$PWD/../../filetype/lib/libfiletyped.a $$PWD/../../filetype/lib/libfiled.a $$PWD/../../filetype/lib/libonigurumad.a

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
	../../src/curlclient.cpp \
	../../src/genmsg.cpp \
	../../src/inetclient.cpp \
	../../src/urlencode.cpp
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
	../../src/curlclient.h \
	../../src/genmsg.h \
	../../src/inetclient.h \
	../../src/urlencode.h

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

