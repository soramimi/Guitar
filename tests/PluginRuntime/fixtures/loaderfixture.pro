TEMPLATE = lib
TARGET = loaderfixture
CONFIG += plugin c++20
QT = core
DESTDIR = $$PWD/../../../lib/soramimi.jp
SOURCES = loaderfixture.cpp
HEADERS = Service.h
CONFIG(debug, debug|release): TARGET = loaderfixtured
