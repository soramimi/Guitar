TEMPLATE = app
TARGET = WorkerProcessTest
CONFIG -= qt
CONFIG += console c++17
!win32:error(WorkerProcessTest requires Windows)
DESTDIR = $$PWD/../_bin
DEFINES += NOMINMAX
INCLUDEPATH += $$PWD/../src
SOURCES += $$PWD/main.cpp \
	$$PWD/../src/ProcessWinConPtyWithWorker.cpp \
	$$PWD/../src/BasicProcessWinConPty.cpp \
	$$PWD/../src/BasicProcessWin.cpp \
	$$PWD/../src/AbstractProcess.cpp \
	$$PWD/../src/ProcessHelper.cpp
