# render — OpenGL 4.5 (DSA) renderer, derived render meshes, passes.
#
# Consumes scene/geom data and produces pixels; never mutates either. The
# GlDevice seam is deliberately thin rather than a full RHI abstraction — enough
# to make a future Vulkan backend possible, not enough to cost weeks now.
kinetiqra_add_module(render
    DEPENDS
        kinetiqra::core
        kinetiqra::math
        kinetiqra::geom
        kinetiqra::scene
        kinetiqra::anim
        glad::glad
)
