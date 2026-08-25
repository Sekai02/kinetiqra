# math — transforms, AABB, ray, TRS decomposition. A thin layer over GLM that
# fixes our conventions (right-handed, Y-up, metres) in one place.
kinetiqra_add_module(math
    DEPENDS
        kinetiqra::core
        glm::glm
)
