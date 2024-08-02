cmake_minimum_required(VERSION 3.30)
project(yawarakai LANGUAGES CXX)

add_executable(ywrk)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD ON)

file(GLOB_RECURSE ywrk_SOURCE_FILES src/*.cpp)
file(GLOB_RECURSE ywrk_MODULE_FILES src/*.cppm)
target_sources(ywrk
PRIVATE ${ywrk_SOURCE_FILES}
PRIVATE
  FILE_SET CXX_MODULES
  BASE_DIRS ${PROJECT_SOURCE_DIR}
  FILES ${ywrk_MODULE_FILES}
)

set_target_properties(ywrk
PROPERTIES
  CXX_STANDARD 23
  CXX_SCAN_FOR_MODULES ON
)
