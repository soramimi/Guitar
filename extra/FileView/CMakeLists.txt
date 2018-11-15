cmake_minimum_required(VERSION 3.2)

project(FileView
    VERSION 0.9.0
    LANGUAGES CXX C
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 COMPONENTS Core Widgets Network Svg REQUIRED)

set(FileView_SOURCES
    main.cpp
    MainWindow.cpp
    ../../src/texteditor/AbstractCharacterBasedApplication.cpp
    ../../src/texteditor/InputMethodPopup.cpp
    ../../src/texteditor/TextEditorTheme.cpp
    ../../src/texteditor/TextEditorWidget.cpp
    ../../src/texteditor/unicode.cpp
    ../../src/texteditor/UnicodeWidth.cpp
    ../../src/FileViewWidget.cpp
    ../../src/common/joinpath.cpp
    ../../src/common/misc.cpp
    ../../src/ImageViewWidget.cpp
    ../../src/MemoryReader.cpp
    ../../src/Photoshop.cpp
    ../../src/ImageViewWidget.cpp
    )

set(FileView_HEADERS
    MainWindow.h
    ../../src/texteditor/AbstractCharacterBasedApplication.h
    ../../src/texteditor/InputMethodPopup.h
    ../../src/texteditor/TextEditorTheme.h
    ../../src/texteditor/TextEditorWidget.h
    ../../src/texteditor/unicode.h
    ../../src/texteditor/UnicodeWidth.h
    ../../src/FileViewWidget.h
    ../../src/common/joinpath.h
    ../../src/common/misc.h
    ../../src/ImageViewWidget.h
    ../../src/MemoryReader.h
    ../../src/Photoshop.h
    ../../src/ImageViewWidget.h
    )

set(FileView_UIS
    MainWindow.ui
    )

add_executable(${PROJECT_NAME}
    ${FileView_SOURCES}
    ${FileView_HEADERS}
    ${FileView_UIS}
    )

target_link_libraries(${PROJECT_NAME}
    Qt5::Widgets
    Qt5::Network
    Qt5::Svg
)
