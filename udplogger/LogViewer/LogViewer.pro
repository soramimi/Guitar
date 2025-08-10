
TEMPLATE = app
TARGET = LogViewer
INCLUDEPATH += .
INCLUDEPATH += ../src
INCLUDEPATH += ../include
QT += core gui widgets

# !win32:LIBS += $$PWD/../lib/liblogger.a
win32:LIBS += -lws2_32

HEADERS += \
	../../src/common/htmlencode.h \
	../../src/common/strformat.h \
	../RemoteLogger.h \
	LogView.h \
	MainWindow.h \
	main.h \
	toi.h

SOURCES += \
	../../src/common/htmlencode.cpp \
	../RemoteLogger.cpp \
	LogView.cpp \
	MainWindow.cpp \
	main.cpp

FORMS += \
	MainWindow.ui
