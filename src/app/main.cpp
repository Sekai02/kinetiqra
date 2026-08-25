// Entry point.
//
// Deliberately minimal: this exists to prove the build graph links and that the
// vcpkg dependencies resolve. Window creation, the GL 4.5 context and the ImGui
// docking shell arrive in the next milestone.

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <cstdio>

int main() {
    int glfw_major = 0;
    int glfw_minor = 0;
    int glfw_revision = 0;
    glfwGetVersion(&glfw_major, &glfw_minor, &glfw_revision);

    // Touch GLM so the dependency is exercised rather than merely linked.
    const glm::vec3 up{0.0F, 1.0F, 0.0F};

    std::printf("kinetiqra %s\n", KINETIQRA_VERSION);
    std::printf("  assets: %s\n", KINETIQRA_ASSET_DIR);
    std::printf("  glfw:   %d.%d.%d\n", glfw_major, glfw_minor, glfw_revision);
    std::printf("  up:     (%.1f, %.1f, %.1f)\n", static_cast<double>(up.x),
                static_cast<double>(up.y), static_cast<double>(up.z));

    return 0;
}
