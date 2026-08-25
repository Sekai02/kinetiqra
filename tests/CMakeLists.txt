# Unit tests.
#
# One executable across all modules: the suite is small, links fast, and a
# single binary keeps `ctest` output readable. Split per-module only if link
# times become a problem.

find_package(doctest CONFIG REQUIRED)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(kinetiqra_tests ${TEST_SOURCES})

target_link_libraries(kinetiqra_tests
    PRIVATE
        kinetiqra::core
        kinetiqra::math
        kinetiqra::geom
        kinetiqra::scene
        kinetiqra::anim
        kinetiqra::io
        doctest::doctest
)

kinetiqra_set_warnings(kinetiqra_tests)
set_target_properties(kinetiqra_tests PROPERTIES FOLDER "tests")

add_test(NAME kinetiqra_tests COMMAND kinetiqra_tests)
