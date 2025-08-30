
TEMPLATE = app
TARGET = LogViewer
INCLUDEPATH += .
INCLUDEPATH += ../src
INCLUDEPATH += ../include
QT += core gui widgets

DESTDIR = $$PWD/_bin

# !win32:LIBS += $$PWD/../lib/liblogger.a
msvc:LIBS += -lws2_32

msvc:INCLUDEPATH += C:/vcpkg/installed/x64-windows/include
msvc:LIBS += -LC:/vcpkg/installed/x64-windows/lib

# OpenSSL

macx:INCLUDEPATH += /opt/homebrew/include
macx:LIBS += /opt/homebrew/lib/libssl.a /opt/homebrew/lib/libcrypto.a
!msvc:LIBS += -lssl -lcrypto
msvc:LIBS += -llibcrypto -llibssl

HEADERS += \
	../../src/common/base64.h \
	../../src/common/htmlencode.h \
	../../src/common/strformat.h \
	../../src/webclient.h \
	../RemoteLogger.h \
	LogView.h \
	MainWindow.h \
	main.h \
	toi.h

SOURCES += \
	../../src/common/base64.cpp \
	../../src/common/htmlencode.cpp \
	../../src/webclient.cpp \
	../RemoteLogger.cpp \
	LogView.cpp \
	MainWindow.cpp \
	main.cpp

FORMS += \
	MainWindow.ui
