# Module graph.
#
# Dependencies flow strictly downward. Nothing below may depend on anything
# above it, and `geom`/`scene` in particular must never learn that `io` or
# `render` exist. add_subdirectory order mirrors that layering.

message(STATUS "kinetiqra modules:")

add_subdirectory(core)
add_subdirectory(math)
add_subdirectory(geom)
add_subdirectory(scene)
add_subdirectory(anim)
add_subdirectory(io)
add_subdirectory(render)
add_subdirectory(app)
