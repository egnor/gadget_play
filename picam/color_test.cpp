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

int main(int argc, char** argv) {
    std::string camera_arg = "/base/soc/i2c0mux/i2c@1/ov5647@36";
    libcamera::Size size_arg = {85, 64};
    float fps_arg = 0.0;
    float gain_arg = 8.0;
    float shutter_arg = 0.03;
    float red_arg = 1.0;
    float blue_arg = 1.5;

    CLI::App app("Exercise libcamera");
    app.add_option("--camera", camera_arg, "Camera ID to use");
    app.add_option("--width", size_arg.width, "Capture X pixel size");
    app.add_option("--height", size_arg.height, "Capture Y pixel size");
    app.add_option("--fps", fps_arg, "Frame rate to specify");
    app.add_option("--gain", man_gain_arg, "Analog gain");
    app.add_option("--shutter", man_shutter_arg, "Exposure time");
    app.add_option("--red", man_red_arg, "Red gain");
    app.add_option("--blue", man_blue_arg, "Blue gain");
    CLI11_PARSE(app, argc, argv);

    libcamera::CameraManager manager;
    manager.start();
    auto camera = manager.get(camera_arg);
    if (!camera) {
        fmt::print("*** Camera not found: {}\n", camera_arg);
        return 1;
    }
    if (camera->acquire()) {
        fmt::print("*** Acquisition failed\n");
        return 1;
    }

    auto const config = camera->generateConfiguration({});
    libcamera::StreamConfiguration desired = {};
    desired.size = size_arg;
    desired.pixelFormat = libcamera::formats::XRGB8888;
    desired.bufferCount = 4;
    config->addConfiguration(desired);
    if (config->validate() == libcamera::CameraConfiguration::Invalid) {
        fmt::print("*** Invalid configuration: {}\n", desired.toString());
        return 1;
    }
    if (camera->configure(config.get())) {
        fmt::print("*** Configuration failed\n");
        return 1;
    }

    auto const& sconfig = (*config)[0];
    fmt::print("Configured: {}\n", sconfig.toString());

    auto allocator = std::make_unique<libcamera::FrameBufferAllocator>(camera);
    if (allocator->allocate(sconfig.stream()) < 0) {
        fmt::print("*** Allocation failed\n");
        return 1;
    }

    int64_t const fmicros = std::round(1e6 / fps_arg);
    int const smicros = std::round(shutter_arg * 1e6f);
    libcamera::ControlList controls{libcamera::controls::controls};
    controls.set(libcamera::controls::FrameDurationLimits, {fmicros, fmicros});
    controls.set(libcamera::controls::AeEnable, false);
    controls.set(libcamera::controls::AnalogueGain, gain_arg);
    controls.set(libcamera::controls::DigitalGain, 1.0);
    controls.set(libcamera::controls::ExposureTime, smicros);
    controls.set(libcamera::controls::AwbEnable, false);
    controls.set(libcamera::controls::ColourGains, {red_arg, blue_arg});

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

    if (camera->start(&camera_controls)) {
        fmt::print("*** Camera start failed\n");
        return 1;
    }

    moodycamel::BlockingConcurrentQueue<libcamera::Request*> done_q;
    auto enq = [&done_q](libcamera::Request* r) { done_q.enqueue(r); };
    camera->requestCompleted.connect(&done_q, enq);
    for (auto const& request : requests)
        camera->queueRequest(request.get());

    fmt::print("\n=== Receiving frames ===\n");
    auto const total_frames = list_frames_arg + save_frames_arg;
    for (;;) {
        libcamera::Request* done;
        done_q.wait_dequeue(done);
        if (done->status() != libcamera::Request::RequestComplete) {
            fmt::print("*** Camera request #{} incomplete\n", done->sequence());
            break;
        }

        save_png(sconfig, mmaps[done->cookie()], f - list_frames_arg);

        done->reuse(libcamera::Request::ReuseBuffers);
        camera->queueRequest(done);
    }
}
