#include <kinetiqra/app/Application.hpp>
#include <kinetiqra/app/Commands.hpp>
#include <kinetiqra/app/Gizmo.hpp>
#include <kinetiqra/geom/Bake.hpp>
#include <kinetiqra/geom/Extrude.hpp>
#include <kinetiqra/geom/Primitives.hpp>
#include <kinetiqra/io/Image.hpp>
#include <kinetiqra/io/gltf/GltfExport.hpp>
#include <kinetiqra/io/gltf/GltfImport.hpp>
#include <kinetiqra/scene/Raycast.hpp>

#define GLM_ENABLE_EXPERIMENTAL

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <unordered_set>

namespace kinetiqra::app {

namespace {

// Not the size the window opens at, which is maximised. This is the size it
// returns to when someone un-maximises it.
constexpr int kInitialWidth = 1280;
constexpr int kInitialHeight = 800;

// The dockspace is named here rather than left to ImGui to invent, because the
// layout has to be built before the dockspace is submitted and that means
// knowing its id first. Any constant will do, as long as it is the same one
// every frame.
constexpr ImGuiID kDockspaceId = 0x4B494E45;  // "KINE"

// How the window is carved up on a first run, as fractions of what is left at
// each step rather than of the whole, since every split divides the remainder
// of the one before it.
constexpr float kTimelineHeight = 0.20F;
constexpr float kLeftColumnWidth = 0.19F;
constexpr float kRightColumnWidth = 0.26F;
constexpr float kCameraHeight = 0.32F;
constexpr float kPoseHeight = 0.55F;

// How far the pointer may travel between press and release and still count as a
// click rather than as a camera drag. Both use the left button, and a picking
// that fired at the end of every orbit would be unusable.
constexpr float kClickSlop = 4.0F;

constexpr float kVertexPointSize = 7.0F;
constexpr float kSelectedPointSize = 11.0F;

const math::Vec4 kVertexColour{0.55F, 0.57F, 0.62F, 1.0F};
const math::Vec4 kSelectedColour{1.0F, 0.62F, 0.20F, 1.0F};
const math::Vec4 kFaceColour{1.0F, 0.62F, 0.20F, 0.45F};

void report_glfw_error(int code, const char* description) {
    std::fprintf(stderr, "glfw error %d: %s\n", code, description);
}

ImGuizmo::OPERATION as_gizmo(GizmoOperation operation) {
    switch (operation) {
        case GizmoOperation::Rotate:
            return ImGuizmo::ROTATE;
        case GizmoOperation::Scale:
            return ImGuizmo::SCALE;
        case GizmoOperation::Translate:
            break;
    }
    return ImGuizmo::TRANSLATE;
}

}  // namespace

Application::~Application() {
    shutdown();
}

bool Application::initialise(const std::filesystem::path& model, std::string& error) {
    glfwSetErrorCallback(report_glfw_error);

    if (glfwInit() == GLFW_FALSE) {
        error = "could not initialise GLFW";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    // Open filling the screen, the way an editor is expected to. Maximised
    // rather than fullscreen: the title bar stays, the taskbar is respected,
    // and the window can be moved and shrunk without a shortcut invented for
    // the purpose.
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    window_ = glfwCreateWindow(kInitialWidth, kInitialHeight, "kinetiqra", nullptr, nullptr);
    if (window_ == nullptr) {
        // Almost always a driver without 4.5 core, which is worth saying plainly
        // rather than letting the first GL call fail somewhere further in.
        error = "could not create a window with an OpenGL 4.5 core context";
        return false;
    }

    // The hint above is ignored by some window managers, including the X11 one
    // this is developed on, where a window created with it comes up at the size
    // it was asked for and never maximises. Asking again once the window exists
    // is honoured, so both are here: the hint avoids a visible resize where it
    // works, and this covers the rest.
    glfwMaximizeWindow(window_);

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    if (!renderer_.initialise(reinterpret_cast<render::GlLoader>(glfwGetProcAddress),
                              std::string(KINETIQRA_ASSET_DIR) + "/shaders", error)) {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();

    // Asked now, before the first frame reads the file. A layout the user
    // arranged is theirs to keep, so the built-in one is only laid out when
    // there is nothing to keep; otherwise it would be quietly overwritten on
    // every start.
    layout_ready_ = io.IniFilename != nullptr && std::filesystem::exists(io.IniFilename);

    if (!ImGui_ImplGlfw_InitForOpenGL(window_, true)) {
        error = "could not initialise the ImGui GLFW backend";
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 450")) {
        error = "could not initialise the ImGui OpenGL backend";
        return false;
    }

    imgui_ready_ = true;

    // Files can also arrive by being dropped on the window, which GLFW reports
    // through this callback. The user pointer is how it finds its way back here.
    glfwSetWindowUserPointer(window_, this);
    glfwSetDropCallback(window_, on_files_dropped);

    if (model.empty()) {
        load_default_scene();
    } else {
        load_scene(model);
    }

    std::printf("kinetiqra %s\n", KINETIQRA_VERSION);
    std::printf("  renderer: %s\n", renderer_.driver_description().c_str());
    if (!load_error_.empty()) {
        std::fprintf(stderr, "  %s\n", load_error_.c_str());
    }

    return true;
}

void Application::load_default_scene() {
    scene_.clear();

    clips_.clear();
    pose_.clear();
    showing_pose_ = false;
    player_ = anim::Player{};

    // A box built in code, so the editor opens on something rather than on an
    // empty grid.
    const scene::MeshId mesh = scene_.add_mesh(geom::make_box());
    const scene::NodeId node = scene_.add_node("box");
    scene_.set_mesh(node, mesh);
    scene_.node(node)->transform.translation = math::Vec3{0.0F, 0.5F, 0.0F};

    commands_.clear();
    source_ = "built-in box";
    load_error_.clear();
    selection_ = Selection{};
    selection_.set_node(node);
    suggest_export_path("box");
    rebuild_textures();
    rebuild_render_meshes();
}

void Application::suggest_export_path(const std::string& stem) {
    const std::string suggestion = stem + "-export.glb";
    std::snprintf(export_path_.data(), export_path_.size(), "%s", suggestion.c_str());
    export_status_.clear();
}

void Application::load_scene(const std::filesystem::path& path) {
    std::string error;
    if (!io::import_gltf(path, scene_, error, &clips_)) {
        // The scene was left empty by the importer, so fall back rather than
        // leaving the editor showing nothing with no explanation.
        load_default_scene();
        load_error_ = error;
        std::fprintf(stderr, "kinetiqra: %s\n", error.c_str());
        return;
    }

    commands_.clear();
    source_ = path.filename().string();
    load_error_.clear();
    suggest_export_path(path.stem().string());
    selection_ = Selection{};
    selection_.set_node(scene_.roots().empty() ? scene::NodeId{} : scene_.roots().front());
    rebuild_textures();
    rebuild_render_meshes();

    clip_index_ = 0;
    pose_.clear();
    showing_pose_ = false;
    player_ = anim::Player{};
    if (!clips_.empty()) {
        player_.set_duration(clips_.front().duration);
    }

    std::printf("loaded %s: %zu nodes, %zu meshes, %zu clips\n", source_.c_str(),
                scene_.node_count(), scene_.mesh_count(), clips_.size());
    std::fflush(stdout);
}

void Application::rebuild_render_meshes() {
    render_meshes_.clear();

    for (const scene::MeshId id : scene_.meshes()) {
        const geom::EditMesh* mesh = scene_.mesh(id);
        if (mesh == nullptr) {
            continue;
        }
        render_meshes_[id.index].upload(geom::bake(*mesh));
    }
}

void Application::rebuild_textures() {
    textures_.clear();

    // Base colour and emissive are pictures, stored bent by a curve that has to
    // be undone before any lighting maths. A normal map or a roughness map is
    // not a picture: its numbers are directions and measurements, and
    // straightening them would corrupt every one of them.
    std::unordered_set<std::uint32_t> as_colour;
    for (const scene::MaterialId material_id : scene_.materials()) {
        const scene::Material* material = scene_.material(material_id);
        if (material == nullptr) {
            continue;
        }

        for (const scene::Texture* texture :
             {&material->base_colour_texture, &material->emissive_texture}) {
            if (texture->valid()) {
                as_colour.insert(texture->image.index);
            }
        }
    }

    for (const scene::ImageId id : scene_.images()) {
        const scene::Image* image = scene_.image(id);
        if (image == nullptr || image->empty()) {
            continue;
        }

        std::string error;
        const io::DecodedImage decoded = io::decode_image(image->bytes, error);
        if (!decoded.valid()) {
            // A picture that will not decode is not worth refusing the model
            // over. The material draws with white where it would have gone, and
            // the reason is said out loud rather than swallowed.
            std::fprintf(stderr, "kinetiqra: %s\n", error.c_str());
            continue;
        }

        // Whether an image holds colours or measurements is decided by the
        // material using it, not by the image, so the answer is worked out
        // first and looked up here.
        const bool colour = as_colour.count(id.index) != 0;

        textures_[id.index].upload(
            decoded.pixels.data(), decoded.width, decoded.height,
            colour ? render::ColourSpace::Srgb : render::ColourSpace::Linear);
    }
}

render::MaterialDraw Application::material_for(std::uint32_t index) const {
    render::MaterialDraw draw;

    const scene::Material* material = scene_.material(scene_.material_at(index));
    if (material == nullptr) {
        // A mesh that names no material, which is every mesh in a file that has
        // none. White, lit, and not a special case anywhere else.
        return draw;
    }

    draw.base_colour = material->base_colour;
    draw.metallic = material->metallic;
    draw.roughness = material->roughness;
    draw.emissive = material->emissive;
    draw.normal_scale = material->normal_scale;
    draw.occlusion_strength = material->occlusion_strength;
    draw.alpha_cutoff = material->alpha_cutoff;
    draw.double_sided = material->double_sided;

    switch (material->alpha_mode) {
        case scene::AlphaMode::Mask:
            draw.alpha_mode = 1;
            break;
        case scene::AlphaMode::Blend:
            draw.alpha_mode = 2;
            break;
        case scene::AlphaMode::Opaque:
            break;
    }

    const auto map = [this](const scene::Texture& texture) -> const render::Texture* {
        if (!texture.valid()) {
            return nullptr;
        }
        const auto found = textures_.find(texture.image.index);
        return found != textures_.end() ? &found->second : nullptr;
    };

    draw.base_colour_map = map(material->base_colour_texture);
    draw.metallic_roughness_map = map(material->metallic_roughness_texture);
    draw.normal_map = map(material->normal_texture);
    draw.occlusion_map = map(material->occlusion_texture);
    draw.emissive_map = map(material->emissive_texture);

    return draw;
}

void Application::rebuild_render_mesh(scene::MeshId mesh) {
    const geom::EditMesh* editable = scene_.mesh(mesh);
    if (editable == nullptr) {
        return;
    }
    render_meshes_[mesh.index].upload(geom::bake(*editable));
}

scene::MeshId Application::selected_mesh_id() const {
    const scene::Node* node = scene_.node(selection_.node());
    return node != nullptr ? node->mesh : scene::MeshId{};
}

geom::EditMesh* Application::selected_mesh() {
    return scene_.mesh(selected_mesh_id());
}

const geom::EditMesh* Application::selected_mesh() const {
    return scene_.mesh(selected_mesh_id());
}

bool Application::gizmo_anchor(math::Mat4& world) const {
    const scene::Node* node = scene_.node(selection_.node());
    if (node == nullptr) {
        return false;
    }

    if (selection_.mode() == EditMode::Object) {
        world = scene_.world_transform(selection_.node());
        return true;
    }

    const geom::EditMesh* mesh = selected_mesh();
    if (mesh == nullptr) {
        return false;
    }

    const std::vector<geom::VertexId> moving = selection_.moving_vertices(*mesh);
    if (moving.empty()) {
        return false;
    }

    // Where the vertices are drawn, not where they are stored. On a skinned
    // mesh those are different places, and a handle at the stored one would sit
    // in empty space far from the model.
    const std::vector<math::Vec3> positions = scene_.world_positions(selection_.node());

    math::Vec3 centre{0.0F, 0.0F, 0.0F};
    for (const geom::VertexId vertex : moving) {
        if (vertex.index < positions.size()) {
            centre += positions[vertex.index];
        }
    }
    centre /= static_cast<float>(moving.size());

    world = glm::translate(math::Mat4{1.0F}, centre);
    return true;
}

void Application::update_gizmo(const math::Mat4& view, const math::Mat4& projection) {
    const ImGuiIO& io = ImGui::GetIO();

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
    ImGuizmo::SetRect(0.0F, 0.0F, io.DisplaySize.x, io.DisplaySize.y);

    // A clip playing means the viewport is showing the pose rather than the
    // document, so an edit made now would be invisible and would look as though
    // nothing had happened.
    if (showing_pose_) {
        end_drag();
        return;
    }

    if (!gizmo_dragging_ && !gizmo_anchor(gizmo_matrix_)) {
        end_drag();
        return;
    }

    // Elements are moved rather than turned or resized for now, and a rotate
    // handle that silently translated would be worse than one that is not
    // offered.
    const ImGuizmo::OPERATION operation =
        selection_.mode() == EditMode::Mesh ? ImGuizmo::TRANSLATE : as_gizmo(gizmo_operation_);

    const math::Mat4 before = gizmo_matrix_;

    ImGuizmo::Manipulate(&view[0][0], &projection[0][0], operation, ImGuizmo::LOCAL,
                         &gizmo_matrix_[0][0]);

    if (!ImGuizmo::IsUsing()) {
        end_drag();
        return;
    }

    if (!gizmo_dragging_) {
        gizmo_dragging_ = true;
        gizmo_start_ = before;

        if (const scene::Node* node = scene_.node(selection_.node()); node != nullptr) {
            node_before_ = node->transform;
        }

        vertex_drag_.clear();
        vertex_drag_inverse_.clear();

        if (selection_.mode() == EditMode::Mesh) {
            if (const geom::EditMesh* mesh = selected_mesh(); mesh != nullptr) {
                // The matrix each vertex is carried into the world by, kept for
                // the length of the gesture. The gizmo says how far it moved in
                // the world, and every vertex needs that answer translated back
                // into the space it is stored in, which on a skinned mesh
                // differs from one vertex to the next.
                const std::vector<math::Mat4> matrices = scene_.vertex_matrices(selection_.node());

                for (const geom::VertexId vertex : selection_.moving_vertices(*mesh)) {
                    vertex_drag_.push_back(
                        {vertex, mesh->position(vertex), mesh->position(vertex)});
                    vertex_drag_inverse_.push_back(vertex.index < matrices.size()
                                                       ? glm::inverse(matrices[vertex.index])
                                                       : math::Mat4{1.0F});
                }
            }
        }
    }

    if (selection_.mode() == EditMode::Object) {
        drag_node(gizmo_matrix_);
    } else {
        drag_vertices(math::Vec3{gizmo_matrix_[3]} - math::Vec3{gizmo_start_[3]});
    }
}

void Application::drag_node(const math::Mat4& world) {
    scene::Node* node = scene_.node(selection_.node());
    if (node == nullptr) {
        return;
    }

    // The gizmo works in world space and the node stores its own. Dividing out
    // the parent is what keeps a joint moving with its parent rather than
    // jumping to wherever the handle happened to be.
    math::Mat4 local = world;
    if (node->parent.valid()) {
        local = glm::inverse(scene_.world_transform(node->parent)) * world;
    }

    math::Vec3 scale{1.0F, 1.0F, 1.0F};
    math::Quat rotation{1.0F, 0.0F, 0.0F, 0.0F};
    math::Vec3 translation{0.0F, 0.0F, 0.0F};
    math::Vec3 skew{0.0F, 0.0F, 0.0F};
    math::Vec4 perspective{0.0F, 0.0F, 0.0F, 1.0F};

    if (!glm::decompose(local, scale, rotation, translation, skew, perspective)) {
        return;
    }

    // Written straight into the scene for the length of the gesture, so the
    // model follows the pointer. The command is pushed when the drag ends.
    node->transform.translation = translation;
    node->transform.rotation = rotation;
    node->transform.scale = scale;
}

void Application::drag_vertices(const math::Vec3& world_delta) {
    geom::EditMesh* mesh = selected_mesh();
    if (mesh == nullptr || vertex_drag_.empty()) {
        return;
    }

    for (std::size_t index = 0; index < vertex_drag_.size(); ++index) {
        MoveVertices::Moved& moved = vertex_drag_[index];

        // The handle moved this far in the world; this vertex is stored in the
        // space the joints and the node's transform carried it out of, so the
        // movement is carried back through the same matrix.
        const auto local_delta =
            math::Vec3{vertex_drag_inverse_[index] * math::Vec4{world_delta, 0.0F}};

        // From where it started, not from where it is, or the drag would
        // accumulate its own rounding on every frame.
        moved.after = moved.before + local_delta;
        if (mesh->contains(moved.vertex)) {
            mesh->set_position(moved.vertex, moved.after);
        }
    }

    rebuild_render_mesh(selected_mesh_id());
}

void Application::end_drag() {
    if (!gizmo_dragging_) {
        return;
    }

    gizmo_dragging_ = false;

    if (selection_.mode() == EditMode::Object) {
        scene::Node* node = scene_.node(selection_.node());
        if (node == nullptr) {
            return;
        }

        const scene::Transform after = node->transform;

        // Put back, so that executing the command is what applies the change
        // and undo has somewhere to return to.
        node->transform = node_before_;
        commands_.execute(
            std::make_unique<TransformNode>(scene_, selection_.node(), node_before_, after));
        return;
    }

    if (vertex_drag_.empty()) {
        return;
    }

    geom::EditMesh* mesh = selected_mesh();
    if (mesh == nullptr) {
        vertex_drag_.clear();
        vertex_drag_inverse_.clear();
        return;
    }

    // A drag that ended where it began is not an edit, and would otherwise fill
    // the history with steps that do nothing.
    const bool moved =
        std::any_of(vertex_drag_.begin(), vertex_drag_.end(),
                    [](const MoveVertices::Moved& one) { return one.before != one.after; });

    if (!moved) {
        vertex_drag_.clear();
        vertex_drag_inverse_.clear();
        return;
    }

    for (const MoveVertices::Moved& one : vertex_drag_) {
        if (mesh->contains(one.vertex)) {
            mesh->set_position(one.vertex, one.before);
        }
    }

    commands_.execute(std::make_unique<MoveVertices>(scene_, selected_mesh_id(), vertex_drag_));
    vertex_drag_.clear();
    vertex_drag_inverse_.clear();

    rebuild_render_mesh(selected_mesh_id());
}

void Application::extrude_selection() {
    if (selection_.mode() != EditMode::Mesh || selection_.kind() != ElementKind::Face) {
        return;
    }

    const geom::EditMesh* mesh = selected_mesh();
    if (mesh == nullptr || selection_.faces().empty()) {
        return;
    }

    geom::EditMesh before = mesh->clone();
    geom::EditMesh after = mesh->clone();

    const geom::ExtrudeResult result = geom::extrude(after, selection_.faces());
    if (result.caps.empty()) {
        return;
    }

    const scene::MeshId id = selected_mesh_id();
    commands_.execute(
        std::make_unique<ReplaceMesh>(scene_, id, std::move(before), std::move(after), "extrude"));

    // The new faces stand where the old ones did, so the obvious next thing is
    // to move them. Selecting the walls too would drag the shape flat again.
    selection_.clear_elements();
    for (const geom::FaceId face : result.caps) {
        selection_.toggle(face);
    }

    rebuild_render_mesh(id);
}

void Application::update_picking() {
    const ImGuiIO& io = ImGui::GetIO();

    if (io.WantCaptureMouse || gizmo_dragging_ || ImGuizmo::IsOver() || ImGuizmo::IsUsing()) {
        return;
    }

    // What is drawn while a clip plays is the pose, and what would be picked is
    // the document. Rather than select the wrong thing, this waits.
    if (showing_pose_ || !ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        return;
    }

    const ImVec2 drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0F);
    if (std::abs(drag.x) + std::abs(drag.y) > kClickSlop) {
        // That was the camera being moved, not something being chosen.
        return;
    }

    const math::Ray ray = viewport_.camera().ray_through(
        math::Vec2{io.MousePos.x, io.MousePos.y}, math::Vec2{io.DisplaySize.x, io.DisplaySize.y});

    const std::optional<scene::SceneHit> hit = scene::raycast(scene_, ray);

    if (!hit.has_value()) {
        if (!io.KeyShift) {
            selection_.clear_elements();
        }
        return;
    }

    if (selection_.mode() == EditMode::Object || hit->node != selection_.node()) {
        // Clicking a different model in mesh mode moves to it rather than doing
        // nothing, which is less surprising than a click that is ignored.
        selection_.set_node(hit->node);
        return;
    }

    if (selection_.kind() == ElementKind::Vertex) {
        if (io.KeyShift) {
            selection_.toggle(hit->vertex);
        } else {
            selection_.select_only(hit->vertex);
        }
        return;
    }

    if (io.KeyShift) {
        selection_.toggle(hit->face);
    } else {
        selection_.select_only(hit->face);
    }
}

void Application::draw_selection(const math::Mat4& view_projection) const {
    if (selection_.mode() != EditMode::Mesh) {
        return;
    }

    const geom::EditMesh* mesh = selected_mesh();
    if (mesh == nullptr) {
        return;
    }

    // Already in the world, deformed by the same joints the shader used, so the
    // overlay lands on the model rather than on the bind pose it is stored in.
    const std::vector<math::Vec3> positions =
        scene_.world_positions(selection_.node(), active_pose());
    const math::Mat4 model{1.0F};

    const auto position_of = [&](geom::VertexId vertex) {
        return vertex.index < positions.size() ? positions[vertex.index] : mesh->position(vertex);
    };

    if (selection_.kind() == ElementKind::Vertex) {
        std::vector<math::Vec3> all;
        std::vector<math::Vec3> chosen;

        for (const geom::VertexId vertex : mesh->vertices()) {
            (selection_.contains(vertex) ? chosen : all).push_back(position_of(vertex));
        }

        renderer_.draw_points(all, model, view_projection, kVertexColour, kVertexPointSize);
        renderer_.draw_points(chosen, model, view_projection, kSelectedColour, kSelectedPointSize);
        return;
    }

    // Fanned the same way the bake and the raycast fan them, so the highlight
    // covers exactly the triangles that would be hit.
    std::vector<math::Vec3> triangles;
    for (const geom::FaceId face : selection_.faces()) {
        const std::vector<geom::CornerId>* corners = mesh->face_corners(face);
        if (corners == nullptr || corners->size() < 3) {
            continue;
        }

        const math::Vec3 first = position_of(mesh->corner_vertex((*corners)[0]));
        for (std::size_t i = 2; i < corners->size(); ++i) {
            triangles.push_back(first);
            triangles.push_back(position_of(mesh->corner_vertex((*corners)[i - 1])));
            triangles.push_back(position_of(mesh->corner_vertex((*corners)[i])));
        }
    }

    renderer_.draw_overlay(triangles, model, view_projection, kFaceColour);
}

void Application::on_files_dropped(GLFWwindow* window, int count, const char** paths) {
    auto* application = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (application == nullptr || count < 1) {
        return;
    }

    // Only the first file: the editor holds one scene, and loading several in
    // sequence would just leave the last one anyway.
    application->load_scene(std::filesystem::path(paths[0]));
}

void Application::run() {
    while (glfwWindowShouldClose(window_) == GLFW_FALSE) {
        glfwPollEvents();
        draw_frame();
        glfwSwapBuffers(window_);
    }
}

void Application::draw_menu_bar() {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("Window")) {
        // The way back. Without it anyone whose imgui.ini predates the built-in
        // layout, which is anyone who has run the editor before, would never
        // see it.
        if (ImGui::MenuItem("Reset layout")) {
            layout_ready_ = false;
        }
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

// The shape Unity and Blender share, and share because it works: the tree down
// one side, what is selected down the other, the clip along the bottom, and the
// world filling everything left over.
//
// DockBuilder lives in imgui_internal.h because the docking API has not settled
// yet. It is the only way to place windows from code, and every editor built on
// ImGui reaches for it.
void Application::build_default_layout(ImGuiID dockspace) {
    ImGui::DockBuilderRemoveNode(dockspace);
    // Only the DockSpace flag: the passthrough centre is asked for again on
    // every frame by the dockspace itself, so setting it here as well would
    // just be saying it twice.
    ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);

    // Before any split. The header says so, and without it the ratios below are
    // measured against a node of no size and come out wrong.
    ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetMainViewport()->WorkSize);

    ImGuiID centre = dockspace;

    // The clip first, so that it runs the whole width rather than only the part
    // the columns leave behind.
    const ImGuiID bottom =
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, kTimelineHeight, nullptr, &centre);

