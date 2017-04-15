QT += core gui widgets

CONFIG += c++11

TARGET = askpass
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += askpass/main.cpp \
	askpass/AskPassDialog.cpp

FORMS += \
	askpass/AskPassDialog.ui

HEADERS += \
	askpass/AskPassDialog.h
