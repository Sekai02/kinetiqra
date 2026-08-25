cmake_minimum_required(VERSION 3.25)

project(kinetiqra
    VERSION 0.1.0
    DESCRIPTION "A code-first 3D humanoid avatar and animation engine."
    HOMEPAGE_URL "https://github.com/Sekai02/kinetiqra"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

option(KINETIQRA_BUILD_TESTS "Build the unit test suite" ON)
option(KINETIQRA_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(KinetiqraModule)
include(Dependencies)

# Absolute path to runtime assets (shaders, fonts). During development these are
# loaded from the source tree so shader edits hot-reload without a rebuild; a
# shipping build will embed them instead.
set(KINETIQRA_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------

add_subdirectory(src)

if(KINETIQRA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "")
message(STATUS "kinetiqra ${PROJECT_VERSION}")
message(STATUS "  build type:  ${CMAKE_BUILD_TYPE}")
message(STATUS "  compiler:    ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  tests:       ${KINETIQRA_BUILD_TESTS}")
message(STATUS "")
