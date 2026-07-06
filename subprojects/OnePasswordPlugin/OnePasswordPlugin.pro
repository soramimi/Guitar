include(../../common.pri)

# DESTDIR = $$PWD/_bin
!win32:DESTDIR = $$PWD/../../lib
win32:DESTDIR = $$PWD/../../_bin

CONFIG(release,debug|release):TARGET = onepasswordplugin
CONFIG(debug,debug|release):TARGET = onepasswordplugind

TEMPLATE = lib
CONFIG += plugin
QT = core

INCLUDEPATH += $$PWD/libapikey

LIBS += $$PWD/_bin/libapikey.so

unix:LIBS += -ldl
win32:LIBS += -lole32 -loleaut32

HEADERS += \
	src/OnePassword.h \
	src/OnePasswordInterface.h \
	src/OnePasswordPlugin.h
SOURCES += \
	src/OnePassword.cpp \
	src/OnePasswordInterface.cpp \
	src/OnePasswordPlugin.cpp

DISTFILES += \
	onepasswordplugin.json
