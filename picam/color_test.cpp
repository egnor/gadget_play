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

#include "checks.h"
#include "save_png.h"

struct HSI { uint16_t h; uint8_t s, i; };

struct RGB { uint8_t r, g, b; };

struct ColorOptions {
    int min_s = 128;
    int min_i = 8;
    int h_step = 10;
    int h_match = 20;
    int h_keepout = 20;
    double on_f = 0.1;
    double off_f = 0.05;
};

std::vector<std::vector<HSI>> hsi_from_buffer(
    libcamera::StreamConfiguration const& sconfig, uint8_t const* mem
) {
    std::vector<std::vector<HSI>> hsi_rows;
    hsi_rows.reserve(sconfig.size.height);
    for (size_t y = 0 ; y < sconfig.size.height; ++y) {
        uint8_t const* rgb_row = mem + y * sconfig.stride;
        auto& hsi_row = hsi_rows.emplace_back();
        hsi_row.reserve(sconfig.size.width);
        for (size_t x = 0; x < sconfig.size.width; ++x) {
            auto const r = rgb_row[x * 3 + 0];
            auto const g = rgb_row[x * 3 + 1];
            auto const b = rgb_row[x * 3 + 2];

            // https://en.wikipedia.org/wiki/HSL_and_HSV#Hue_and_chroma
            auto const m = std::min({r, g, b});
            auto const M = std::max({r, g, b});
            if (m == M) {
                hsi_row.emplace_back(0, 0, m);
            } else {
                auto i = (r + g + b) / 3;
                auto s = i ? 255 - (255 * m / i) : 0;
                auto h =
                    (b == M) ? 240 + 60 * (r - g) / (M - m) :
                    (g == M) ? 120 + 60 * (b - r) / (M - m) :
                    (360 + 60 * (g - b) / (M - m)) % 360;
                hsi_row.emplace_back(h, s, i);
            }
        }
    }

    return hsi_rows;
}

std::vector<double> spectrum_from_hsi(
    std::vector<std::vector<HSI>> const& rows, ColorOptions const& args
) {
    double const pixel_fraction = 1.0 / (rows.size() * rows[0].size());
    const int bins = (360 + args.h_step - 1) / args.h_step;
    std::vector<double> spectrum(bins, 0);
    for (auto const& row : rows) {
        for (auto const& [h, s, i] : row) {
            if (s < args.min_s) continue;
            if (i < args.min_i) continue;
            for (
                 int c = (h + 360 - args.h_match) / args.h_step;
                 c <= (h + 360 + args.h_match) / args.h_step;
                 ++c
            ) {
                 spectrum[c % bins] += pixel_fraction;
            }
        }
    }
    return spectrum;
}

std::vector<bool> update_active(
    std::vector<bool> const& old_active, std::vector<double> const& spectrum,
    ColorOptions const& args
) {
    std::vector<bool> active;
    active.resize(spectrum.size(), false);
    for (size_t c = 0; c < spectrum.size(); ++c) {
        if (spectrum[c] <= args.off_f) {
            active[c] = false;
        } else if (c < old_active.size() && old_active[c]) {
            active[c] = true;
        } else if (spectrum[c] < args.on_f) {
            active[c] = false;
        } else {
            active[c] = true;
            int const hue = c * args.h_step;
            for (
                int nc = (hue + 360 - args.h_keepout) / args.h_step;
                nc <= (hue + 360 + args.h_keepout) / args.h_step;
                ++nc
            ) {
                size_t const nc_wrap = nc % spectrum.size();
                if (
                    (nc_wrap < old_active.size() && old_active[nc_wrap]) ||
                    spectrum[nc_wrap] > spectrum[c] ||
                    (spectrum[nc_wrap] == spectrum[c] && c > nc_wrap)
                ) {
                    active[c] = false;
                    break;
                }
            }
        }
    }
    return active;
}

void print_data(
    std::vector<std::vector<HSI>> const& hsi_rows,
    std::vector<double> const& spectrum,
    ColorOptions const& args
) {
    std::map<int, int> h_hist, s_hist, i_hist;
    for (auto const& hsi_row : hsi_rows) {
        for (auto const& [h, s, i] : hsi_row) {
            ++s_hist[s / 4];
            ++i_hist[i / 4];
            if (s >= args.min_s && i >= args.min_i) ++h_hist[h / 6];
        }
    }

    for (auto const& [name, hist, limit, group] : {
        std::tuple{"H", h_hist, 60, 10},
        std::tuple{"S", s_hist, 64, 8},
        std::tuple{"I", i_hist, 64, 8},
    }) {
        int max = 0;
        for (auto const& [v, n] : hist) max = std::max(max, n);

        fmt::print("  {} [", name);
        for (int v = 0; v < limit; ++v) {
            static char const* const blocks[] = {
                " ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"
            };

            if (v && (v % group == 0)) fmt::print(":");
            auto const it = hist.find(v);
            auto const n = (it == hist.end()) ? 0 : it->second;
            fmt::print("{}", blocks[n * 7 / max]);
        }
        fmt::print("]\n");
    }

    fmt::print("  S");
    for (size_t c = 0; c < spectrum.size(); ++c) {
        if (spectrum[c] >= args.on_f) {
            int const hue = c * args.h_step;
            fmt::print(" {}:{:.3f}", hue, spectrum[c]);
        }
    }
    fmt::print("\n");
}

