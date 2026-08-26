#include <kinetiqra/app/Application.hpp>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <cstdio>

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

bool Application::initialise(std::string& error) {
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

    std::printf("kinetiqra %s\n", KINETIQRA_VERSION);
    std::printf("  renderer: %s\n", renderer_.driver_description().c_str());

    return true;
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

    update_camera();
    draw_camera_panel();

    ImGui::Render();

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);

    renderer_.begin_frame(width, height);

    if (height > 0) {
        const auto& camera = viewport_.camera();
        const float aspect = static_cast<float>(width) / static_cast<float>(height);
        renderer_.draw_grid(camera.view_projection(aspect), camera.position(), camera.far_plane());
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
