#include <kinetiqra/app/Application.hpp>
#include <kinetiqra/app/PoseCommand.hpp>
#include <kinetiqra/geom/Bake.hpp>
#include <kinetiqra/geom/Primitives.hpp>
#include <kinetiqra/io/gltf/GltfImport.hpp>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <cstdio>
#include <memory>

namespace kinetiqra::app {

namespace {

constexpr int kInitialWidth = 1280;
constexpr int kInitialHeight = 800;

void report_glfw_error(int code, const char* description) {
    std::fprintf(stderr, "glfw error %d: %s\n", code, description);
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

    window_ = glfwCreateWindow(kInitialWidth, kInitialHeight, "kinetiqra", nullptr, nullptr);
    if (window_ == nullptr) {
        // Almost always a driver without 4.5 core, which is worth saying plainly
        // rather than letting the first GL call fail somewhere further in.
        error = "could not create a window with an OpenGL 4.5 core context";
        return false;
    }

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

    // A box built in code, so the editor opens on something rather than on an
    // empty grid.
    const scene::MeshId mesh = scene_.add_mesh(geom::make_box());
    const scene::NodeId node = scene_.add_node("box");
    scene_.set_mesh(node, mesh);
    scene_.node(node)->transform.translation = math::Vec3{0.0F, 0.5F, 0.0F};

    commands_.clear();
    source_ = "built-in box";
    load_error_.clear();
    selected_ = node;
    rebuild_render_meshes();
}

void Application::load_scene(const std::filesystem::path& path) {
    std::string error;
    if (!io::import_gltf(path, scene_, error)) {
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
    selected_ = scene_.roots().empty() ? scene::NodeId{} : scene_.roots().front();
    rebuild_render_meshes();

    std::printf("loaded %s: %zu nodes, %zu meshes\n", source_.c_str(), scene_.node_count(),
                scene_.mesh_count());
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

void Application::draw_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                                 ImGuiDockNodeFlags_PassthruCentralNode);

    handle_shortcuts();
    update_camera();
    draw_camera_panel();
    draw_scene_panel();
    draw_pose_panel();

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

            if (node->skin.valid() && found->second.skinned()) {
                // No model matrix: the joints already place the mesh, and glTF
                // says the transform of the node carrying a skinned mesh is
                // ignored.
                renderer_.draw_skinned_mesh(found->second, scene_.joint_matrices(node->skin),
                                            view_projection, camera.position());
            } else {
                renderer_.draw_mesh(found->second, scene_.world_transform(id), view_projection,
                                    camera.position());
            }
        }
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
    // wherever no panel has claimed it.
    input.over_world = !io.WantCaptureMouse;

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
        ImGui::TextUnformatted("Drag to orbit, shift or middle drag to pan, scroll to zoom.");
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

        for (const scene::NodeId root : scene_.roots()) {
            draw_node(root);
        }

        ImGui::Separator();

        const scene::Node* node = scene_.node(selected_);
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
        scene::Node* node = scene_.node(selected_);

        if (node == nullptr) {
            ImGui::TextUnformatted("select a node in the scene tree");
            ImGui::End();
            return;
        }

        ImGui::Text("%s", node->name.empty() ? "(unnamed)" : node->name.c_str());
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
            const math::Quat after = node->transform.rotation;
            node->transform.rotation = pose_before_;
            commands_.execute(
                std::make_unique<RotateJoint>(scene_, selected_, pose_before_, after));
            pose_dragging_ = false;
        } else if (pose_dragging_ && !ImGui::IsItemActive()) {
            pose_dragging_ = false;
        }

        ImGui::Separator();
        ImGui::Text("history  %zu", commands_.depth());
        ImGui::BeginDisabled(!commands_.can_undo());
        if (ImGui::Button("Undo")) {
            commands_.undo();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!commands_.can_redo());
        if (ImGui::Button("Redo")) {
            commands_.redo();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextUnformatted("Ctrl+Z, Ctrl+Y");
    }
    ImGui::End();
}

void Application::handle_shortcuts() {
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyCtrl || io.WantTextInput) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (io.KeyShift) {
            commands_.redo();
        } else {
            commands_.undo();
        }
    } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        commands_.redo();
    }
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
    if (id == selected_) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    const char* label = node->name.empty() ? "(unnamed)" : node->name.c_str();
    const bool open = ImGui::TreeNodeEx(static_cast<const void*>(&node->name), flags, "%s", label);

    if (ImGui::IsItemClicked()) {
        selected_ = id;
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

    renderer_.shutdown();

    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    glfwTerminate();
}

}  // namespace kinetiqra::app
