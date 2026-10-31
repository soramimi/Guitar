QT += core gui widgets

TEMPLATE = app
CONFIG += console c++20
CONFIG -= app_bundle

LIBS += -lgtest -lpthread

INCLUDEPATH += ..
INCLUDEPATH += ../../

PROCESS_SRC = $$PWD/../../process/src
PROCESS_PRI = $$PWD/../../process/ptyprocess.pri
INCLUDEPATH += $$PROCESS_SRC
include($$PROCESS_PRI)

SOURCES += \
	../../common/qmisc.cpp \
	../AbstractTextEditorApplication.cpp \
	../EscapeSequence.cpp \
	../PatternState.cpp \
	../TerminalScreen.cpp \
	../TextEditorTheme.cpp \
	../TextEditorView.cpp \
	../UnicodeWidth.cpp \
	../WrapAlgorithm.cpp \
	../unicode.cpp \
	../TextEditorApp/src/PtyProcessController.cpp \
	../TextEditorApp/src/PtyProcessFactory.cpp \
	main.cpp

HEADERS += \
	../AbstractTextEditorApplication.h \
	../EscapeSequence.h \
	../LineIndexMap/LineIndexMap.h \
	../PatternState.h \
	../TerminalScreen.h \
	../TextEditorTheme.h \
	../TextEditorView.h \
	../UnicodeWidth.h \
	../WrapAlgorithm.h \
	../unicode.h \
	../TextEditorApp/src/PtyProcessController.h \
	../TextEditorApp/src/PtyProcessFactory.h

contains(CONFIG, thread_sanitizer) {
	QMAKE_CXXFLAGS += -fsanitize=thread -fno-omit-frame-pointer
	QMAKE_LFLAGS += -fsanitize=thread
}
