project('picam', ['c', 'cpp'], version: '0.0',
    default_options: [
        'cpp_std=c++2a',
        'warning_level=3',
        'werror=true',
    ]
)

add_project_arguments('-Wno-pedantic', '-Wno-psabi', language: ['c', 'cpp'])

deps = [
    dependency('concurrentqueue'),
    dependency('fmt'),
    dependency('libcamera'),
    dependency('libpng'),
]

picam_lib = library(
    'picam', ['save_png.cpp'],
    dependencies: [deps],
)

executable(
    'picam_test', 'picam_test.cpp',
    link_with: [picam_lib],
    dependencies: [deps],
)

executable(
    'color_test', 'color_test.cpp',
    link_with: [picam_lib],
    dependencies: [deps],
)
