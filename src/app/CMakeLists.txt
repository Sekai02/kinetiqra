# app — the editor.
#
# Sole owner of mutable state: the editor holds the Scene, the CommandStack and
# the Selection. Panels and tools receive references and issue commands; they
# never mutate the scene directly. That rule is what keeps undo airtight as the
# UI grows, so it is worth defending even when a direct mutation looks harmless.
#
# The editor is built as a module like any other, so that its panels and tools
# can be tested. `main.cpp` sits outside `src/` on purpose: the module helper
# globs `src/*.cpp`, which leaves the entry point out of the library and keeps
# the executable a shell around it.

kinetiqra_add_module(app
    DEPENDS
        kinetiqra::core
        kinetiqra::math
        kinetiqra::geom
        kinetiqra::scene
        kinetiqra::anim
        kinetiqra::io
        kinetiqra::render
        glfw
        imgui::imgui
        imguizmo::imguizmo
)

target_compile_definitions(kinetiqra_app
    PRIVATE
        KINETIQRA_VERSION="${PROJECT_VERSION}"
        KINETIQRA_ASSET_DIR="${KINETIQRA_ASSET_DIR}"
)

add_executable(kinetiqra main.cpp)

target_link_libraries(kinetiqra PRIVATE kinetiqra::app)

kinetiqra_set_warnings(kinetiqra)
set_target_properties(kinetiqra PROPERTIES FOLDER "app")
