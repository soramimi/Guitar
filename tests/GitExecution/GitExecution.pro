# Linux integration tests for the real application Git/session/process code.
include(../../Guitar.pro)
TARGET = GitExecutionTest
DESTDIR = $$OUT_PWD
LIBS += -lgtest
INCLUDEPATH += $$PWD/../..
reuse_app_objects {
    # Run after a matching release Guitar build; retain all app link dependencies.
    appObjects = $$files($$PWD/../../_build/*.o)
    appObjects -= $$PWD/../../_build/main.o $$PWD/../../_build/git_execution_tests.o
    OBJECTS += $$appObjects
    SOURCES = $$PWD/git_execution_tests.cpp
    HEADERS =
    FORMS =
    RESOURCES =
} else {
    SOURCES -= $$SRC/main.cpp
    SOURCES += $$PWD/git_execution_tests.cpp
    units = EscapeSequence PatternState TerminalScreen WrapAlgorithm
    for(unit, units) {
        SOURCES -= src/texteditor/$${unit}.cpp
        HEADERS -= src/texteditor/$${unit}.h
        SOURCES += $$SRC/texteditor/$${unit}.cpp
        HEADERS += $$SRC/texteditor/$${unit}.h
    }
}
SOURCES += $$PWD/model_settings_tests.cpp

SOURCES += $$PWD/pack_tests.cpp
