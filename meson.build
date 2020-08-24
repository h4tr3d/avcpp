project(
    'avcpp_setup',
    'cpp',
    meson_version: '>= 0.50.0',
    default_options : [
        'c_std=c11', 
        'cpp_std=c++17'
    ],
    version: '2.0.99',
)

if ! meson.is_subproject()

    subproject('avcpp')

    if get_option('build_samples')
        subproject('avcpp_samples')
    endif

    if get_option('build_tests')
        subproject('avcpp_tests')
    endif
else
    subdir('src')
endif

meson.override_dependency('avcpp', avcpp_dep)
