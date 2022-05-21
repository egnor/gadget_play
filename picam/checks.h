// Error-reporting utilities

#pragma once

#include <stdexcept>

#include <fmt/core.h>

// Throws std::logic_error if the condition is false
#define CHECK_LOGIC(f) \
    [&]{ if (!(f)) throw std::logic_error("LOGIC fail (" FILELINE ") " #f); }()

// Throws std::invalid_argument if the condition is false, using fmt args
#define CHECK_ARG(f, ...) \
    [&]{ if (!(f)) throw std::invalid_argument(fmt::format(__VA_ARGS__)); }()

// Throws std::runtime_error if the condition is false, using fmt args
#define CHECK_RUNTIME(f, ...) \
    [&]{ if (!(f)) throw std::runtime_error(fmt::format(__VA_ARGS__)); }()

// File and line number used by CHECK_LOGIC
#define FILELINE __FILE__ ":" STRINGIFY(__LINE__)
#define STRINGIFY(x) STRINGIFY2(x)
#define STRINGIFY2(x) #x
