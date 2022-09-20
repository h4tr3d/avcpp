project(
    'avcpp',
    'cpp',
    meson_version: '>= 0.50.0',
    default_options : [
        'c_std=c11', 
        'cpp_std=c++17'
    ],
    version: '2.1.0',
)

if not meson.is_subproject()
    subdir('src')

    if get_option('build_samples')
        subdir('example/api2-samples')
    endif

    if get_option('build_tests')
        subdir('tests')
    endif
else
    subdir('src')
endif

if meson.version().version_compare('>=0.54.0')
    meson.override_dependency('avcpp', avcpp_dep)
endif
