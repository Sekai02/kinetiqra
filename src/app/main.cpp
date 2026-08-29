#include <kinetiqra/anim/Clip.hpp>
#include <kinetiqra/app/Application.hpp>
#include <kinetiqra/io/gltf/GltfExport.hpp>
#include <kinetiqra/io/gltf/GltfImport.hpp>
#include <kinetiqra/scene/Scene.hpp>

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

void usage() {
    std::fprintf(stderr, "usage: kinetiqra [model.gltf|model.glb] [--export out.gltf|out.glb]\n");
}

// Reads a model and writes it out again without opening a window or a GL
// context.
//
// The importer's last two real bugs turned up in files rather than in tests, so
// being able to put a folder of models through the round trip from a shell is
// worth more than the few lines it costs.
int convert(const std::filesystem::path& input, const std::filesystem::path& output) {
    kinetiqra::scene::Scene scene;
    std::vector<kinetiqra::anim::Clip> clips;
    std::string error;

    if (!kinetiqra::io::import_gltf(input, scene, error, &clips)) {
        std::fprintf(stderr, "kinetiqra: %s\n", error.c_str());
        return 1;
    }

    // The geometry counts are the point of printing anything at all: running a
    // model out and back in again and comparing these two lines is what catches
    // a mesh that lost a face or a rig on the way through.
    std::size_t vertices = 0;
    std::size_t faces = 0;
    for (const kinetiqra::scene::MeshId id : scene.meshes()) {
        const kinetiqra::geom::EditMesh* mesh = scene.mesh(id);
        vertices += mesh->vertex_count();
        faces += mesh->face_count();
    }

    std::printf("read  %s: %zu nodes, %zu meshes, %zu vertices, %zu faces, %zu clips\n",
                input.string().c_str(), scene.node_count(), scene.mesh_count(), vertices, faces,
                clips.size());

    if (!kinetiqra::io::export_gltf(output, scene, clips, error)) {
        std::fprintf(stderr, "kinetiqra: %s\n", error.c_str());
        return 1;
    }

    std::printf("wrote %s\n", output.string().c_str());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    // One optional model to open, and an optional destination. Without either
    // the editor starts on a built-in box, and a file can still be dropped on
    // the window.
    std::filesystem::path model;
    std::filesystem::path output;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];

        if (argument == "--export") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "kinetiqra: --export needs a path\n");
                usage();
                return 1;
            }
            output = argv[++i];
            continue;
        }

        if (argument.rfind("--", 0) == 0 || !model.empty()) {
            std::fprintf(stderr, "kinetiqra: unexpected argument '%s'\n", argument.c_str());
            usage();
            return 1;
        }

        model = argument;
    }

    if (!output.empty()) {
        if (model.empty()) {
            std::fprintf(stderr, "kinetiqra: --export needs a model to read\n");
            usage();
            return 1;
        }
        return convert(model, output);
    }

    kinetiqra::app::Application application;

    std::string error;
    if (!application.initialise(model, error)) {
        std::fprintf(stderr, "kinetiqra: %s\n", error.c_str());
        return 1;
    }

    application.run();
    return 0;
}
