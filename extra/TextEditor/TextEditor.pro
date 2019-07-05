#-------------------------------------------------
#
# Project created by QtCreator 2016-03-14T22:31:35
#
#-------------------------------------------------

QT       += core gui widgets

CONFIG += c++11
TARGET = TextEditor
TEMPLATE = app

unix:QMAKE_CXXFLAGS += -Wall -Wextra -Werror=return-type -Werror=trigraphs -Wno-switch -Wno-reorder

INCLUDEPATH += $$PWD/src
INCLUDEPATH += $$PWD/../../src/texteditor
INCLUDEPATH += $$PWD/../../src

linux:LIBS += -lncursesw
macx:LIBS += -lncurses

SOURCES += \
	$$PWD/../../src/common/joinpath.cpp \
	$$PWD/../../src/common/misc.cpp \
	$$PWD/../../src/texteditor/AbstractCharacterBasedApplication.cpp \
	$$PWD/../../src/texteditor/InputMethodPopup.cpp \
	$$PWD/../../src/texteditor/TextEditorTheme.cpp \
	$$PWD/../../src/texteditor/TextEditorWidget.cpp \
	$$PWD/../../src/texteditor/UnicodeWidth.cpp \
	$$PWD/../../src/texteditor/unicode.cpp \
	src/MainWindow.cpp \
	src/MySettings.cpp \
	src/main.cpp\
	src/cmain.cpp

HEADERS  += \
	$$PWD/../../src/common/joinpath.h \
	$$PWD/../../src/common/misc.h \
	$$PWD/../../src/texteditor/AbstractCharacterBasedApplication.h \
	$$PWD/../../src/texteditor/InputMethodPopup.h \
	$$PWD/../../src/texteditor/TextEditorTheme.h \
	$$PWD/../../src/texteditor/TextEditorWidget.h \
	$$PWD/../../src/texteditor/UnicodeWidth.h \
	$$PWD/../../src/texteditor/unicode.h \
	src/MainWindow.h \
	src/MySettings.h \
	src/cmain.h

FORMS    += \
	src/MainWindow.ui

