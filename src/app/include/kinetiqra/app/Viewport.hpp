#pragma once

#include <kinetiqra/math/OrbitCamera.hpp>

namespace kinetiqra::app {

// One frame's worth of pointer state, gathered by the caller.
//
// The viewport takes this rather than reading the UI library directly, which is
// what lets the rules below be tested without a window, a context or a frame.
struct CameraInput {
    math::Vec2 delta{0.0F, 0.0F};
    float wheel{0.0F};
    bool left{false};
    bool middle{false};
    bool shift{false};

    // False while the pointer is over a panel, which is where the world is not.
    bool over_world{false};
};

// The camera and the input that drives it.
//
// Camera movement is deliberately not routed through the command stack. Where
// the viewer stands is view state, not scene state, and an undo history filled
// with camera moves buries the edits it exists to reverse.
class Viewport {
public:
    // `render_size` is the region the world is drawn into, measured in the same
    // units as `input.delta`, and is what panning is scaled against.
    //
    // A drag starts only over the world, never over a panel, but once started it
    // keeps control until the button is released. Re-checking every frame would
    // stop the camera dead the moment the cursor crossed a panel mid-gesture.
    void update(const CameraInput& input, math::Vec2 render_size);

    [[nodiscard]] math::OrbitCamera& camera() { return camera_; }

    [[nodiscard]] const math::OrbitCamera& camera() const { return camera_; }

    [[nodiscard]] bool dragging() const { return drag_ != Drag::None; }

private:
    enum class Drag { None, Orbit, Pan };

    math::OrbitCamera camera_;
    Drag drag_{Drag::None};
};

}  // namespace kinetiqra::app
