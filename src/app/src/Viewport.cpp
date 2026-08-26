#include <kinetiqra/app/Viewport.hpp>

namespace kinetiqra::app {

namespace {

// Radians of rotation per unit dragged. Chosen so that crossing the window
// horizontally turns roughly half a revolution.
constexpr float kOrbitSensitivity = 0.008F;

}  // namespace

void Viewport::update(const CameraInput& input, math::Vec2 render_size) {
    const bool holding = input.left || input.middle;

    if (!holding) {
        drag_ = Drag::None;
    } else if (drag_ == Drag::None && input.over_world) {
        // Shift with the left button pans, which is the habit modelling tools
        // share for people without a middle mouse button.
        drag_ = (input.middle || input.shift) ? Drag::Pan : Drag::Orbit;
    }

    switch (drag_) {
        case Drag::Orbit:
            camera_.orbit(-input.delta.x * kOrbitSensitivity, -input.delta.y * kOrbitSensitivity);
            break;
        case Drag::Pan:
            camera_.pan(input.delta.x, input.delta.y, render_size);
            break;
        case Drag::None:
            break;
    }

    // The wheel is not a gesture, so it is judged where the pointer is now. That
    // also leaves scrolling a panel to the panel.
    if (input.over_world && input.wheel != 0.0F) {
        camera_.dolly(input.wheel);
    }
}

}  // namespace kinetiqra::app