    const ImGuiID left =
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Left, kLeftColumnWidth, nullptr, &centre);
    const ImGuiID right =
        ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, kRightColumnWidth, nullptr, &centre);

    ImGuiID left_top = left;
    const ImGuiID left_bottom =
        ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, kCameraHeight, nullptr, &left_top);

    ImGuiID right_top = right;
    const ImGuiID right_bottom =
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, kPoseHeight, nullptr, &right_top);

    ImGui::DockBuilderDockWindow("Scene", left_top);
    ImGui::DockBuilderDockWindow("Camera", left_bottom);
    ImGui::DockBuilderDockWindow("Edit", right_top);
    ImGui::DockBuilderDockWindow("Pose", right_bottom);
    ImGui::DockBuilderDockWindow("Timeline", bottom);

    // Whatever is left is the central node, and the world already shows through
    // it because the dockspace was asked for a passthrough centre.
    ImGui::DockBuilderFinish(dockspace);
}

void Application::draw_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    draw_menu_bar();

    // Maximising is a request the window manager grants a frame or two later,
    // so the first frames still measure the window at the size it was created
    // at. The splits below are worked out once and then kept in pixels, so
    // building too early leaves columns sized for a window half as wide.
    //
    // Waiting for two frames that agree is enough, and is a property of the
    // window rather than a number of frames guessed at.
    const ImVec2 work = ImGui::GetMainViewport()->WorkSize;
    const bool settled = work.x > 0.0F && work.x == settled_size_.x && work.y == settled_size_.y;
    settled_size_ = math::Vec2{work.x, work.y};

    // Before the dockspace is submitted, not after. By the time it has been
    // submitted for this frame the panels have already been told where they go,
    // and rearranging the nodes behind them changes nothing they can still
    // read. Getting this backwards leaves the panels floating and looks exactly
    // like a layout that was never written.
    if (!layout_ready_ && settled) {
        build_default_layout(kDockspaceId);
        layout_ready_ = true;
    }

    ImGui::DockSpaceOverViewport(kDockspaceId, ImGui::GetMainViewport(),
                                 ImGuiDockNodeFlags_PassthruCentralNode);

    ImGuizmo::BeginFrame();

    handle_shortcuts();
    update_animation(ImGui::GetIO().DeltaTime);

    {
        // The gizmo goes first, because whether it has the pointer is what
        // decides if the camera and the picking may have it. Asking after the
        // fact would let a drag on a handle orbit the view for one frame.
        const ImGuiIO& io = ImGui::GetIO();
        const float aspect = io.DisplaySize.y > 0.0F ? io.DisplaySize.x / io.DisplaySize.y : 1.0F;

        update_gizmo(viewport_.camera().view(), viewport_.camera().projection(aspect));
    }

    update_camera();
    update_picking();

    draw_camera_panel();
    draw_scene_panel();
    draw_edit_panel();
    draw_pose_panel();
    draw_timeline_panel();

    ImGui::Render();

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);

    renderer_.begin_frame(width, height);

    if (height > 0) {
        const auto& camera = viewport_.camera();
        const float aspect = static_cast<float>(width) / static_cast<float>(height);
        const math::Mat4 view_projection = camera.view_projection(aspect);

        renderer_.draw_grid(view_projection, camera.position(), camera.far_plane());

        for (const scene::NodeId id : scene_.nodes_in_order()) {
            const scene::Node* node = scene_.node(id);
            if (node == nullptr || !node->mesh.valid()) {
                continue;
            }

            const auto found = render_meshes_.find(node->mesh.index);
            if (found == render_meshes_.end()) {
                continue;
            }

            const scene::Pose* pose = active_pose();

            // Once per material rather than once per mesh. The indices of each
            // one are a contiguous run, so this is several calls into a single
            // buffer rather than several buffers.
            //
            // The runs come from what was uploaded rather than from baking the
            // mesh again. Baking here would be work proportional to the model
            // on every frame, to learn something that has not changed since the
            // upload.
            for (const geom::Section& section : found->second.sections()) {
                const render::MaterialDraw material = material_for(section.material);

                if (node->skin.valid() && found->second.skinned()) {
                    // No model matrix: the joints already place the mesh, and
                    // glTF says the transform of the node carrying a skinned
                    // mesh is ignored.
                    renderer_.draw_skinned_mesh(
                        found->second, scene_.joint_matrices(node->skin, pose), view_projection,
                        camera.position(), material, section.first_index, section.index_count);
                } else {
                    renderer_.draw_mesh(found->second, scene_.world_transform(id, pose),
                                        view_projection, camera.position(), material,
                                        section.first_index, section.index_count);
                }
            }
        }

        // Last, so that it sits over the world it is describing.
        draw_selection(view_projection);
    }

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Application::update_camera() {
    const ImGuiIO& io = ImGui::GetIO();

    CameraInput input;
    input.delta = math::Vec2{io.MouseDelta.x, io.MouseDelta.y};
    input.wheel = io.MouseWheel;
    input.left = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    input.middle = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    input.shift = io.KeyShift;

    // The world fills the window behind the dockspace, so it owns the pointer
    // wherever no panel has claimed it, and wherever the gizmo has not: a drag
    // on a handle is an edit, and orbiting at the same time would fight it.
    input.over_world = !io.WantCaptureMouse && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing();

    // Sized in ImGui units rather than framebuffer pixels, to match MouseDelta.
    // Mixing the two would halve the pan speed on a high density display.
    viewport_.update(input, math::Vec2{io.DisplaySize.x, io.DisplaySize.y});
}

