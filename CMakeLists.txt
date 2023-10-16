cmake_minimum_required(VERSION 3.2)

project(Guitar
	VERSION 1.0.0
	LANGUAGES CXX C
)
set(CMAKE_CXX_EXTENSIONS  OFF)
set(CMAKE_CXX_STANDARD    17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Disable in-source-build
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES  ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

option(BUILD_WITH_QT5 "Build with Qt 5" OFF)
option(BUILD_WITH_QT6 "Build with Qt 6" OFF)

if(BUILD_WITH_QT6)
	set(QT_VERSION_MAJOR 6)
elseif(BUILD_WITH_QT5)
	set(QT_VERSION_MAJOR 5)
endif()

if(NOT QT_VERSION_MAJOR)
	message(STATUS "QT_VERSION_MAJOR, BUILD_WITH_QT5 or BUILD_WITH_QT6 not set, detecting Qt version...")
	find_package(QT NAMES Qt6 Qt5 COMPONENTS Core REQUIRED)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
	set(QT_MIN_VERSION 6.0)
elseif(QT_VERSION_MAJOR EQUAL 5)
	set(QT_MIN_VERSION 5.0)
else()
	message(FATAL_ERROR "Invalid QT_VERSION_MAJOR.")
endif()

set(QT_DEFAULT_MAJOR_VERSION ${QT_VERSION_MAJOR})

set(QT_COMPONENTS Core Gui Widgets Network Svg LinguistTools)
if(QT_VERSION_MAJOR EQUAL 6)
	list(APPEND QT_COMPONENTS Core5Compat)
endif()
find_package(Qt${QT_VERSION_MAJOR} ${QT_MIN_VERSION} COMPONENTS ${QT_COMPONENTS} REQUIRED)

if(QT_VERSION_MAJOR EQUAL 5 AND Qt5Core_VERSION VERSION_LESS 5.15.0)
	macro(qt_add_translation)
		qt5_add_translation(${ARGN})
	endmacro()
endif()

find_package(zlib REQUIRED)
find_package(OpenSSL REQUIRED)
if(WIN32)
	# check package at
	# https://github.com/rprichard/winpty
	find_package(winpty REQUIRED )
endif()

find_library(MAGIC_LIBRARIES NAMES magic libmagic HINTS /usr/lib /usr/lib64 /usr/local/lib /usr/local/lib64 REQUIRED)
find_path(MAGIC_INCLUDE_DIRS NAMES magic.h PATHS /usr/include /usr/local/include REQUIRED)

# extract version information
string(TIMESTAMP Guitar_copyright_year "%Y")
execute_process(
	COMMAND git rev-parse --short=7 HEAD
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	OUTPUT_VARIABLE Guitar_git_hash
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

configure_file(version.h.in version.h)
configure_file(win.rc.in win.rc.h)
configure_file(Info.plist.in Info.plist)


add_subdirectory(src)
#add_subdirectory(extra)
