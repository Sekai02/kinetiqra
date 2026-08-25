# Centralized warning configuration.
#
# Warnings are applied per-target rather than globally so that third-party code
# pulled in by vcpkg is never compiled with our (deliberately strict) settings.

function(kinetiqra_set_warnings target)
    set(_gcc_clang
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wdouble-promotion
        -Wnull-dereference
        -Wformat=2
    )

    set(_msvc
        /W4
        /permissive-
        /w14640  # thread-unsafe static member initialization
        /w14826  # conversion is sign-extended
    )

    if(MSVC)
        set(_warnings ${_msvc})
        if(KINETIQRA_WARNINGS_AS_ERRORS)
            list(APPEND _warnings /WX)
        endif()
    else()
        set(_warnings ${_gcc_clang})
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            list(APPEND _warnings -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast)
        endif()
        if(KINETIQRA_WARNINGS_AS_ERRORS)
            list(APPEND _warnings -Werror)
        endif()
    endif()

    target_compile_options(${target} PRIVATE ${_warnings})
endfunction()
