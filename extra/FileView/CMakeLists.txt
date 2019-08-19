cmake_minimum_required(VERSION 3.2)

project(FileView
    VERSION 0.9.0
    LANGUAGES CXX C
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 再帰的にSourceを取得...としたいけど不要なものをとりすぎてしまうので限定的
file(GLOB_RECURSE FileView_SOURCES ${CMAKE_SOURCE_DIR}/src/texteditor/*.cpp)
file(GLOB_RECURSE common ${CMAKE_SOURCE_DIR}/src/common/*.cpp)
list(APPEND FileView_SOURCES
	${common}
	${CMAKE_SOURCE_DIR}/src/FileViewWidget.cpp
	${CMAKE_SOURCE_DIR}/src/ImageViewWidget.cpp
	${CMAKE_SOURCE_DIR}/src/MemoryReader.cpp
	${CMAKE_SOURCE_DIR}/src/Photoshop.cpp)

# MainWindow.cppをSWAP
list(APPEND FileView_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.cpp)
list(REMOVE_ITEM FileView_SOURCES ${CMAKE_SOURCE_DIR}/src/MainWindow.cpp)
list(REMOVE_ITEM FileView_SOURCES ${CMAKE_SOURCE_DIR}/src/main.cpp)

# main.cpp以外のSourceをコンパイル
add_library(fileview-lib ${FileView_SOURCES})
target_link_libraries(fileview-lib PUBLIC
	Qt5::Widgets
	Qt5::Network
	Qt5::Svg)
target_include_directories(fileview-lib PUBLIC ${CMAKE_BINARY_DIR})
target_include_directories(fileview-lib PUBLIC ${PROJECT_SOURCE_DIR})
target_include_directories(fileview-lib PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(fileview-lib PUBLIC ${CMAKE_SOURCE_DIR}/src/texteditor)
target_include_directories(fileview-lib PUBLIC ${CMAKE_SOURCE_DIR}/src/common)

set_target_properties(fileview-lib PROPERTIES OUTPUT_NAME fileview)


file(GLOB_RECURSE FileView_UIS ./*.ui)

# main.cppをコンパイル
add_executable(${PROJECT_NAME}
	main.cpp
	${FileView_UIS})


# 結合
target_link_libraries(${PROJECT_NAME} fileview-lib)
