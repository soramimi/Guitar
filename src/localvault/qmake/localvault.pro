DESTDIR = $$PWD/../_bin
TEMPLATE = app
TARGET = localvault
QT += core widgets
CONFIG += c++17
CONFIG -= app_bundle
linux|macx:CONFIG += link_pkgconfig

msvc:INCLUDEPATH += C:/vcpkg/installed/x64-windows/include
msvc:CONFIG(debug,debug|release) {
	LIBS += -LC:/vcpkg/installed/x64-windows/debug/lib
}

msvc:CONFIG(release,debug|release) {
	LIBS += -LC:/vcpkg/installed/x64-windows/lib
}

# GCC 7/8 では std::filesystem 用に -lstdc++fs が必要な場合がある
# GCC 9+ では無害だが無視される
linux-g++*:LIBS += -lstdc++fs

SOURCES += ../src/main.cpp

LOCALVAULT_SRC = $$PWD/../src
include(localvault.pri)