void Application::draw_camera_panel() {
    // The world is drawn behind the dockspace rather than into a texture, so
    // this panel only reports what the camera is doing; the whole background is
    // what responds to the pointer. Rendering into an offscreen target, which
    // would make a panel the viewport in its own right, comes with the first
    // pass that needs more than one view.
    if (ImGui::Begin("Camera")) {
        const auto& camera = viewport_.camera();
        ImGui::TextWrapped("Drag to orbit, shift or middle drag to pan, scroll to zoom.");
        ImGui::Separator();
        ImGui::Text("target    %.2f, %.2f, %.2f m", static_cast<double>(camera.target().x),
                    static_cast<double>(camera.target().y), static_cast<double>(camera.target().z));
        ImGui::Text("distance  %.2f m", static_cast<double>(camera.distance()));
        ImGui::Text("%.1f fps", static_cast<double>(ImGui::GetIO().Framerate));
    }
    ImGui::End();
}

void Application::draw_scene_panel() {
    if (ImGui::Begin("Scene")) {
        ImGui::Text("source  %s", source_.c_str());
        ImGui::TextUnformatted("Pass a .gltf or .glb on the command line to open it.");

        // Dropping a file works where the window system delivers the event,
        // which rules out a Wayland session running this through XWayland.
        ImGui::TextUnformatted("Dropping one on the window also works on X11.");

        if (!load_error_.empty()) {
            ImGui::TextColored(ImVec4{0.9F, 0.4F, 0.4F, 1.0F}, "%s", load_error_.c_str());
        }

        ImGui::Separator();

        // What leaves is the scene as authored. A clip being played is a view
        // of the document, so the playhead has no say in what gets written.
        ImGui::SetNextItemWidth(-70.0F);
        ImGui::InputText("##export", export_path_.data(), export_path_.size());
        ImGui::SameLine();

        if (ImGui::Button("Export")) {
            const std::filesystem::path target{export_path_.data()};
            std::string error;
            export_status_ =
                io::export_gltf(target, scene_, clips_, error) ? "wrote " + target.string() : error;
        }

        if (!export_status_.empty()) {
            ImGui::TextWrapped("%s", export_status_.c_str());
        }

        ImGui::Separator();

        for (const scene::NodeId root : scene_.roots()) {
            draw_node(root);
        }

        ImGui::Separator();

        const scene::Node* node = scene_.node(selection_.node());
        const geom::EditMesh* mesh = node != nullptr ? scene_.mesh(node->mesh) : nullptr;

        if (mesh == nullptr) {
            ImGui::TextUnformatted("no mesh selected");
        } else {
            // The ratio between these is the point. Attributes live on corners,
            // so a vertex is shared while its normals and UVs are not, and the
            // bake splits again only what the GPU cannot share.
            ImGui::Text("editable  %zu vertices, %zu corners, %zu faces", mesh->vertex_count(),
                        mesh->corner_count(), mesh->face_count());

            const auto found = render_meshes_.find(node->mesh.index);
            if (found != render_meshes_.end()) {
                ImGui::Text("baked     %zu vertices, %zu indices", found->second.vertex_count(),
                            found->second.index_count());
            }
        }
    }
    ImGui::End();
}

