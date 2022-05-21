#include <glob.h>
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

struct HCV { int16_t h; uint8_t c, l; };

struct RGB { uint8_t r, g, b; };

struct ColorOptions {
    int min_c = 96;
    int h_step = 5;
    int h_merge = 10;
    int h_keepout = 20;
    double on_f = 0.1;
    double off_f = 0.05;
};

HCV to_hcv(RGB rgb) {
    // https://en.wikipedia.org/wiki/HSL_and_HSV#Hue_and_chroma
    auto const [r, g, b] = rgb;
    auto const m = std::min({r, g, b});
    auto const M = std::max({r, g, b});
    if (m == M) {
        return {0, 0, m};
    } else {
        auto h =
            (b == M) ? 240 + 60 * (r - g) / (M - m) :
            (g == M) ? 120 + 60 * (b - r) / (M - m) :
            (360 + 60 * (g - b) / (M - m)) % 360;

        CHECK_LOGIC(h >= 0 && h < 360);
        CHECK_LOGIC(M - m >= 0 && M - m < 256);
        CHECK_LOGIC(M >= 0 && M < 256);
        return {int16_t(h), uint8_t(M - m), uint8_t(M)};
    }
}

RGB to_rgb(HCV const& hcv) {
    auto const& [h, c, v] = hcv;
    CHECK_ARG(h >= 0 && h < 360, "Bad hue: {}", h);
    CHECK_ARG(c >= 0 && c < 256, "Bad chroma: {}", c);
    CHECK_ARG(v >= 0 && v < 256, "Bad value: {}", c);

    // https://en.wikipedia.org/wiki/HSL_and_HSV#HCV_to_RGB
    auto const Z = 60 - std::abs(h % 120 - 60);
    CHECK_LOGIC(Z >= 0 && Z <= 60);
    uint8_t const X = c * Z / 60;

    RGB rgb1;
    switch (h / 60) {
        case 0: rgb1 = {c, X, 0}; break;
        case 1: rgb1 = {X, c, 0}; break;
        case 2: rgb1 = {0, c, X}; break;
        case 3: rgb1 = {0, X, c}; break;
        case 4: rgb1 = {X, 0, c}; break;
        default: rgb1 = {c, 0, X}; break;
    }

    auto const m = std::max(0, v - std::max(c, X));
    auto const r = rgb1.r + m, g = rgb1.g + m, b = rgb1.b + m;
    CHECK_LOGIC(r >= 0 && r < 256);
    CHECK_LOGIC(g >= 0 && g < 256);
    CHECK_LOGIC(b >= 0 && b < 256);
    return {uint8_t(r), uint8_t(g), uint8_t(b)};
}

uint8_t to_bucket(int hue, ColorOptions const& args) {
    if (hue < 0) hue = 360 - (-hue % 360);
    return ((hue + args.h_step / 2) % 360) / args.h_step;
}

int16_t to_hue(int b, ColorOptions const& args) {
    CHECK_ARG(b >= 0 && b * args.h_step < 360, "Bad hue bucket: {}", b);
    return b * args.h_step;
}

