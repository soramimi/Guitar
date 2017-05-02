QT += core gui widgets

CONFIG += c++11

TARGET = askpass
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

unix:QMAKE_CXXFLAGS += -Wall -Wextra -Werror=return-type -Werror=trigraphs -Wno-switch -Wreorder -Wno-reorder
linux:QMAKE_RPATHDIR += $ORIGIN
macx:QMAKE_RPATHDIR += @executable_path/../Frameworks

SOURCES += askpass/main.cpp \
	askpass/AskPassDialog.cpp

FORMS += \
	askpass/AskPassDialog.ui

HEADERS += \
	askpass/AskPassDialog.h
