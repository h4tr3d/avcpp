project(
    'avcpp',
    'cpp',
    meson_version: '>= 0.54.1',
    default_options : [
        'c_std=c11', 
        'cpp_std=c++20'
    ],
    version: files('VERSION.txt'),
)

subdir('src/avcpp')
if not meson.is_subproject()

    if get_option('build_samples')
        subdir('example/api2-samples')
    endif

    if get_option('build_tests')
        subdir('tests')
    endif
endif

meson.override_dependency('avcpp', avcpp_dep)
