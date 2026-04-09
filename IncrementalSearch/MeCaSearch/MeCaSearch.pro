QT       += core gui widgets

CONFIG += c++17

LIBS += $$PWD/../liblibmecasearch.a

SOURCES += \
    main.cpp \
    MainWindow.cpp

HEADERS += \
    MainWindow.h

FORMS += \
    MainWindow.ui

