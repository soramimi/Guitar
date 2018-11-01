message("Searching for zlib...")

message("zlib_DIR: ${zlib_DIR}")
set(zlib_ROOT_DIR "${zlib_DIR}")
message("zlib_ROOT_DIR: ${zlib_ROOT_DIR}")

find_path(zlib_INCLUDE_DIRS
    NAMES zlib.h
    HINTS "${zlib_ROOT_DIR}/include"
    DOC "The zlib include directory"
)

find_library(zlib_LIBRARIES 
    NAMES z
    HINTS "${zlib_ROOT_DIR}/lib"
    DOC "The zlib library"
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(zlib  DEFAULT_MSG
                                  zlib_LIBRARIES zlib_INCLUDE_DIRS)

mark_as_advanced(zlib_INCLUDE_DIRS zlib_LIBRARIES )

if(zlib_FOUND)
  message("zlib found")
  add_library(zlib SHARED IMPORTED)
  set_target_properties(zlib PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${zlib_INCLUDE_DIRS}"
  )

	set_property(TARGET zlib APPEND PROPERTY IMPORTED_LOCATION "${zlib_LIBRARIES}")
endif()
