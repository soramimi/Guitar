
SOURCES += $$PROCESS_SRC/AbstractProcess.cpp
SOURCES += $$PROCESS_SRC/ProcessHelper.cpp

!win32:SOURCES += $$PROCESS_SRC/BasicProcessPosix.cpp

win32 {
	SOURCES += \
		$$PROCESS_SRC/BasicProcessWin.cpp \
		$$PROCESS_SRC/BasicProcessWinConPty.cpp \
		$$PROCESS_SRC/ProcessWinConPtyWithWorker.cpp \
		$$PROCESS_SRC/ProcessWin.cpp \
		$$PROCESS_SRC/ProcessWinConPty.cpp \
		$$PROCESS_SRC/ProcessWinPty.cpp
}

HEADERS += $$PROCESS_SRC/AbstractProcess.h
HEADERS += $$PROCESS_SRC/ProcessHelper.h

!win32:HEADERS += $$PROCESS_SRC/BasicProcessPosix.h

win32 {
	HEADERS += \
		$$PROCESS_SRC/BasicProcessWin.h \
		$$PROCESS_SRC/BasicProcessWinConPty.h \
		$$PROCESS_SRC/ProcessWinConPtyWithWorker.h \
		$$PROCESS_SRC/ProcessWin.h \
		$$PROCESS_SRC/ProcessWinConPty.h \
		$$PROCESS_SRC/ProcessWinHelper.h \
		$$PROCESS_SRC/ProcessWinPty.h
}
