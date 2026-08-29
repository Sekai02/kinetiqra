#pragma once

#include <kinetiqra/app/Viewport.hpp>
#include <kinetiqra/core/Command.hpp>
#include <kinetiqra/render/RenderMesh.hpp>
#include <kinetiqra/render/Renderer.hpp>
#include <kinetiqra/scene/Scene.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

struct GLFWwindow;

namespace kinetiqra::app {

// The editor shell: the window, the GL context, the ImGui docking layout, the
// scene and the frame loop.
//
// This is the only place that owns mutable state. When there are edits to
// undo, the command stack will live here too, alongside the scene and the
// selection, and panels will issue commands rather than reaching in.
class Application {
public:
    Application() = default;
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    // `model` is optional. Without one the editor opens on a built-in box, so
    // it never starts on an empty screen.
    bool initialise(const std::filesystem::path& model, std::string& error);

    void run();

private:
    void draw_frame();
    void update_camera();
    void draw_camera_panel();
    void draw_scene_panel();
    void draw_pose_panel();
    void draw_node(scene::NodeId id);
    void handle_shortcuts();
    void shutdown();

    void load_default_scene();
    void load_scene(const std::filesystem::path& path);

    // Bakes every mesh in the scene and uploads it. Called when the scene is
    // replaced, which is the only thing that can change it so far.
    void rebuild_render_meshes();

    static void on_files_dropped(GLFWwindow* window, int count, const char** paths);

    GLFWwindow* window_{nullptr};
    render::Renderer renderer_;
    Viewport viewport_;

    scene::Scene scene_;

    // Keyed by the mesh handle's index, which is stable for as long as the
    // scene lives and is replaced wholesale along with it.
    std::unordered_map<std::uint32_t, render::RenderMesh> render_meshes_;

    core::CommandStack commands_;

    scene::NodeId selected_;
    std::string source_;
    std::string load_error_;

    // The rotation being dragged, in degrees, and what it was before the drag
    // started. A command is pushed when the control is released, not on every
    // frame, or a single drag would bury the history under hundreds of steps.
    math::Vec3 pose_euler_{0.0F, 0.0F, 0.0F};
    math::Quat pose_before_{1.0F, 0.0F, 0.0F, 0.0F};
    bool pose_dragging_{false};

    bool imgui_ready_{false};
};

}  // namespace kinetiqra::app
