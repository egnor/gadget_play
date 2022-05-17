#include <libcamera/libcamera.h>

#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>
#include <fmt/core.h>

int main(int argc, char** argv) {
    std::string camera_arg;

    CLI::App app("Exercise libcamera");
    app.add_option("--camera", camera_arg, "Camera name to use");
    CLI11_PARSE(app, argc, argv);

    auto const manager = std::make_unique<libcamera::CameraManager>();
    manager->start();

    if (manager->cameras().empty()) {
        fmt::print("*** No cameras available\n");
        return 1;
    }

    fmt::print("\n=== Cameras ===\n");
    std::shared_ptr<libcamera::Camera> found;
    for (auto const& cam : manager->cameras()) {
        if (!found && cam->id().find(camera_arg) != std::string::npos)
            found = cam;
        fmt::print("{} {}\n", (found == cam) ? "=>" : "  ", cam->id());
    }
    fmt::print("\n");

    if (!found) {
        fmt::print("*** No cameras matching \"{}\"\n", camera_arg);
        return 1;
    }

    return 0;
}
