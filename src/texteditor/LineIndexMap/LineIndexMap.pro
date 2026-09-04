TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

LIBS += -lgtest -lgtest_main -lpthread

SOURCES += \
        main.cpp

HEADERS += \
        somethingmap.h

DISTFILES += \
	AGENTS.md
