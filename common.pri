QMAKE_PROJECT_DEPTH = 0

CPP_STD = c++20
CONFIG += $$CPP_STD
gcc:QMAKE_CXXFLAGS += -std=$$CPP_STD -Wall -Wextra

msvc:DEFINES += NOMINMAX

msvc:INCLUDEPATH += C:/vcpkg/installed/x64-windows/include
msvc:LIBS += -LC:/vcpkg/installed/x64-windows/lib

gcc:QMAKE_CXXFLAGS += -include $$PWD/pragma.h
msvc:QMAKE_CXXFLAGS += /FI $$PWD/pragma.h
HEADERS += $$PWD/pragma.h
