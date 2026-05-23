QMAKE_PROJECT_DEPTH = 0
CPP_STD = c++17
gcc:QMAKE_CXXFLAGS += -std=$$CPP_STD -Wall -Wextra -Werror=return-type -Werror=trigraphs -Wno-switch -Wno-reorder -Wno-unused-parameter -Wno-unused-parameter
msvc:DEFINES += NOMINMAX
msvc:INCLUDEPATH += C:/vcpkg/installed/x64-windows/include
msvc:LIBS += -LC:/vcpkg/installed/x64-windows/lib
