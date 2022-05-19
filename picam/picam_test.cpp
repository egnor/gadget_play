#include <stdio.h>
#include <sys/mman.h>

#include <cmath>
#include <memory>

#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>
#include <fmt/core.h>
#include <libcamera/libcamera.h>
#include <moodycamel/blockingconcurrentqueue.h>
#include <png.h>

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

static void save_png(
    libcamera::StreamConfiguration const& sconfig,
    std::shared_ptr<uint8_t const> const& mmap, int index
) {
    auto const filename = fmt::format("frame{:04d}.png", index);
    fmt::print(">>> Saving: {}\n", filename);
    if (sconfig.pixelFormat != libcamera::formats::BGR888) {
        fmt::print("*** Bad format ({})\n", sconfig.pixelFormat.toString());
        return;
    }

    FILE* fp = fopen(filename.c_str(), "wb");
    if (!fp) {
        fmt::print("*** File open failed\n");
        return;
    }

    std::vector<png_byte const*> rows;
    auto* png = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    auto* png_info = png_create_info_struct(png);
    if (setjmp(png_jmpbuf(png))) {
        fmt::print("*** PNG write failed\n");
    } else {
        png_set_IHDR(
            png, png_info, sconfig.size.width, sconfig.size.height,
            8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT
        );
        png_set_filter(png, 0, PNG_FILTER_AVG);
        png_set_compression_level(png, 1);

        for (size_t y = 0; y < sconfig.size.height; ++y)
            rows.push_back(mmap.get() + y * sconfig.stride);

        png_init_io(png, fp);
        png_set_rows(png, png_info, const_cast<png_bytepp>(rows.data()));
        png_write_png(png, png_info, PNG_TRANSFORM_IDENTITY, 0);
    }

    png_destroy_write_struct(&png, &png_info);
    fclose(fp);
}

