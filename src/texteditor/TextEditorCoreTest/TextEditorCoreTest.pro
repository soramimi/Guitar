QT += core gui widgets

TEMPLATE = app
CONFIG += console c++20
CONFIG -= app_bundle

LIBS += -lgtest -lpthread

INCLUDEPATH += ..
INCLUDEPATH += ../../

SOURCES += \
	../AbstractTextEditorApplication.cpp \
	../EscapeSequence.cpp \
	../PatternState.cpp \
	../TerminalScreen.cpp \
	../UnicodeWidth.cpp \
	../WrapAlgorithm.cpp \
	../unicode.cpp \
	main.cpp

HEADERS += \
	../AbstractTextEditorApplication.h \
	../EscapeSequence.h \
	../LineIndexMap/LineIndexMap.h \
	../PatternState.h \
	../TerminalScreen.h \
	../UnicodeWidth.h \
	../WrapAlgorithm.h \
	../unicode.h

contains(CONFIG, thread_sanitizer) {
	QMAKE_CXXFLAGS += -fsanitize=thread -fno-omit-frame-pointer
	QMAKE_LFLAGS += -fsanitize=thread
}
