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

executable(
    'picam_test', 'picam_test.cpp',
    dependencies: [deps],
)