int main(int argc, char** argv) {
    std::string camera_arg;
    libcamera::Size size_arg = {640, 480};
    std::string format_arg = "BGR888";
    int buffers_arg = 4;
    int list_frames_arg = 5;
    int save_frames_arg = 0;
    float fps_arg = 0.0;
    float man_gain_arg = 0.0;
    float man_shutter_arg = 0.0;
    float man_red_arg = 0.0;
    float man_blue_arg = 0.0;

    CLI::App app("Exercise libcamera");
    app.add_option("--camera", camera_arg, "Camera name to use");
    app.add_option("--width", size_arg.width, "Capture X pixel size");
    app.add_option("--height", size_arg.height, "Capture Y pixel size");
    app.add_option("--buffers", buffers_arg, "Image buffers to allocate");
    app.add_option("--list_frames", list_frames_arg, "List (and skip) frames");
    app.add_option("--save_frames", save_frames_arg, "Save as frameXXXX.png");
    app.add_option("--fps", fps_arg, "Frame rate to specify");
    app.add_option("--manual_gain", man_gain_arg, "Manual analog gain");
    app.add_option("--manual_shutter", man_shutter_arg, "Manual exposure time");
    app.add_option("--manual_red", man_red_arg, "Manual red gain");
    app.add_option("--manual_blue", man_blue_arg, "Manual blue gain");
    CLI11_PARSE(app, argc, argv);

    libcamera::CameraManager manager;
    manager.start();
    fmt::print("\n=== Cameras (libcamera {}) ===\n", manager.version());
    if (manager.cameras().empty()) {
        fmt::print("*** No cameras available\n");
        return 1;
    }

    std::shared_ptr<libcamera::Camera> camera;
    for (auto const& cam : manager.cameras()) {
        if (!camera && cam->id().find(camera_arg) != std::string::npos)
            camera = cam;
        fmt::print("{} {}\n", (camera == cam) ? "=>" : "  ", cam->id());
    }
    if (!camera) {
        fmt::print("*** No cameras matching \"{}\"\n", camera_arg);
        return 1;
    }

    fmt::print("\n=== Camera properties ({}) ===\n", camera->id());
    for (auto const& [name, value] : controls_text(camera->properties()))
        fmt::print("{} = {}\n", name, value);

    fmt::print("\n--- Formats ---\n");
    for (auto const& [name, role] : {
        std::pair{"Viewfinder", libcamera::Viewfinder},
        std::pair{"StillCapture", libcamera::StillCapture},
        std::pair{"VideoRecording", libcamera::VideoRecording},
        std::pair{"Raw", libcamera::Raw},
    }) {
        auto const config = camera->generateConfiguration({role});
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

    fmt::print("\n--- Available controls ---\n");
    std::map<std::string, std::string> info_text;
    for (auto const& [id, info] : camera->controls())
        info_text[id->name()] = info.toString();
    for (auto const& [name, info] : info_text)
        fmt::print("{}: {}\n", name, info);

    libcamera::StreamConfiguration desired = {};
    desired.size = size_arg;
    desired.pixelFormat = libcamera::PixelFormat::fromString(format_arg);
    desired.bufferCount = buffers_arg;
    fmt::print(
        "\n=== Acquiring and configuring camera ({}) ===\n",
        desired.toString()
    );
    if (!desired.pixelFormat.isValid()) {
        fmt::print("*** Pixel format \"{}\" invalid\n", format_arg);
        return 1;
    }

    if (camera->acquire()) {
        fmt::print("*** Acquisition failed\n");
        return 1;
    }

    auto config = camera->generateConfiguration({});
    config->addConfiguration(desired);
    if (config->validate() == libcamera::CameraConfiguration::Invalid) {
        fmt::print("*** Invalid configuration\n");
        return 1;
    }

    auto const& sconfig = (*config)[0];
    fmt::print(
        "Validated: {:<18} {:>4}kB/f {:>4}B/l {}buf\n", sconfig.toString(),
        sconfig.frameSize / 1024, sconfig.stride, sconfig.bufferCount
    );

    if (save_frames_arg && sconfig.pixelFormat != libcamera::formats::BGR888) {
        fmt::print(
            "*** Bad pixel format ({}) for --save_frames, need BGR888\n",
            sconfig.pixelFormat.toString()
        );
        return 1;
    }

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

    for (auto const& [name, value] : controls_text(camera_controls))
        fmt::print("  {} = {}\n", name, value);

    fmt::print("\n");  // Blank line before configure()'s log messages
    if (camera->configure(config.get())) {
        fmt::print("*** Configuration failed\n");
        return 1;
    }

    auto allocator = std::make_unique<libcamera::FrameBufferAllocator>(camera);
    if (allocator->allocate(sconfig.stream()) < 0) {
        fmt::print("*** Allocation failed\n");
        return 1;
    }

    auto const& buffers = allocator->buffers(sconfig.stream());
    std::vector<std::unique_ptr<libcamera::Request>> requests;
    std::vector<std::shared_ptr<uint8_t const>> mmaps;
    for (auto const& buffer : buffers) {
        auto request = camera->createRequest(requests.size());
        if (request->addBuffer(sconfig.stream(), buffer.get())) {
            fmt::print("*** Buffer association failed\n");
            return 1;
        }

        requests.push_back(std::move(request));

        if (save_frames_arg && buffer->planes().size() > 0) {
            auto const& plane = buffer->planes()[0];
            int const size = plane.length, fd = plane.fd.get();
            void* vmem = ::mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
            if (vmem == MAP_FAILED) {
                fmt::print("*** Buffer mapping failed\n");
                return 1;
            }
            mmaps.emplace_back(
                (uint8_t const*) vmem,
                [size](uint8_t const* m) { ::munmap((void*) m, size); }
            );
        }
    }

    fmt::print("\n=== Starting camera ===\n");
    if (camera->start(&camera_controls)) {
        fmt::print("*** Camera failed to start\n");
        return 1;
    }

    moodycamel::BlockingConcurrentQueue<libcamera::Request*> done_q;
    auto enq = [&done_q](libcamera::Request* r) { done_q.enqueue(r); };
    camera->requestCompleted.connect(&done_q, enq);
    for (auto const& request : requests)
        camera->queueRequest(request.get());

    auto const total_frames = list_frames_arg + save_frames_arg;
    fmt::print("--- Receiving {} frames ---\n", total_frames);
    for (int f = 0; f < total_frames; ++f) {
        libcamera::Request* done;
        done_q.wait_dequeue(done);
        if (done->status() != libcamera::Request::RequestComplete) {
            fmt::print("*** Camera request #{} incomplete\n", done->sequence());
            break;
        }

        auto const* buf = done->findBuffer(sconfig.stream());
        if (buf->metadata().status != libcamera::FrameMetadata::FrameSuccess) {
            fmt::print("*** Frame #{} failed\n", buf->metadata().sequence);
            break;
        }

        if (done->sequence()) fmt::print("\n");
        fmt::print(
            "Req #{:<4} Seq #{:<4} {:.3f}us", done->sequence(),
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

        if (f >= list_frames_arg)
            save_png(sconfig, mmaps[done->cookie()], f - list_frames_arg);

        done->reuse(libcamera::Request::ReuseBuffers);
        camera->queueRequest(done);
    }

    fmt::print("\n=== Stopping and releasing camera ===\n");
    camera->stop();
    camera->release();
    camera.reset();
    allocator.reset();
    manager.stop();
    return 0;
}
