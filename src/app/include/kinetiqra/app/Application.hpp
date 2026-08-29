#pragma once

#include <kinetiqra/app/Viewport.hpp>
#include <kinetiqra/geom/EditMesh.hpp>
#include <kinetiqra/render/RenderMesh.hpp>
#include <kinetiqra/render/Renderer.hpp>

#include <string>

struct GLFWwindow;

namespace kinetiqra::app {

// The editor shell: the window, the GL context, the ImGui docking layout and
// the frame loop.
//
// This is the only place that owns mutable state. When there is a scene to
// edit, it will live here alongside the command stack and the selection, and
// panels will reach it by reference and issue commands rather than reaching
// into it directly.
class Application {
public:
    Application() = default;
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    // Returns false and fills `error` if the window, the GL context or the
    // renderer could not be brought up.
    bool initialise(std::string& error);

    void run();

private:
    void draw_frame();
    void update_camera();
    void draw_camera_panel();
    void shutdown();

    GLFWwindow* window_{nullptr};
    render::Renderer renderer_;
    Viewport viewport_;

    // The editable mesh is the model; the render mesh is a copy of it in the
    // shape the GPU needs. Keeping both means the split stays visible, and it
    // is what the counts in the panel report.
    geom::EditMesh mesh_;
    render::RenderMesh render_mesh_;

    bool imgui_ready_{false};
};

}  // namespace kinetiqra::app