void Application::draw_pose_panel() {
    if (ImGui::Begin("Pose")) {
        scene::Node* node = scene_.node(selection_.node());

        if (node == nullptr) {
            ImGui::TextUnformatted("select a node in the scene tree");
            ImGui::End();
            return;
        }

        ImGui::Text("%s", node->name.empty() ? "(unnamed)" : node->name.c_str());

        if (showing_pose_) {
            // Editing here still changes the document, but the viewport is
            // showing the clip, so the change would not be visible and would
            // look like nothing happened.
            ImGui::TextColored(ImVec4{0.85F, 0.75F, 0.35F, 1.0F},
                               "a clip is playing; stop it to see your pose");
        }

        ImGui::Separator();

        // Euler angles are a poor way to store a rotation and a good way to
        // offer one: the node keeps a quaternion, and these three numbers exist
        // only for as long as the control is on screen.
        if (!pose_dragging_) {
            pose_euler_ = glm::degrees(glm::eulerAngles(node->transform.rotation));
        }

        const bool changed = ImGui::DragFloat3("rotation", &pose_euler_.x, 0.5F);

        if (changed) {
            if (!pose_dragging_) {
                // Remember where the joint was before the gesture began, so the
                // command can put it back there in one step.
                pose_before_ = node->transform.rotation;
                pose_dragging_ = true;
            }

            // Written directly for the duration of the drag, so the mesh follows
            // the pointer; the command is what makes it permanent.
            node->transform.rotation = math::Quat(glm::radians(pose_euler_));
        }

        // One command per gesture. Pushing one per frame would make undo
        // useless, since a single drag would need hundreds of presses to walk
        // back.
        if (pose_dragging_ && ImGui::IsItemDeactivatedAfterEdit()) {
            const scene::Transform after = node->transform;

            scene::Transform before = node->transform;
            before.rotation = pose_before_;
            node->transform = before;

            commands_.execute(
                std::make_unique<TransformNode>(scene_, selection_.node(), before, after));
            pose_dragging_ = false;
        } else if (pose_dragging_ && !ImGui::IsItemActive()) {
            pose_dragging_ = false;
        }

        // The history used to be repeated here as well. It belongs to the
        // session rather than to whichever node is selected, so it lives in the
        // Edit panel and only there.
    }
    ImGui::End();
}

