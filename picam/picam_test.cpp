#include <sys/mman.h>

#include <cmath>
#include <memory>

#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>
#include <fmt/core.h>
#include <libcamera/libcamera.h>
#include <moodycamel/blockingconcurrentqueue.h>

#include "checks.h"
#include "save_png.h"

static std::map<std::string, std::string> controls_text(
    libcamera::ControlList const& controls
) {
    std::map<std::string, std::string> text;
    for (auto const& [n, value] : controls) {
        auto const& control = controls.idMap()->at(n);
        text[control->name()] = value.toString();
    }
    return text;
}

int main(int argc, char** argv) {
    struct Context {
        std::unique_ptr<libcamera::CameraManager> manager;
        std::unique_ptr<libcamera::FrameBufferAllocator> allocator;
        std::shared_ptr<libcamera::Camera> camera;
        std::vector<std::unique_ptr<libcamera::Request>> requests;
        moodycamel::BlockingConcurrentQueue<libcamera::Request*> done_q;

        ~Context() {
            if (camera) camera->stop();
            if (camera) camera->release();
            camera.reset();
            allocator.reset();
            if (manager) manager->stop();
        }
    } cx;

    std::string camera_arg;
    libcamera::Size size_arg = {640, 480};
    std::string format_arg = "BGR888";
    int buffers_arg = 2;
    int list_frames_arg = 3;
    int save_frames_arg = 0;
    float fps_arg = 0.0;
    libcamera::Rectangle crop_arg = {-1, -1, 0, 0};
    int transform_arg = -1;
    float man_gain_arg = 0.0;
    float man_shutter_arg = 0.0;
    float man_red_arg = 0.0;
    float man_blue_arg = 0.0;

    CLI::App app("Exercise libcamera");
    app.add_option("--camera", camera_arg, "Camera name to use");
    app.add_option("--width", size_arg.width, "Capture X pixel size");
    app.add_option("--height", size_arg.height, "Capture Y pixel size");
    app.add_option("--format", format_arg, "Capture pixel format");
    app.add_option("--buffers", buffers_arg, "Image buffers to allocate");
    app.add_option("--list_frames", list_frames_arg, "List (and skip) frames");
    app.add_option("--save_frames", save_frames_arg, "Save as frame####.png");
    app.add_option("--fps", fps_arg, "Frame rate to specify");
    app.add_option("--crop_x", crop_arg.x, "Sensor region left");
    app.add_option("--crop_y", crop_arg.y, "Sensor region top");
    app.add_option("--crop_width", crop_arg.width, "Sensor region width");
    app.add_option("--crop_height", crop_arg.height, "Sensor region height");
    app.add_option("--transform", transform_arg, "Image flip/rot bits, 0-7");
    app.add_option("--manual_gain", man_gain_arg, "Manual analog gain");
    app.add_option("--manual_shutter", man_shutter_arg, "Manual exposure time");
    app.add_option("--manual_red", man_red_arg, "Manual red gain");
    app.add_option("--manual_blue", man_blue_arg, "Manual blue gain");
    CLI11_PARSE(app, argc, argv);

    cx.manager = std::make_unique<libcamera::CameraManager>();
    cx.manager->start();
    fmt::print("\n=== Cameras (libcamera {}) ===\n", cx.manager->version());
    CHECK_RUNTIME(!cx.manager->cameras().empty(), "No cameras available");

    for (auto const& cam : cx.manager->cameras()) {
        if (!cx.camera && cam->id().find(camera_arg) != std::string::npos)
            cx.camera = cam;
        fmt::print("{} {}\n", (cx.camera == cam) ? "=>" : "  ", cam->id());
    }
    CHECK_RUNTIME(cx.camera, "No cameras match: {}", camera_arg);

    fmt::print("\n=== Acquiring camera ({}) ===\n", cx.camera->id());
    CHECK_RUNTIME(!cx.camera->acquire(), "Acquire failed: {}", cx.camera->id());

    fmt::print("\n--- Role recommendations and supported formats ---\n");
    for (auto const& [name, role] : {
        std::pair{"Viewfinder", libcamera::Viewfinder},
        std::pair{"StillCapture", libcamera::StillCapture},
        std::pair{"VideoRecording", libcamera::VideoRecording},
        std::pair{"Raw", libcamera::Raw},
    }) {
        auto const config = cx.camera->generateConfiguration({role});
        if (!config || config->empty()) continue;

        auto const& sconfig = (*config)[0];
        fmt::print(
            "{:<15} {:<18} {:>4}kB/f {:>4}B/l {}buf\n", name,
            sconfig.toString(), sconfig.frameSize / 1024,
            sconfig.stride, sconfig.bufferCount
        );
        if (role == libcamera::Raw || role == libcamera::VideoRecording) {
            for (auto const& pixelformat : sconfig.formats().pixelformats()) {
                fmt::print(
                    "  {:<15} {}\n", pixelformat.toString(),
                    sconfig.formats().range(pixelformat).toString()
                );
            }
        }
    }

    auto config = cx.camera->generateConfiguration({libcamera::VideoRecording});
    CHECK_RUNTIME(!config->empty(), "No stream for VideoRecording role");

    if (transform_arg >= 0) {
        CHECK_ARG(transform_arg < 8, "Transform ({}) not 0-7", transform_arg);
        config->transform = (libcamera::Transform) transform_arg;
    } else {
        config->transform = libcamera::transformFromRotation(
            cx.camera->properties().get(libcamera::properties::Rotation)
        );
    }

    auto* sconfig = &(*config)[0];
    sconfig->size = size_arg;
    sconfig->pixelFormat = libcamera::PixelFormat::fromString(format_arg);
    sconfig->bufferCount = buffers_arg;
    fmt::print(
        "\n=== Configuring camera ({} tf={} buf={}) ===\n",
        sconfig->toString(), transformToString(config->transform),
        sconfig->bufferCount
    );
    CHECK_ARG(sconfig->pixelFormat.isValid(), "Bad format: {}", format_arg);

    CHECK_RUNTIME(
        config->validate() != libcamera::CameraConfiguration::Invalid,
        "Invalid configuration: {}", sconfig->toString()
    );
    fmt::print(
        "Validated: {} tf={} buf={} {:>4}kB/f {:>4}B/l\n", sconfig->toString(),
        transformToString(config->transform), sconfig->bufferCount,
        sconfig->frameSize / 1024, sconfig->stride
    );

    if (save_frames_arg) {
        CHECK_ARG(
            sconfig->pixelFormat == libcamera::formats::BGR888,
            "Bad pixel format ({}) for --save_frames, need BGR888",
            sconfig->pixelFormat.toString()
        );
    }

    fmt::print("\n");  // Blank line before configure()'s log messages
    CHECK_RUNTIME(!cx.camera->configure(config.get()), "Configure failed");

    auto* stream = sconfig->stream();
    CHECK_RUNTIME(stream, "No stream after configuration");

    fmt::print("\n--- Camera properties ---\n");
    for (auto const& [name, value] : controls_text(cx.camera->properties()))
        fmt::print("{} = {}\n", name, value);

    fmt::print("\n--- Dynamic controls available ---\n");
    std::map<std::string, std::string> info_text;
    for (auto const& [id, info] : cx.camera->controls())
        info_text[id->name()] = info.toString();
    for (auto const& [name, info] : info_text)
        fmt::print("{}: {}\n", name, info);

    libcamera::ControlList camera_controls{libcamera::controls::controls};
    if (fps_arg > 0.0) {
        int64_t ft = 1000000 / fps_arg;
        camera_controls.set(libcamera::controls::FrameDurationLimits, {ft, ft});
    }
    if (man_gain_arg > 0.0 && man_shutter_arg > 0.0)
        camera_controls.set(libcamera::controls::AeEnable, false);
    if (man_gain_arg > 0.0) {
        camera_controls.set(libcamera::controls::AnalogueGain, man_gain_arg);
        camera_controls.set(libcamera::controls::DigitalGain, 1.0);
    }
    if (man_shutter_arg > 0.0) {
        int const micros = std::round(man_shutter_arg * 1e6f);
        camera_controls.set(libcamera::controls::ExposureTime, micros);
    }
    if (man_red_arg > 0.0 || man_blue_arg > 0.0) {
        float const red = man_red_arg, blue = man_blue_arg;
        camera_controls.set(libcamera::controls::AwbEnable, false);
        camera_controls.set(libcamera::controls::ColourGains, {red, blue});
    }

    if (
        crop_arg.x >= 0 || crop_arg.y >= 0 ||
        crop_arg.width > 0 || crop_arg.height > 0
    ) {
        auto const& ps = cx.camera->properties();
        auto const& crop_max = ps.get(libcamera::properties::ScalerCropMaximum);
        if (crop_arg.x < 0) crop_arg.x = crop_max.x;
        if (crop_arg.y < 0) crop_arg.y = crop_max.y;
        if (crop_arg.width <= 0) crop_arg.width = crop_max.width;
        if (crop_arg.height <= 0) crop_arg.height = crop_max.height;
        camera_controls.set(libcamera::controls::ScalerCrop, crop_arg);
    }

    if (!camera_controls.empty()) {
        fmt::print("\n--- Dynamic controls set ---\n");
        for (auto const& [name, value] : controls_text(camera_controls))
            fmt::print("{} = {}\n", name, value);
    }

    cx.allocator = std::make_unique<libcamera::FrameBufferAllocator>(cx.camera);
    CHECK_RUNTIME(cx.allocator->allocate(stream) > 0, "Buffer alloc failed");

    auto const& buffers = cx.allocator->buffers(stream);
    std::vector<std::shared_ptr<uint8_t const>> mmaps;
    for (auto const& buffer : buffers) {
        auto request = cx.camera->createRequest(cx.requests.size());
        CHECK_RUNTIME(!request->addBuffer(stream, buffer.get()), "Add failed");
        cx.requests.push_back(std::move(request));

        if (save_frames_arg && buffer->planes().size() > 0) {
            auto const& plane = buffer->planes()[0];
            int const size = plane.length, fd = plane.fd.get();
            void* vmem = ::mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
            CHECK_RUNTIME(vmem != MAP_FAILED, "Buffer mapping failed");
            mmaps.emplace_back(
                (uint8_t const*) vmem,
                [size](uint8_t const* m) { ::munmap((void*) m, size); }
            );
        }
    }

    fmt::print("\n=== Starting camera ===\n");
    CHECK_RUNTIME(!cx.camera->start(&camera_controls), "Camera start failed");
    cx.camera->requestCompleted.connect(
        &cx, [&cx](libcamera::Request* r) { cx.done_q.enqueue(r); }
    );

    for (auto const& request : cx.requests)
        cx.camera->queueRequest(request.get());

    auto const total_frames = list_frames_arg + save_frames_arg;
    fmt::print("--- Receiving {} frames ---\n", total_frames);
    for (int f = 0; f < total_frames; ++f) {
        libcamera::Request* done;
        cx.done_q.wait_dequeue(done);
        CHECK_RUNTIME(
            done->status() == libcamera::Request::RequestComplete,
            "Request #{} incomplete", done->sequence()
        );

        auto const* buf = done->findBuffer(stream);
        CHECK_RUNTIME(
            buf->metadata().status == libcamera::FrameMetadata::FrameSuccess,
            "Frame #{} failed", buf->metadata().sequence
        );

        if (done->sequence()) fmt::print("\n");
        fmt::print(
            "Req #{:<4} Seq #{:<4} {:.3f}s", done->sequence(),
            buf->metadata().sequence, buf->metadata().timestamp * 1e-9
        );

        for (size_t p = 0; p < buf->metadata().planes().size(); ++p) {
            auto const& plane = buf->metadata().planes()[p];
            fmt::print("{}{}", p ? " + " : " (", plane.bytesused);
        }
        fmt::print("B)\n");

        for (auto const& [name, value] : controls_text(done->controls()))
            fmt::print("  (control) {} = {}\n", name, value);
        for (auto const& [name, value] : controls_text(done->metadata()))
            fmt::print("  {} = {}\n", name, value);

        if (f >= list_frames_arg) {
            auto const fn = fmt::format("frame{:04d}.png", f - list_frames_arg);
            fmt::print(">>> Saving: {}\n", fn);
            save_png(
                sconfig->size.width, sconfig->size.height, sconfig->stride,
                mmaps[done->cookie()].get(), fn
            );
        }

        done->reuse(libcamera::Request::ReuseBuffers);
        cx.camera->queueRequest(done);
    }

    fmt::print("\n=== Stopping and releasing camera ===\n");
    return 0;
}
