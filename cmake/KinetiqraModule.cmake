# Module definition helper.
#
# Every kinetiqra module has the same shape:
#
#     src/<name>/
#       include/kinetiqra/<name>/   public headers  (visible to dependents)
#       src/                        private headers + translation units
#
# Only `include/` is on the public include path, so a header that is not there
# is physically unreachable from other modules. Dependencies are declared with
# DEPENDS and enforced by the linker, which is what keeps the layering honest:
# geom cannot reach into render by accident, it simply will not compile.
#
# Usage:
#     kinetiqra_add_module(geom DEPENDS kinetiqra::core kinetiqra::math)
#
# Modules with no .cpp files yet become INTERFACE (header-only) targets and are
# promoted to STATIC automatically once a translation unit appears. This lets us
# wire up the whole dependency graph before writing any implementation.

function(kinetiqra_add_module MODULE_NAME)
    cmake_parse_arguments(ARG "" "" "DEPENDS" ${ARGN})

    set(_target kinetiqra_${MODULE_NAME})
    set(_dir ${CMAKE_CURRENT_SOURCE_DIR})

    file(GLOB_RECURSE _sources CONFIGURE_DEPENDS "${_dir}/src/*.cpp")
    file(GLOB_RECURSE _headers CONFIGURE_DEPENDS
        "${_dir}/include/*.hpp"
        "${_dir}/src/*.hpp"
    )

    if(_sources)
        add_library(${_target} STATIC ${_sources} ${_headers})
        set(_scope PUBLIC)
        target_include_directories(${_target} PRIVATE "${_dir}/src")
        kinetiqra_set_warnings(${_target})
        set_target_properties(${_target} PROPERTIES FOLDER "modules")
    else()
        add_library(${_target} INTERFACE)
        set(_scope INTERFACE)
    endif()

    add_library(kinetiqra::${MODULE_NAME} ALIAS ${_target})

    target_include_directories(${_target} ${_scope} "${_dir}/include")
    target_compile_features(${_target} ${_scope} cxx_std_20)

    if(ARG_DEPENDS)
        target_link_libraries(${_target} ${_scope} ${ARG_DEPENDS})
    endif()

    if(_sources)
        list(LENGTH _sources _source_count)
        message(STATUS "  module ${MODULE_NAME}: STATIC (${_source_count} sources)")
    else()
        message(STATUS "  module ${MODULE_NAME}: INTERFACE (no sources yet)")
    endif()
endfunction()
