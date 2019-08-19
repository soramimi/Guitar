cmake_minimum_required(VERSION 3.2)

project(Guitar
	VERSION 1.0.0
	LANGUAGES CXX C
)
set(CMAKE_CXX_EXTENSIONS  OFF)
set(CMAKE_CXX_STANDARD    11)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Disable in-source-build
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES  ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

find_package(Qt5 COMPONENTS Core Widgets Network Svg REQUIRED)
find_package(zlib REQUIRED )
find_package(OpenSSL REQUIRED )
find_package(Qt5LinguistTools REQUIRED)
if(WIN32)
	# check package at
	# https://github.com/rprichard/winpty
	find_package(winpty REQUIRED )
endif()

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
