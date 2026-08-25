# app — the editor executable.
#
# Sole owner of mutable state: Editor holds the Scene, the CommandStack and the
# Selection. Panels and tools receive references and issue commands; they never
# mutate the scene directly. That rule is what keeps undo airtight as the UI
# grows, so it is worth defending even when a direct mutation looks harmless.

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE APP_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")

add_executable(kinetiqra ${APP_SOURCES} ${APP_HEADERS})

target_include_directories(kinetiqra PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_definitions(kinetiqra
    PRIVATE
        KINETIQRA_VERSION="${PROJECT_VERSION}"
        KINETIQRA_ASSET_DIR="${KINETIQRA_ASSET_DIR}"
)

target_link_libraries(kinetiqra
    PRIVATE
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

kinetiqra_set_warnings(kinetiqra)
set_target_properties(kinetiqra PROPERTIES FOLDER "app")
