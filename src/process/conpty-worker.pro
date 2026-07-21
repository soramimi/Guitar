TARGET = conpty-worker
DESTDIR = $$PWD/_bin
TEMPLATE = app
CONFIG -= qt
CONFIG += console
CONFIG += c++17

win32:INCLUDEPATH += winpty\include
win32:LIBS += -L$$PWD\winpty\x64\lib -lwinpty

win32:DEFINES += NOMINMAX

DEFINES += CONPTY_WORKER

HEADERS += \
	sampleapp/base64.h \
	sampleapp/misc.h \
	sampleapp/unicode_conversion.h
	
SOURCES += \
	sampleapp/main.cpp \
	sampleapp/misc.cpp \
	sampleapp/unicode_conversion.cpp

INCLUDEPATH += $$PWD/sampleapp

PROCESS_SRC += $$PWD/src
PROCESS_PRI = $$PROCESS_SRC/../process.pri
INCLUDEPATH += $$PROCESS_SRC
DISTFILES += $$PROCESS_PRI
include($$PROCESS_PRI)

