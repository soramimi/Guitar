#-------------------------------------------------
#
# Project created by QtCreator 2017-12-04T22:48:45
#
#-------------------------------------------------

QT       += core gui widgets svg

TARGET = FileView
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += $$PWD/../../src/
INCLUDEPATH += $$PWD/../../src/texteditor


SOURCES += \
        main.cpp \
        MainWindow.cpp \
    ../../src/texteditor/AbstractCharacterBasedApplication.cpp \
    ../../src/texteditor/InputMethodPopup.cpp \
    ../../src/texteditor/TextEditorTheme.cpp \
    ../../src/texteditor/TextEditorWidget.cpp \
    ../../src/texteditor/unicode.cpp \
    ../../src/texteditor/UnicodeWidth.cpp \
    ../../src/FileViewWidget.cpp \
    ../../src/common/joinpath.cpp \
    ../../src/common/misc.cpp \
    ../../src/ImageViewWidget.cpp \
    ../../src/MemoryReader.cpp \
    ../../src/Photoshop.cpp \
    ../../src/ImageViewWidget.cpp

HEADERS += \
        MainWindow.h \
    ../../src/texteditor/AbstractCharacterBasedApplication.h \
    ../../src/texteditor/InputMethodPopup.h \
    ../../src/texteditor/TextEditorTheme.h \
    ../../src/texteditor/TextEditorWidget.h \
    ../../src/texteditor/unicode.h \
    ../../src/texteditor/UnicodeWidth.h \
    ../../src/FileViewWidget.h \
    ../../src/common/joinpath.h \
    ../../src/common/misc.h \
    ../../src/ImageViewWidget.h \
    ../../src/MemoryReader.h \
    ../../src/Photoshop.h \
    ../../src/ImageViewWidget.h

FORMS += \
		MainWindow.ui
