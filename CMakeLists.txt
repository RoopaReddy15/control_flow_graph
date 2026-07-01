cmake_minimum_required(VERSION 3.14)
project(cfg_tool C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Find LLVM & Clang ────────────────────────────────────────
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Using ClangConfig.cmake in: ${Clang_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# ── Map LLVM components to libraries ─────────────────────────
llvm_map_components_to_libnames(LLVM_LIBS
    core
    support
    mc
    option
    bitreader
    profiledata
    frontendopenmp
    frontendoffloading
)

# ── Build cfg_tool ────────────────────────────────────────────
add_executable(cfg_tool main.cpp)

target_include_directories(cfg_tool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(cfg_tool PRIVATE
    clang-cpp
    LLVM
)

# ── Windows-specific ──────────────────────────────────────────
if(WIN32)
    target_link_libraries(cfg_tool PRIVATE version)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()
