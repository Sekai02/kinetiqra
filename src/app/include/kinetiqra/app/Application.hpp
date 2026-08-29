#pragma once

#include <kinetiqra/anim/Clip.hpp>
#include <kinetiqra/anim/Player.hpp>
#include <kinetiqra/app/Viewport.hpp>
#include <kinetiqra/core/Command.hpp>
#include <kinetiqra/render/RenderMesh.hpp>
#include <kinetiqra/render/Renderer.hpp>
#include <kinetiqra/scene/Pose.hpp>
#include <kinetiqra/scene/Scene.hpp>

#include <array>
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
    void draw_timeline_panel();

    // Evaluates the selected clip into the pose, or drops the pose when nothing
    // is playing so that the scene draws as it was authored.
    void update_animation(float delta_seconds);

    // Null while stopped, which is what makes the scene the source of truth
    // again the moment playback ends.
    [[nodiscard]] const scene::Pose* active_pose() const;
    void draw_node(scene::NodeId id);
    void handle_shortcuts();
    void shutdown();

    void load_default_scene();
    void load_scene(const std::filesystem::path& path);

    // Names the file the Export button writes to, after whatever was loaded, so
    // that exporting never silently overwrites the model that was opened.
    void suggest_export_path(const std::string& stem);

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

    // The clips the file carried, the playhead, and the pose the clip is
    // evaluated into. The pose is never written back to the scene: playing is a
    // view of the document, not an edit of it, which is what keeps the user's
    // hand-made pose and the undo history intact.
    std::vector<anim::Clip> clips_;
    anim::Player player_;
    scene::Pose pose_;
    std::size_t clip_index_{0};
    bool showing_pose_{false};

    scene::NodeId selected_;
    std::string source_;
    std::string load_error_;

    // Where the Export button writes, and what happened the last time it was
    // pressed. A plain text field rather than a file dialog, which is a
    // window system's job and not one ImGui does for us.
    std::array<char, 256> export_path_{};
    std::string export_status_;

    // The rotation being dragged, in degrees, and what it was before the drag
    // started. A command is pushed when the control is released, not on every
    // frame, or a single drag would bury the history under hundreds of steps.
    math::Vec3 pose_euler_{0.0F, 0.0F, 0.0F};
    math::Quat pose_before_{1.0F, 0.0F, 0.0F, 0.0F};
    bool pose_dragging_{false};

    bool imgui_ready_{false};
};

}  // namespace kinetiqra::app
