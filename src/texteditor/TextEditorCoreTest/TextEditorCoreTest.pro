QT += core gui widgets

TEMPLATE = app
CONFIG += console c++20
CONFIG -= app_bundle

LIBS += -lgtest -lgtest_main -lpthread

INCLUDEPATH += ..

SOURCES += \
	../EscapeSequence.cpp \
	main.cpp

HEADERS += \
	../AbstractTextEditorApplication.h \
	../EscapeSequence.h \
	../LineIndexMap/LineIndexMap.h
