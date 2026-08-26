#include <kinetiqra/app/Application.hpp>

#include <cstdio>
#include <string>

int main() {
    kinetiqra::app::Application application;

    std::string error;
    if (!application.initialise(error)) {
        std::fprintf(stderr, "kinetiqra: %s\n", error.c_str());
        return 1;
    }

    application.run();
    return 0;
}
