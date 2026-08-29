#include <kinetiqra/app/Application.hpp>

#include <cstdio>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
    // One optional argument: a model to open. Without it the editor starts on a
    // built-in box, and a file can still be dropped on the window.
    std::filesystem::path model;
    if (argc > 1) {
        model = argv[1];
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
