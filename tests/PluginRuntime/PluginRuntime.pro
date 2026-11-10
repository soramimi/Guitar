TEMPLATE = app
TARGET = PluginRuntimeTest
QT = core
CONFIG += console c++20
CONFIG -= app_bundle
LIBS += -lgtest -lpthread -lzstd
SOURCES += main.cpp ../../subprojects/FileTypePlugin/src/zs.cpp ../../subprojects/IncrementalSearchPlugin/src/romaji.cpp
sanitize {
    QMAKE_CXXFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
    QMAKE_LFLAGS += -fsanitize=address,undefined
}
# Link the plugin types so UBSan can check their RTTI as well as virtual calls.
LIBS += -L$$PWD/../../lib/soramimi.jp -lfiletypeplugin -lincrementalsearchplugin
QMAKE_RPATHDIR += $$PWD/../../lib/soramimi.jp
DEFINES += NO_LOGGER
INCLUDEPATH += $$PWD/../../src $$PWD/../../src/common
