add_definitions(-DAPP_GUITAR)

# Sourceを再帰的に取得
file(GLOB_RECURSE Guitar_SOURCES  ./*.cpp ../filetype/*.cpp)

if(WIN32)
# win32環境においてunix関係のアイテムを使用しないように
	file(GLOB uinx_src ./unix/*.cpp)
	list(REMOVE_ITEM Guitar_SOURCES ${uinx_src})
elseif(UNIX)
# unix環境においてwin32関係のアイテムを使用しないように
	file(GLOB_RECURSE win32_src ./win32/*.cpp)
	list(REMOVE_ITEM Guitar_SOURCES ${win32_src})
endif()

# main.cppは別個にコンパイルしたいのでリストから除外
list(REMOVE_ITEM Guitar_SOURCES main.cpp)

file(GLOB_RECURSE Guitar_UIS  ./*.ui)

file(GLOB_RECURSE Guitar_RESOURCES   ./resources/*.qrc)

file(GLOB_RECURSE Guitar_TRANSLATIONS  ./resources/translations/*.ts)
qt_add_translation(Guitar_QM_FILES ${Guitar_TRANSLATIONS})

configure_file(${Guitar_RESOURCES} ${CMAKE_BINARY_DIR} COPYONLY)

add_custom_target(translations ALL DEPENDS ${Guitar_QM_FILES})
add_custom_target(resources ALL DEPENDS ${Guitar_RESOURCES})
add_dependencies(resources translations)

# Sourceをライブラリとして構築する
add_library(guitar-lib ${Guitar_SOURCES})
target_link_libraries(guitar-lib PUBLIC
	Qt${QT_VERSION_MAJOR}::Core
	Qt${QT_VERSION_MAJOR}::Network
	Qt${QT_VERSION_MAJOR}::Gui
	Qt${QT_VERSION_MAJOR}::Widgets
	Qt${QT_VERSION_MAJOR}::Svg
	zlib 
	OpenSSL::SSL
	OpenSSL::Crypto
	${MAGIC_LIBRARIES}
	)

if(QT_VERSION_MAJOR EQUAL 6)
	target_link_libraries(guitar-lib PUBLIC Qt${QT_VERSION_MAJOR}::Core5Compat)
endif()

# ライブラリに読み込むHeaderの設定
target_include_directories(guitar-lib PUBLIC ${CMAKE_BINARY_DIR})
target_include_directories(guitar-lib PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_include_directories(guitar-lib PUBLIC ${PROJECT_SOURCE_DIR}/src/texteditor)
target_include_directories(guitar-lib PUBLIC ${PROJECT_SOURCE_DIR}/src/coloredit)

set_target_properties(guitar-lib PROPERTIES OUTPUT_NAME guitar)

# main.cppのコンパイル
add_executable(${PROJECT_NAME} 
		main.cpp
		${Guitar_RESOURCES}
		${Guitar_QM_FILES}
		${Guitar_UIS}
		)

# main.cppとそれ以外をリンク
target_link_libraries(${PROJECT_NAME} guitar-lib)
