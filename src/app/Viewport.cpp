#include "Viewport.hpp"

#include <imgui.h>

namespace kinetiqra::app {

namespace {

// Radians of rotation per pixel dragged. Chosen so that crossing the window
// horizontally turns roughly half a revolution.
constexpr float kOrbitSensitivity = 0.008F;

}  // namespace

void Viewport::update(bool hovered, math::Vec2 viewport_size) {
    const ImGuiIO& io = ImGui::GetIO();

    if (!hovered) {
        return;
    }

    const ImVec2 drag = io.MouseDelta;
    const bool shift_held = io.KeyShift;

    // Left drag orbits, unless shift turns it into a pan, which is the habit
    // most modelling tools share for people without a middle mouse button.
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && !shift_held) {
        camera_.orbit(-drag.x * kOrbitSensitivity, -drag.y * kOrbitSensitivity);
    } else if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
               (ImGui::IsMouseDown(ImGuiMouseButton_Left) && shift_held)) {
        camera_.pan(drag.x, drag.y, viewport_size);
    }

    if (io.MouseWheel != 0.0F) {
        camera_.dolly(io.MouseWheel);
    }
}

}  // namespace kinetiqra::app
