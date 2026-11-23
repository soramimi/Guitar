include(../../../common.pri)

QT = core testlib
CONFIG += testcase console c++17
CONFIG -= app_bundle
TARGET = IncrementalSearchTests
DESTDIR = $$PWD/../../../_bin
INCLUDEPATH += ../src
SOURCES += reading_search_tests.cpp
