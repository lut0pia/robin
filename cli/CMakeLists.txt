cmake_minimum_required(VERSION 3.1)

project(rbncli LANGUAGES C)

file(GLOB SOURCES "*.c")
if(WIN32)
  list(FILTER SOURCES EXCLUDE REGEX ".*_unix\.c")
else()
  list(FILTER SOURCES EXCLUDE REGEX ".*_win\.c")
  link_libraries(m dl pthread)
endif()

add_executable(rbncli ${SOURCES})