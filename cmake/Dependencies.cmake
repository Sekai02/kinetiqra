# Third-party dependencies, resolved by vcpkg (see vcpkg.json).
#
# Every find_package lives here rather than in module CMakeLists so that the
# module files stay a pure statement of the dependency graph.

find_package(glfw3 CONFIG REQUIRED)      # target: glfw
find_package(glm CONFIG REQUIRED)        # target: glm::glm
find_package(glad CONFIG REQUIRED)       # target: glad::glad
find_package(imgui CONFIG REQUIRED)      # target: imgui::imgui
find_package(imguizmo CONFIG REQUIRED)   # target: imguizmo::imguizmo
find_package(fastgltf CONFIG REQUIRED)   # target: fastgltf::fastgltf

# stb ships headers only and exports no CMake config, so wrap it in an imported
# interface target to keep consumers uniform.
find_path(STB_INCLUDE_DIRS "stb_image.h" REQUIRED)

if(NOT TARGET stb::stb)
    add_library(stb::stb INTERFACE IMPORTED)
    target_include_directories(stb::stb INTERFACE "${STB_INCLUDE_DIRS}")
endif()