const scene::Pose* Application::active_pose() const {
    return showing_pose_ ? &pose_ : nullptr;
}

void Application::update_animation(float delta_seconds) {
    if (clip_index_ >= clips_.size()) {
        showing_pose_ = false;
        return;
    }

    player_.advance(delta_seconds);

    // The pose is rebuilt whenever the playhead is somewhere other than the
    // start, so scrubbing shows the clip even while paused. Back at zero with
    // nothing playing, the pose is dropped and the scene speaks for itself,
    // which is how the user's hand-made pose comes back.
    const bool showing = player_.playing() || player_.time() > 0.0F;

    if (!showing) {
        showing_pose_ = false;
        pose_.clear();
        return;
    }

    pose_.clear();
    anim::evaluate(clips_[clip_index_], player_.time(), scene_, pose_);
    showing_pose_ = true;
}

void Application::draw_timeline_panel() {
    if (ImGui::Begin("Timeline")) {
        if (clips_.empty()) {
            ImGui::TextUnformatted("this file carries no animation");
            ImGui::End();
            return;
        }

        const anim::Clip& clip = clips_[clip_index_];

        if (ImGui::BeginCombo("clip", clip.name.empty() ? "(unnamed)" : clip.name.c_str())) {
            for (std::size_t index = 0; index < clips_.size(); ++index) {
                const bool selected = index == clip_index_;
                const char* label =
                    clips_[index].name.empty() ? "(unnamed)" : clips_[index].name.c_str();

                if (ImGui::Selectable(label, selected)) {
                    clip_index_ = index;
                    player_.stop();
                    player_.set_duration(clips_[index].duration);
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button(player_.playing() ? "Pause" : "Play")) {
            if (player_.playing()) {
                player_.pause();
            } else {
                player_.play();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            player_.stop();
        }
        ImGui::SameLine();
        bool looping = player_.looping();
        if (ImGui::Checkbox("loop", &looping)) {
            player_.set_looping(looping);
        }

        float time = player_.time();
        if (ImGui::SliderFloat("time", &time, 0.0F, player_.duration(), "%.3f s")) {
            player_.set_time(time);
        }

        float speed = player_.speed();
        if (ImGui::DragFloat("speed", &speed, 0.01F, -4.0F, 4.0F, "%.2fx")) {
            player_.set_speed(speed);
        }

        ImGui::Separator();
        ImGui::Text("%zu channels over %.2f s", clip.channels.size(),
                    static_cast<double>(clip.duration));

        if (showing_pose_) {
            ImGui::TextColored(ImVec4{0.85F, 0.75F, 0.35F, 1.0F},
                               "showing the clip, not the authored pose");
        }
    }
    ImGui::End();
}

void Application::handle_shortcuts() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) {
        return;
    }

    if (io.KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            if (io.KeyShift) {
                redo();
            } else {
                undo();
            }
        } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
            redo();
        }
        return;
    }

    // Nothing below should fire while a handle is being dragged, or a key press
    // would change what the drag means halfway through it.
    if (gizmo_dragging_) {
        return;
    }

    // Shift and E rather than E on its own, which the gizmo already has.
    if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_E, false)) {
        extrude_selection();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
        gizmo_operation_ = GizmoOperation::Translate;
    } else if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
        gizmo_operation_ = GizmoOperation::Rotate;
    } else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
        gizmo_operation_ = GizmoOperation::Scale;
    } else if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
        // Tab is also how ImGui walks between widgets, which is why this only
        // runs when no field has the keyboard.
        selection_.set_mode(selection_.mode() == EditMode::Object ? EditMode::Mesh
                                                                  : EditMode::Object);
    } else if (ImGui::IsKeyPressed(ImGuiKey_1, false)) {
        selection_.set_kind(ElementKind::Vertex);
    } else if (ImGui::IsKeyPressed(ImGuiKey_2, false)) {
        selection_.set_kind(ElementKind::Face);
    }
}

