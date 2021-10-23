cmake_minimum_required(VERSION 3.1)

project(rbncli LANGUAGES C)

file(GLOB SOURCES "*.c" "*.h" "../*.h")
list(FILTER SOURCES EXCLUDE REGEX "/export.c") # Generated file
if(WIN32)
  list(FILTER SOURCES EXCLUDE REGEX ".*_unix\.c")
  link_libraries(winmm dsound)
else()
  list(FILTER SOURCES EXCLUDE REGEX ".*_win\.c")
  link_libraries(m dl pthread)
endif()

# Warnings and errors
if(MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  add_compile_options(/WX)
else()
  add_compile_options(-Werror)
endif()

add_executable(rbncli ${SOURCES})

set_target_properties(
  rbncli PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