RGB rgb_from_hsi(HSI const& hsi) {
    auto const& [h_360, s_255, i_255] = hsi;

    // https://en.wikipedia.org/wiki/HSL_and_HSV#HSI_to_RGB
    auto const Z_60 = 60 - std::abs(h_360 % 120 - 60);
    auto const C_255 = 3 * i_255 * s_255 * 60 / 255 / (60 + Z_60);
    auto const X_255 = C_255 * Z_60 / 60;

    std::array<int, 3> rgb1_255;
    switch (h_360 / 60) {
        case 0: rgb1_255 = {C_255, X_255, 0}; break;
        case 1: rgb1_255 = {X_255, C_255, 0}; break;
        case 2: rgb1_255 = {0, C_255, X_255}; break;
        case 3: rgb1_255 = {0, X_255, C_255}; break;
        case 4: rgb1_255 = {X_255, 0, C_255}; break;
        default: rgb1_255 = {C_255, 0, X_255}; break;
    }

    auto const m_255 = i_255 * (255 - s_255) / 255;
    return {
        uint8_t(std::min(rgb1_255[0] + m_255, 255)),
        uint8_t(std::min(rgb1_255[1] + m_255, 255)),
        uint8_t(std::min(rgb1_255[1] + m_255, 255)),
    };
}

void save_color_image(
    int width, int height, int frame_stride, uint8_t const* frame_mem,
    std::vector<std::vector<HSI>> const& hsi_rows,
    std::vector<bool> const& active, ColorOptions const& args,
    std::string const& fn
) {
    int const out_stride = width * 2 * 3;
    std::vector<uint8_t> out(hsi_rows.size() * out_stride, 0);
    for (int y = 0; y < height; ++y) {
        auto const& hsi_row = hsi_rows[y];
        auto* const frame_row = out.data() + y * out_stride;
        memcpy(frame_row, frame_mem + y * frame_stride, width * 3);

        auto* const color_row = frame_row + width * 3;
        for (int x = 0; x < width; ++x) {
            auto [h, s, i] = hsi_row[x];
            h = h / args.h_step * args.h_step;
            if (i < args.min_i) {
                s = i = 0;    // Too dim: turn black
            } else if (s < args.min_s) {
                s = 0;        // Too unsaturated: gray out (keep H, I)
            } else {
                s = i = 255;  // Usable: max S and I to emphasize H
            }

            auto const rgb = rgb_from_hsi({h, s, i});
            color_row[x * 3 + 0] = rgb.r;
            color_row[x * 3 + 1] = rgb.g;
            color_row[x * 3 + 2] = rgb.b;
        }
    }

    int active_count = 0;
    int const square_size = std::min(width / 8, height / 8);
    for (size_t c = 0; c < active.size(); ++c) {
        if (!active[c]) continue;
        auto const rgb = rgb_from_hsi({uint16_t(c * args.h_step), 255, 255});
        for (int y = 0; y < square_size; ++y) {
            auto* const out_row = out.data() + y * out_stride +
                (width + (square_size - 1) * active_count) * 3;
            for (int x = 0; x < square_size; ++x) {
                auto color = rgb;
                if (x == 0 || x == square_size - 1) color = {255, 255, 255};
                if (y == 0 || y == square_size - 1) color = {255, 255, 255};
                out_row[x * 3 + 0] = color.r;
                out_row[x * 3 + 1] = color.g;
                out_row[x * 3 + 2] = color.b;
            }
        }
        ++active_count;
    }

    save_png(width * 2, height, out_stride, out.data(), fn);
}

