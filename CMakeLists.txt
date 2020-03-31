cmake_minimum_required(VERSION 3.15)
project(nova)

set(CMAKE_CXX_STANDARD 20)

set(DEPS_DIR ${PROJECT_SOURCE_DIR}/deps)
include_directories(${DEPS_DIR}/fmt/include)

add_executable(nova tests/main.cpp)
