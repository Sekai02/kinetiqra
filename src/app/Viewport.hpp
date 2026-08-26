#pragma once

#include <kinetiqra/math/OrbitCamera.hpp>

namespace kinetiqra::app {

// The camera and the input that drives it.
//
// Camera movement is deliberately not routed through the command stack. Where
// the viewer stands is view state, not scene state, and an undo history filled
// with camera moves buries the edits it exists to reverse.
class Viewport {
public:
    // Called once per frame with the pointer state gathered from ImGui. The
    // viewport only reacts when it is hovered, so dragging over a panel does
    // not move the camera behind it.
    void update(bool hovered, math::Vec2 viewport_size);

    [[nodiscard]] math::OrbitCamera& camera() { return camera_; }

    [[nodiscard]] const math::OrbitCamera& camera() const { return camera_; }

private:
    math::OrbitCamera camera_;
};

}  // namespace kinetiqra::app