void Application::undo() {
    if (!commands_.undo()) {
        return;
    }

    // The elements that were selected may have just stopped existing, and a
    // count that still claimed them would be lying.
    selection_.clear_elements();
    rebuild_render_meshes();
}

void Application::redo() {
    if (!commands_.redo()) {
        return;
    }

    selection_.clear_elements();
    rebuild_render_meshes();
}

void Application::draw_edit_panel() {
    if (ImGui::Begin("Edit")) {
        int mode = selection_.mode() == EditMode::Object ? 0 : 1;
        ImGui::TextUnformatted("mode");
        ImGui::SameLine();
        if (ImGui::RadioButton("object", &mode, 0) || ImGui::RadioButton("mesh", &mode, 1)) {
            selection_.set_mode(mode == 0 ? EditMode::Object : EditMode::Mesh);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Tab)");

        ImGui::Separator();

        if (selection_.mode() == EditMode::Object) {
            int operation = static_cast<int>(gizmo_operation_);
            ImGui::TextUnformatted("gizmo");
            ImGui::SameLine();
            if (ImGui::RadioButton("move", &operation, 0) ||
                ImGui::RadioButton("turn", &operation, 1) ||
                ImGui::RadioButton("size", &operation, 2)) {
                gizmo_operation_ = static_cast<GizmoOperation>(operation);
            }
            ImGui::TextDisabled("W, E, R");

            const scene::Node* node = scene_.node(selection_.node());
            if (node != nullptr && node->skin.valid()) {
                // Otherwise the handle moves and the model does not, which
                // looks like a broken gizmo rather than a rule of the format.
                ImGui::TextColored(ImVec4{0.85F, 0.75F, 0.35F, 1.0F},
                                   "this node carries a skinned mesh, so its transform is\n"
                                   "ignored when drawing. Move its joints instead.");
            }
        } else {
            int kind = selection_.kind() == ElementKind::Vertex ? 0 : 1;
            ImGui::TextUnformatted("pick");
            ImGui::SameLine();
            if (ImGui::RadioButton("vertices", &kind, 0) || ImGui::RadioButton("faces", &kind, 1)) {
                selection_.set_kind(kind == 0 ? ElementKind::Vertex : ElementKind::Face);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(1, 2)");

            const geom::EditMesh* mesh = selected_mesh();
            if (mesh == nullptr) {
                ImGui::TextUnformatted("the selected node has no mesh");
            } else if (selection_.kind() == ElementKind::Vertex) {
                ImGui::Text("%zu of %zu vertices", selection_.vertices().size(),
                            mesh->vertex_count());
            } else {
                ImGui::Text("%zu of %zu faces", selection_.faces().size(), mesh->face_count());

                ImGui::BeginDisabled(selection_.faces().empty());
                if (ImGui::Button("Extrude")) {
                    extrude_selection();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::TextDisabled("(Shift+E)");
            }

            ImGui::TextUnformatted("Click to pick, shift click to add.");
        }

        ImGui::Separator();
        ImGui::Text("history  %zu", commands_.depth());
        ImGui::BeginDisabled(!commands_.can_undo());
        if (ImGui::Button("Undo")) {
            undo();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!commands_.can_redo());
        if (ImGui::Button("Redo")) {
            redo();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextUnformatted("Ctrl+Z, Ctrl+Y");
    }
    ImGui::End();
}

void Application::draw_node(scene::NodeId id) {
    const scene::Node* node = scene_.node(id);
    if (node == nullptr) {
        return;
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (node->children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (id == selection_.node()) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    const char* label = node->name.empty() ? "(unnamed)" : node->name.c_str();
    const bool open = ImGui::TreeNodeEx(static_cast<const void*>(&node->name), flags, "%s", label);

    if (ImGui::IsItemClicked()) {
        selection_.set_node(id);
    }

    if (open) {
        for (const scene::NodeId child : node->children) {
            draw_node(child);
        }
        ImGui::TreePop();
    }
}

void Application::shutdown() {
    if (imgui_ready_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        imgui_ready_ = false;
    }

    // Before the window goes, because these hold GPU objects and deleting one
    // needs a current context. They are members, so without this they would be
    // destroyed after this function returns, with nothing left to delete them
    // against.
    //
    // Every cache of device objects belongs in this list. Forgetting one is a
    // crash on exit and nowhere else, which is why it goes unnoticed until
    // somebody closes the window properly.
    render_meshes_.clear();
    textures_.clear();

    renderer_.shutdown();

    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    glfwTerminate();
}

}  // namespace kinetiqra::app
