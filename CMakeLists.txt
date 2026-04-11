cmake_minimum_required(VERSION 3.30)

# Enable import std detection before the CXX language is enabled.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

project(toyscheme LANGUAGES CXX)

set(CMAKE_CXX_SCAN_FOR_MODULES ON)

if(23 IN_LIST CMAKE_CXX_COMPILER_IMPORT_STD)
  set(CMAKE_CXX_MODULE_STD ON)
endif()

add_executable(toyscheme)

target_compile_features(toyscheme PRIVATE cxx_std_23)

target_sources(toyscheme
  PRIVATE
    src/main.cpp
    src/toyscheme/general.cpp
    src/toyscheme/eval.cpp
    src/toyscheme/memory.cpp
  PRIVATE
    FILE_SET CXX_MODULES
    BASE_DIRS ${PROJECT_SOURCE_DIR}/src
    FILES
      src/toyscheme.cppm
      src/toyscheme-lisp.cppm
      src/toyscheme-memory.cppm
      src/toyscheme-util.cppm
)