std::vector<double> make_spectrum(
    std::vector<std::vector<HCV>> const& rows, ColorOptions const& args
) {
    double const pixel_fraction = 1.0 / (rows.size() * rows[0].size());
    std::vector<double> spectrum((359 / args.h_step) + 1, 0);
    for (auto const& row : rows) {
        for (auto const& [h, c, v] : row) {
            if (c < args.min_c) continue;
            for (
                 auto b = to_bucket(h - args.h_merge, args);
                 b != to_bucket(h + args.h_merge + args.h_step, args);
                 b = (b + 1) % spectrum.size()
            ) {
                 spectrum[b] += pixel_fraction;
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
    for (size_t b = 0; b < spectrum.size(); ++b) {
        if (spectrum[b] <= args.off_f) {
            active[b] = false;
        } else if (b < old_active.size() && old_active[b]) {
            active[b] = true;
        } else if (spectrum[b] < args.on_f) {
            active[b] = false;
        } else {
            active[b] = true;
            auto const hue = to_hue(b, args);
            for (
                auto nb = to_bucket(hue - args.h_keepout, args);
                nb != to_bucket(hue + args.h_keepout + args.h_step, args);
                nb = (nb + 1) % active.size()
            ) {
                if (
                    (nb < old_active.size() && old_active[nb]) ||
                    spectrum[nb] > spectrum[b] ||
                    (spectrum[nb] == spectrum[b] && b > nb)
                ) {
                    active[b] = false;
                    break;
                }
            }
        }
    }
    return active;
}

void print_data(
    std::vector<std::vector<HCV>> const& hcv_rows,
    std::vector<double> const& spectrum, std::vector<bool> const& active,
    ColorOptions const& args
) {
    std::map<int, int> h_hist, c_hist, v_hist;
    for (auto const& hcv_row : hcv_rows) {
        for (auto const& [h, c, v] : hcv_row) {
            ++c_hist[c / 4];
            ++v_hist[v / 4];
            if (c >= args.min_c) ++h_hist[h / 6];
        }
    }

    for (auto const& [name, hist, limit, group] : {
        std::tuple{"H", h_hist, 60, 10},
        std::tuple{"C", c_hist, 64, 8},
        std::tuple{"V", v_hist, 64, 8},
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
            fmt::print("{}", max ? blocks[n * 7 / max] : "-");
        }
        fmt::print("]\n");
    }

    fmt::print("  B");
    for (size_t b = 0; b < spectrum.size(); ++b) {
        if (spectrum[b] >= args.on_f) {
            fmt::print(
                " {}{}{:.3f}",
                to_hue(b, args), active[b] ? '#' : ':', spectrum[b]
            );
        }
    }
    fmt::print("\n");
}

void save_color_image(
    int width, int height, int frame_stride, uint8_t const* frame_mem,
    std::vector<std::vector<HCV>> const& hcv_rows,
    std::vector<bool> const& active, ColorOptions const& args,
    std::string const& fn
) {
    int const out_stride = width * 2 * 3;
    std::vector<uint8_t> out(hcv_rows.size() * out_stride, 0);
    for (int y = 0; y < height; ++y) {
        auto const& hcv_row = hcv_rows[y];
        auto* const frame_row = out.data() + y * out_stride;
        memcpy(frame_row, frame_mem + y * frame_stride, width * 3);

        auto* const color_row = frame_row + width * 3;
        for (int x = 0; x < width; ++x) {
            auto [h, c, v] = hcv_row[x];
            h = to_hue(to_bucket(h, args), args);
            if (c < args.min_c) {
                c = 0;        // Too unsaturated: gray out (keep H, V)
            } else {
                c = v = 255;  // Max C and V to emphasize H
            }

            auto const rgb = to_rgb({h, c, v});
            color_row[x * 3 + 0] = rgb.r;
            color_row[x * 3 + 1] = rgb.g;
            color_row[x * 3 + 2] = rgb.b;
        }
    }

    int active_count = 0;
    int const square_size = std::min(width / 8, height / 8);
    for (size_t b = 0; b < active.size(); ++b) {
        if (!active[b]) continue;
        auto const rgb = to_rgb({to_hue(b, args), 255, 255});
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
    app.add_option("--min_chroma", color_args.min_c, "Threshold 0-255");
    app.add_option("--hue_step", color_args.h_step, "Precision in degrees");
    app.add_option("--hue_merge", color_args.h_merge, "Color cluster radius");
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

    glob_t files = {};
    if (save_color_arg && !glob("color????.png", 0, nullptr, &files)) {
        fmt::print("Removing {} color????.png files...\n", files.gl_pathc);
        for (size_t i = 0; i < files.gl_pathc; ++i) {
            CHECK_RUNTIME(
                !remove(files.gl_pathv[i]),
                "Remove failed: {}", files.gl_pathv[i]
            );
        }
    }

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
        std::vector<std::vector<HCV>> hcv_rows;
        hcv_rows.reserve(sconfig->size.height);
        for (size_t y = 0 ; y < sconfig->size.height; ++y) {
            auto& row = hcv_rows.emplace_back();
            row.reserve(sconfig->size.width);
            for (size_t x = 0; x < sconfig->size.width; ++x) {
                auto const* pix = mem + y * sconfig->stride + x * 3;
                row.push_back(to_hcv({pix[0], pix[1], pix[2]}));
            }
        }

        auto const spectrum = make_spectrum(hcv_rows, color_args);
        auto const new_active = update_active(active, spectrum, color_args);
        if (new_active != active) {
            active = std::move(new_active);
            fmt::print("{:.3f}s ", nanos * 1e-9);
            for (auto f : active) fmt::print("{}", f ? "#" : ".");
            if (save_color_arg) {
                auto const fn = fmt::format("color{:04d}.png", image_count++);
                save_color_image(
                    sconfig->size.width, sconfig->size.height, sconfig->stride,
                    mem, hcv_rows, active, color_args, fn
                );
                fmt::print(" {}", fn);
            }
            fmt::print("\n");
        }

        if (print_data_arg && nanos >= next_data_nanos) {
            fmt::print("{:.3f}s\n", nanos * 1e-9);
            print_data(hcv_rows, spectrum, active, color_args);
            next_data_nanos += (next_data_nanos ? 0 : nanos) + 500000000;
        }

        done->reuse(libcamera::Request::ReuseBuffers);
        camera->queueRequest(done);
    }
}
