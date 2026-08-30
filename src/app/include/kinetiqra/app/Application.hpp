#pragma once

#include <kinetiqra/anim/Clip.hpp>
#include <kinetiqra/anim/Player.hpp>
#include <kinetiqra/app/Commands.hpp>
#include <kinetiqra/app/Selection.hpp>
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
    void draw_edit_panel();

    // The gizmo, and the click that decides what it is pointed at.
    void update_gizmo(const math::Mat4& view, const math::Mat4& projection);
    void update_picking();

    // One command per gesture: the scene is written directly while the pointer
    // is down so that the model follows it, and the command is what makes the
    // result permanent. A command per frame would bury the history.
    void drag_node(const math::Mat4& world);
    void drag_vertices(const math::Vec3& world_delta);
    void end_drag();
    void extrude_selection();

    // The world matrix the gizmo is placed at: the node in object mode, and the
    // middle of the selected elements in mesh mode.
    [[nodiscard]] bool gizmo_anchor(math::Mat4& world) const;

    [[nodiscard]] geom::EditMesh* selected_mesh();
    [[nodiscard]] const geom::EditMesh* selected_mesh() const;
    [[nodiscard]] scene::MeshId selected_mesh_id() const;

    void draw_selection(const math::Mat4& view_projection) const;

    // Evaluates the selected clip into the pose, or drops the pose when nothing
    // is playing so that the scene draws as it was authored.
    void update_animation(float delta_seconds);

    // Null while stopped, which is what makes the scene the source of truth
    // again the moment playback ends.
    [[nodiscard]] const scene::Pose* active_pose() const;
    void draw_node(scene::NodeId id);
    void handle_shortcuts();

    // Undo can put back geometry, so what is on the GPU has to be rebuilt, and
    // a selection made against elements that have just stopped existing is
    // dropped rather than left pointing at nothing.
    void undo();
    void redo();

    void shutdown();

    void load_default_scene();
    void load_scene(const std::filesystem::path& path);

    // Names the file the Export button writes to, after whatever was loaded, so
    // that exporting never silently overwrites the model that was opened.
    void suggest_export_path(const std::string& stem);

    // Bakes every mesh in the scene and uploads it. Called when the scene is
    // replaced.
    void rebuild_render_meshes();

    // Just the one, which is what an edit needs: rebaking the whole scene on
    // every frame of a drag would be work proportional to the model rather than
    // to the change.
    void rebuild_render_mesh(scene::MeshId mesh);

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

    Selection selection_;
    GizmoOperation gizmo_operation_{GizmoOperation::Translate};

    // What the gesture in progress started from, so that the command can hold
    // both ends of it. The scene is put back to `before` for a moment when the
    // gesture ends, so that executing the command is what applies the change
    // and undo has something to return to.
    bool gizmo_dragging_{false};
    scene::Transform node_before_;
    std::vector<MoveVertices::Moved> vertex_drag_;

    // How to get from the world back to where each of those vertices is
    // stored, taken once when the gesture starts. On a skinned mesh this
    // differs per vertex, and it is what stops a drag on a rigged model moving
    // the geometry a hundred times too far.
    std::vector<math::Mat4> vertex_drag_inverse_;

    // The gizmo's own matrix, owned across the drag rather than recomputed from
    // the selection each frame. The selection moves as the drag goes on, so
    // recomputing it would leave nothing to measure the total against.
    math::Mat4 gizmo_matrix_{1.0F};
    math::Mat4 gizmo_start_{1.0F};

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
