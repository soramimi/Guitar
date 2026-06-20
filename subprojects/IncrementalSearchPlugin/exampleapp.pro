QT += core
TEMPLATE = app
TARGET = exampleapp

DESTDIR = $$PWD/../../_bin

# CONFIG += use_cudachi
use_cudachi {
	DEFINES += USE_SUDACHI
	CONFIG(debug,debug|release):IBS += -L/home/soramimi/develop/sudachi-example/sudachi-capi/target/release -lsudachi_capi
	CONFIG(release,debug|release):IBS += -L/home/soramimi/develop/sudachi-example/sudachi-capi/target/debug -lsudachi_capi
}


SOURCES += exampleapp/main.cpp
