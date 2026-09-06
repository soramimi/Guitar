
QT       += core gui widgets
greaterThan(QT_MAJOR_VERSION, 5) {
    QT += core5compat
}

CONFIG += c++17
TARGET = TextEditorApp
TEMPLATE = app

DESTDIR = $$PWD/_bin

unix:QMAKE_CXXFLAGS += -Wall -Wextra -Werror=return-type -Werror=trigraphs -Wno-switch -Wno-reorder

INCLUDEPATH += $$PWD/../../
INCLUDEPATH += $$PWD/../../texteditor

win32:LIBS += -lole32

SOURCES += \
	../../common/misc.cpp \
	../../common/q/Dir.cpp \
	../../common/q/FileInfo.cpp \
	../../common/qmisc.cpp \
	../../common/realpath.cpp \
	../../common/unicode_conversion.cpp \
	../../common/wstring.cpp \
	../TextEditorTheme.cpp \
	../TextEditorView.cpp \
	../TextEditorWidget.cpp \
	../UnicodeWidth.cpp \
	../unicode.cpp \
	../AbstractTextEditorApplication.cpp \
	src/MainWindow.cpp \
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
	../LineIndexMap/LineIndexMap.h \
	../TextEditorTheme.h \
	../TextEditorView.h \
	../TextEditorWidget.h \
	../UnicodeWidth.h \
	../unicode.h \
	../AbstractTextEditorApplication.h \
	src/MainWindow.h \
	src/MySettings.h

FORMS    += \
	src/MainWindow.ui

DISTFILES += \
	../LineIndexMap/AGENTS.md \
	../AGENTS.md

