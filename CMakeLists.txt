cmake_minimum_required(VERSION 3.30)
project(toyscheme LANGUAGES CXX)

add_executable(toyscheme)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD ON)

file(GLOB_RECURSE toyscheme_SOURCE_FILES src/*.cpp)
file(GLOB_RECURSE toyscheme_MODULE_FILES src/*.cppm)
target_sources(toyscheme
PRIVATE ${toyscheme_SOURCE_FILES}
PRIVATE
  FILE_SET CXX_MODULES
  BASE_DIRS ${PROJECT_SOURCE_DIR}
  FILES ${toyscheme_MODULE_FILES}
)

set_target_properties(toyscheme
PROPERTIES
  CXX_STANDARD 23
  CXX_SCAN_FOR_MODULES ON
)
