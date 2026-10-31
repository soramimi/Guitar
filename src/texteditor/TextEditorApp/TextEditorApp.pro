
QT       += core gui widgets
greaterThan(QT_MAJOR_VERSION, 5) {
    QT += core5compat
}

CONFIG += c++20
TARGET = TextEditorApp
TEMPLATE = app

DESTDIR = $$PWD/_bin

DEFINES += NOMINMAX

unix:QMAKE_CXXFLAGS += -Wall -Wextra -Werror=return-type -Werror=trigraphs -Wno-switch -Wno-reorder

INCLUDEPATH += $$PWD/../../
INCLUDEPATH += $$PWD/../../texteditor
INCLUDEPATH += $$PWD/../../common

PROCESS_SRC = $$PWD/../../process/src
PROCESS_PRI = $$PWD/../../process/ptyprocess.pri
INCLUDEPATH += $$PROCESS_SRC
DISTFILES += $$PROCESS_PRI
include($$PROCESS_PRI)

win32:LIBS += -lole32

win32 {
	# Build the matching worker beside the application before linking it.
	WORKER_BUILD = $$OUT_PWD/conpty-worker-build
	WORKER_PROJECT = $$PWD/../../process/conpty-worker.pro
	worker.target = $$DESTDIR/conpty-worker.exe
	worker.depends = $$WORKER_PROJECT $$files($$PROCESS_SRC/*.cpp) $$files($$PROCESS_SRC/*.h)
	worker.commands = if not exist $$shell_quote($$shell_path($$WORKER_BUILD)) mkdir $$shell_quote($$shell_path($$WORKER_BUILD))
	worker.commands += $$escape_expand(\n\t) cd /d $$shell_quote($$shell_path($$WORKER_BUILD)) && $$shell_quote($$shell_path($$QMAKE_QMAKE)) $$shell_quote($$shell_path($$WORKER_PROJECT)) -after $$shell_quote(DESTDIR=$$shell_path($$DESTDIR)) && $(MAKE)
	QMAKE_EXTRA_TARGETS += worker
	PRE_TARGETDEPS += $$worker.target
}

SOURCES += \
	../../common/misc.cpp \
	../../common/q/Dir.cpp \
	../../common/q/FileInfo.cpp \
	../../common/qmisc.cpp \
	../../common/realpath.cpp \
	../../common/unicode_conversion.cpp \
	../../common/wstring.cpp \
	../EscapeSequence.cpp \
	../PatternState.cpp \
	../TerminalScreen.cpp \
	../TextEditorTheme.cpp \
	../TextEditorView.cpp \
	../TextEditorWidget.cpp \
	../UnicodeWidth.cpp \
	../WrapAlgorithm.cpp \
	../unicode.cpp \
	../AbstractTextEditorApplication.cpp \
	src/MainWindow.cpp \
	src/PtyProcessController.cpp \
	src/PtyProcessFactory.cpp \
	src/MySettings.cpp \
	src/main.cpp

HEADERS  += \
	../../common/joinpath.h \
	../../common/misc.h \
	../../common/q/Dir.h \
	../../common/q/FileInfo.h \
	../../common/qmisc.h \
	../../common/realpath.h \
	../../common/unicode_conversion.h \
	../../common/wstring.h \
	../EscapeSequence.h \
	../LineIndexMap/LineIndexMap.h \
	../PatternState.h \
	../TerminalScreen.h \
	../TextEditorTheme.h \
	../TextEditorView.h \
	../TextEditorWidget.h \
	../UnicodeWidth.h \
	../WrapAlgorithm.h \
	../unicode.h \
	../AbstractTextEditorApplication.h \
	src/MainWindow.h \
	src/PtyProcessController.h \
	src/PtyProcessFactory.h \
	src/MySettings.h

FORMS    += \
	src/MainWindow.ui

DISTFILES += \
	../LineIndexMap/AGENTS.md \
	../AGENTS.md

