cmake_minimum_required(VERSION 3.23)
project(yawarakai LANGUAGES CXX)

add_executable(ywrk)

file(COPY "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.39.33519/modules/std.ixx"
  DESTINATION ${PROJECT_BINARY_DIR}/stdxx)
file(COPY "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.39.33519/modules/std.compat.ixx"
  DESTINATION ${PROJECT_BINARY_DIR}/stdxx)

file(GLOB_RECURSE ywrk_SOURCE_FILES src/*.cpp)
file(GLOB_RECURSE ywrk_MODULE_FILES src/*.cppm)
target_sources(ywrk
PRIVATE
  FILE_SET CXX_MODULES
  BASE_DIRS ${PROJECT_BINARY_DIR}/stdxx
  FILES ${PROJECT_BINARY_DIR}/stdxx/std.ixx ${PROJECT_BINARY_DIR}/stdxx/std.compat.ixx
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
