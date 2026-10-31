TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

LIBS += -lgtest -lgtest_main -lpthread

INCLUDEPATH += ..

SOURCES += \
	../TerminalScreen.cpp \
	../UnicodeWidth.cpp \
	main.cpp

HEADERS += \
	../TerminalScreen.h \
	../UnicodeWidth.h
