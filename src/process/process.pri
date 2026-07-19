
SOURCES += $$PROCESS_SRC/AbstractProcess.cpp
SOURCES += $$PROCESS_SRC/ProcessHelper.cpp

!win32:SOURCES += $$PROCESS_SRC/BasicProcessPosix.cpp

win32 {
	SOURCES += \
		$$PROCESS_SRC/BasicProcessWin.cpp \
		$$PROCESS_SRC/ProcessWin.cpp \
		$$PROCESS_SRC/BasicProcessWinConPTY.cpp \
		$$PROCESS_SRC/ProcessConPtyWithWorker.cpp \
		$$PROCESS_SRC/ProcessWinConPty.cpp \
		$$PROCESS_SRC/ProcessWinPty.cpp
}

HEADERS += $$PROCESS_SRC/AbstractProcess.h
HEADERS += $$PROCESS_SRC/ProcessHelper.h

!win32:HEADERS += $$PROCESS_SRC/BasicProcessPosix.h

win32 {
	HEADERS += \
		$$PROCESS_SRC/BasicProcessWin.h \
		$$PROCESS_SRC/ProcessWin.h \
		$$PROCESS_SRC/BasicProcessWinConPTY.h \
		$$PROCESS_SRC/ProcessConPtyWithWorker.h \
		$$PROCESS_SRC/ProcessWinConPty.h \
		$$PROCESS_SRC/ProcessWinHelper.h \
		$$PROCESS_SRC/ProcessWinPty.h
}
