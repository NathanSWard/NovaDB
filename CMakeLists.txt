cmake_minimum_required(VERSION 3.15)
project(nova)

set(CMAKE_CXX_STANDARD 17)

SET (CMAKE_CXX_FLAGS_INIT                "-Wall -Wpedantic -Wextra")
SET (CMAKE_CXX_FLAGS_DEBUG_INIT          "-g")
SET (CMAKE_CXX_FLAGS_MINSIZEREL_INIT     "-Os -DNDEBUG")
SET (CMAKE_CXX_FLAGS_RELEASE_INIT        "-O3 -DNDEBUG")
SET (CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "-O2 -g")

set(DEPS_DIR ${PROJECT_SOURCE_DIR}/deps)
include_directories(${DEPS_DIR}/fmt/include ${DEPS_DIR}/abseil-cpp/absl)

add_subdirectory(${DEPS_DIR}/abseil-cpp)

add_executable(nova tests/main.cpp)

target_link_libraries(nova absl::flat_hash_map absl::node_hash_map absl::btree)
