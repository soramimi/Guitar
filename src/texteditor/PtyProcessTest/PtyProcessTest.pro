TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

LIBS += -lgtest -lgtest_main -lpthread

PROCESS_SRC = $$PWD/../../process/src
PROCESS_PRI = $$PWD/../../process/ptyprocess.pri
INCLUDEPATH += $$PROCESS_SRC
include($$PROCESS_PRI)

SOURCES += main.cpp
