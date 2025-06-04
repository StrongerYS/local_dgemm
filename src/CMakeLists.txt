set(SRC_FILES)
foreach(ext IN LISTS SRC_FILE_EXTENSIONS)
    file(GLOB_RECURSE tmp_files "./${ext}")
    list(APPEND SRC_FILES ${tmp_files})
endforeach()
message(STATUS "SRC_FILES: ${SRC_FILES}")

add_library(${project_lib_name}
    ${SRC_FILES}
)
target_include_directories(${project_lib_name}
  PUBLIC ${CMAKE_SOURCE_DIR}/include
  PRIVATE ${CMAKE_SOURCE_DIR}/include/internal
)