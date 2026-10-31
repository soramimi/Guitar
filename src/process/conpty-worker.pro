TEMPLATE = app
TARGET = conpty-worker
CONFIG -= qt
CONFIG += console c++17
DESTDIR = $$PWD/_bin
!win32:error(conpty-worker requires Windows)
DEFINES += NOMINMAX
INCLUDEPATH += $$PWD/src
SOURCES += src/ConPtyWorkerMain.cpp \
	src/ProcessWinConPtyWithWorker.cpp \
	src/BasicProcessWinConPty.cpp \
	src/BasicProcessWin.cpp \
	src/AbstractProcess.cpp \
	src/ProcessHelper.cpp
HEADERS += src/ProcessWinConPtyWithWorker.h \
	src/ConPtyWorkerProtocol.h \
	src/BasicProcessWinConPty.h \
	src/BasicProcessWin.h \
	src/AbstractProcess.h \
	src/ProcessHelper.h \
	src/ProcessWinHelper.h
