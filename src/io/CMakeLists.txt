# io — glTF import/export and the native .kqr container.
#
# All format-specific concerns are quarantined here: coordinate conversion,
# unit handling, and the flattening bake that turns editable data into the
# GPU-ready form glTF expects. Nothing in geom/scene/anim knows glTF exists,
# which is what makes adding FBX/USD/VRM later a matter of adding files here.
kinetiqra_add_module(io
    DEPENDS
        kinetiqra::core
        kinetiqra::math
        kinetiqra::geom
        kinetiqra::scene
        kinetiqra::anim
        fastgltf::fastgltf
        stb::stb
)
