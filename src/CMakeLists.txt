file(GLOB_RECURSE Guitar_SOURCES  ./*.cpp)
file(GLOB Guitar_HEADERS  ./*.h)


if(WIN32)
# win32環境においてunix関係のアイテムを使用しないように
	file(GLOB uinx_src ./unix/*.cpp)
	file(GLOB unix_h ./unix/*.h)
	list(REMOVE_ITEM Guitar_SOURCES ${uinx_src})
	list(REMOVE_ITEM Guitar_HEADERS ${uinx_h})
elseif(UNIX)
# unix環境においてwin32関係のアイテムを使用しないように
	file(GLOB_RECURSE win32_src ./win32/*.cpp)
	file(GLOB_RECURSE win32_h ./win32/*.h)
	list(REMOVE_ITEM Guitar_SOURCES ${win32_src})
	list(REMOVE_ITEM Guitar_HEADERS ${win32_src})
endif()

file(GLOB_RECURSE Guitar_UIS  ./*.ui) 

file(GLOB_RECURSE Guitar_RESOURCES   ./resources/*.qrc)

file(GLOB_RECURSE Guitar_TRANSLATIONS  ./resources/translations/*.ts)
qt5_add_translation(Guitar_QM_FILES ${Guitar_TRANSLATIONS})

configure_file(${Guitar_RESOURCES} ${CMAKE_BINARY_DIR} COPYONLY)

add_custom_target(translations ALL DEPENDS ${Guitar_QM_FILES})

add_custom_target(resources ALL DEPENDS ${Guitar_RESOURCES})

add_dependencies(resources translations)

include_directories(${CMAKE_BINARY_DIR})
include_directories(./texteditor)
add_executable(${PROJECT_NAME}
	${Guitar_SOURCES}
	${Guitar_HEADERS}
	${Guitar_UIS}
	${Guitar_RESOURCES}
	${Guitar_QM_FILES}
)

target_link_libraries(${PROJECT_NAME}
	Qt5::Widgets
	Qt5::Network
	Qt5::Svg
	zlib
	OpenSSL::SSL OpenSSL::Crypto
	)
