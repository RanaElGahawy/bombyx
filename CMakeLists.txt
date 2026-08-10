cmake_minimum_required(VERSION 3.15)

# Project name
project(bombyx-cc)

# Platform-specific default install path
set(DEFAULT_OPENCILK_HOME "/opt/opencilk") # Linux/macOS default

set(OPENCILK_HOME "${DEFAULT_OPENCILK_HOME}" CACHE PATH "OpenCilk installation root directory")

# Convert to absolute path and verify
get_filename_component(OPENCILK_HOME "${OPENCILK_HOME}" ABSOLUTE)

# Check if the directory exists (or can be created)
if(NOT EXISTS "${OPENCILK_HOME}")
    message(FATAL_ERROR 
        "OPENCILK_HOME directory '${OPENCILK_HOME}' does not exist.\n"
        "Please specify a valid path with -DOPENCILK_HOME=/path/to/opencilk."
    )
else()
    message("-- Using OpenCilk installation at ${OPENCILK_HOME}")
endif()

# Set the CMake prefix path to include custom LLVM and Clang installation paths
set(CMAKE_PREFIX_PATH "${OPENCILK_HOME}/lib/cmake")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
# If we are building in DEBUG mode, add -g flag
if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    message(STATUS "Building in DEBUG mode")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
else()
    message(STATUS "Building in RELEASE mode")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fno-rtti -fsanitize=address")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fno-rtti")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOPENCILK_HOME=\\\"${OPENCILK_HOME}\\\"")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOMBYX_SUPPORT_DIR=\\\"${CMAKE_SOURCE_DIR}/support\\\"")

# Specify the custom LLVM and Clang installation path
set(LLVM_DIR "${OPENCILK_HOME}/lib/cmake/llvm")
set(CLANG_DIR "${OPENCILK_HOME}/lib/cmake/clang")

# Find LLVM and Clang
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

# Add LLVM headers
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
include_directories("${OPENCILK_HOME}/include")
# Link directories
link_directories("${OPENCILK_HOME}/lib")

# Add LLVM and Clang libraries
add_definitions(${LLVM_DEFINITIONS})

# Export compile commands for Clang tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories("include")
# Source files — grouped by subsystem
set(SOURCE_FILES
    src/main.cpp

    # Compiler core: IR, OpenCilk frontend, IR transformations
    src/core/IR.cpp
    src/core/OpenCilk2IR.cpp
    src/core/FlattenIR.cpp
    src/core/MakeExplicit.cpp
    src/core/DAE.cpp
    src/core/OverlapMemAnalysis.cpp
    src/core/PruneDeadSpawnArgs.cpp
    src/core/CountSpawns.cpp
    src/core/desugarOpenCilk.cpp
    src/core/Cilk1EmuTarget.cpp
    src/core/TBBTarget.cpp

    # HardCilk analysis: task/closure/memory analysis, descriptor generation, driver
    src/hardcilk/HardCilkAnalysis.cpp
    src/hardcilk/HardCilkDescGen.cpp
    src/hardcilk/HardCilkDriver.cpp
    src/hardcilk/HardCilkOverlapWrapperGen.cpp

    # Vitis HLS backend: code generation and TCL synthesis scripts
    src/vitis/VitisHLSTarget.cpp
    src/vitis/VitisHLSTclGen.cpp
)

# Add executable
add_executable(bombyx-cc ${SOURCE_FILES})

set_target_properties(bombyx-cc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Link against LLVM and Clang libraries
target_link_libraries(bombyx-cc PRIVATE
    LLVMCore
    clangTooling
    clangFrontend
    clangSerialization
    clangDriver
)


get_filename_component(CILK_INSTALL_INCLUDE_DIR "${OPENCILK_HOME}/include" ABSOLUTE)
get_filename_component(CILK_INSTALL_BIN_DIR "${OPENCILK_HOME}/bin" ABSOLUTE)

install(
    TARGETS bombyx-cc
    RUNTIME   
)
install(FILES support/cilk_explicit.hh support/tbb_explicit.hh DESTINATION include/)