int main(int argc, char** argv) {
    std::string camera_arg = "/base/soc/i2c0mux/i2c@1/imx219@10";
    libcamera::Size size_arg = {85, 64};
    float fps_arg = 120.0;
    float gain_arg = 8.0;
    float shutter_arg = 1.0 / 120.0;
    float red_arg = 1.2;
    float blue_arg = 2.5;
    ColorOptions color_args = {};
    bool print_data_arg = false;
    bool save_color_arg = false;

    CLI::App app("Chroma detection test");
    app.add_option("--camera", camera_arg, "Camera ID to use");
    app.add_option("--width", size_arg.width, "Capture X pixel size");
    app.add_option("--height", size_arg.height, "Capture Y pixel size");
    app.add_option("--fps", fps_arg, "Frame rate to specify");
    app.add_option("--gain", gain_arg, "Analog gain");
    app.add_option("--shutter", shutter_arg, "Exposure time");
    app.add_option("--red", red_arg, "Red gain");
    app.add_option("--blue", blue_arg, "Blue gain");
    app.add_option("--min_saturation", color_args.min_s, "Threshold 0-255");
    app.add_option("--min_intensity", color_args.min_i, "Threshold 0-255");
    app.add_option("--hue_step", color_args.h_step, "Precision in degrees");
    app.add_option("--hue_match", color_args.h_match, "Color cluster radius");
    app.add_option("--hue_keepout", color_args.h_keepout, "Detection spacing");
    app.add_option("--on_fraction", color_args.on_f, "Turn-on cover");
    app.add_option("--off_fraction", color_args.off_f, "Turn-off cover");
    app.add_flag("--print_data", print_data_arg, "Print regular histograms");
    app.add_flag("--save_color", save_color_arg, "Save changes color####.png");
    CLI11_PARSE(app, argc, argv);

    libcamera::CameraManager manager;
    manager.start();
    auto camera = manager.get(camera_arg);
    CHECK_RUNTIME(camera, "Camera not found: {}", camera_arg);
    CHECK_RUNTIME(!camera->acquire(), "Acquire failed: {}", camera->id());

    auto config = camera->generateConfiguration({libcamera::VideoRecording});
    CHECK_RUNTIME(!config->empty(), "No stream for VideoRecording role");

    config->transform = libcamera::transformFromRotation(
        camera->properties().get(libcamera::properties::Rotation)
    );

    auto* sconfig = &(*config)[0];
    sconfig->size = size_arg;
    sconfig->pixelFormat = libcamera::formats::BGR888;
    sconfig->bufferCount = 4;
    CHECK_RUNTIME(
        config->validate() != libcamera::CameraConfiguration::Invalid,
        "Invalid configuration: {}", sconfig->toString()
    );
    CHECK_RUNTIME(
        sconfig->pixelFormat == libcamera::formats::BGR888,
        "Bad pixel format: {}", sconfig->pixelFormat.toString()
    );
    CHECK_RUNTIME(!camera->configure(config.get()), "Configuration failed");
    fmt::print("Configured: {}\n", sconfig->toString());

    auto* stream = sconfig->stream();
    CHECK_RUNTIME(stream, "No stream after configuration");

    auto allocator = std::make_unique<libcamera::FrameBufferAllocator>(camera);
    CHECK_RUNTIME(allocator->allocate(stream) > 0, "Buffer alloc failed");

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

    auto const& buffers = allocator->buffers(stream);
    std::vector<std::unique_ptr<libcamera::Request>> requests;
    std::vector<std::shared_ptr<uint8_t const>> mmaps;
    for (auto const& buffer : buffers) {
        auto request = camera->createRequest(requests.size());
        CHECK_RUNTIME(!request->addBuffer(stream, buffer.get()), "Add failed");
        requests.push_back(std::move(request));

        auto const& plane = buffer->planes()[0];
        int const size = plane.length, fd = plane.fd.get();
        void* vmem = ::mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
        CHECK_RUNTIME(vmem != MAP_FAILED, "Buffer mapping failed");
        mmaps.emplace_back(
            (uint8_t const*) vmem,
            [size](uint8_t const* m) { ::munmap((void*) m, size); }
        );
    }

    CHECK_RUNTIME(!camera->start(&controls), "Camera start failed");
    moodycamel::BlockingConcurrentQueue<libcamera::Request*> done_q;
    auto enq = [&done_q](libcamera::Request* r) { done_q.enqueue(r); };
    camera->requestCompleted.connect(&done_q, enq);
    for (auto const& request : requests)
        camera->queueRequest(request.get());

    std::vector<bool> active;
    int image_count = 0;
    uint64_t next_data_nanos = 0;
    fmt::print("\n=== Receiving frames ===\n");
    for (;;) {
        libcamera::Request* done;
        done_q.wait_dequeue(done);
        CHECK_RUNTIME(
            done->status() == libcamera::Request::RequestComplete,
            "Request #{} incomplete", done->sequence()
        );

        auto const* buf = done->findBuffer(stream);
        auto const nanos = buf->metadata().timestamp;
        CHECK_RUNTIME(
            buf->metadata().status == libcamera::FrameMetadata::FrameSuccess,
            "Frame #{} failed", buf->metadata().sequence
        );

        uint8_t const* mem = mmaps[done->cookie()].get();
        auto const hsi_rows = hsi_from_buffer(*sconfig, mem);
        auto const spectrum = spectrum_from_hsi(hsi_rows, color_args);
        auto const new_active = update_active(active, spectrum, color_args);
        if (new_active != active) {
            active = std::move(new_active);
            fmt::print("{:.3f}s ", nanos * 1e-9);
            for (auto f : active) fmt::print("{}", f ? "#" : ".");
            if (save_color_arg) {
                auto const fn = fmt::format("color{:04d}.png", image_count++);
                save_color_image(
                    sconfig->size.width, sconfig->size.height, sconfig->stride,
                    mem, hsi_rows, active, color_args, fn
                );
                fmt::print(" {}", fn);
            }
            fmt::print("\n");
        }

        if (print_data_arg && nanos >= next_data_nanos) {
            fmt::print("{:.3f}s\n", nanos * 1e-9);
            print_data(hsi_rows, spectrum, color_args);
            next_data_nanos += (next_data_nanos ? 0 : nanos) + 500000000;
        }

        done->reuse(libcamera::Request::ReuseBuffers);
        camera->queueRequest(done);
    }
}
